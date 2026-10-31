#include "catpkg/builder.h"
#include "catpkg/pkginfo.h"
#include "catpkg/command.h"
#include "utils/fs.h"
#include "catpkg/path.h"
#include "catpkg/ctpg.h"
#include "configuration.h"
#include "verify.h"
#include "database.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/statvfs.h>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>

extern char **database_package_ids;
extern size_t database_package_ids_size;

/*
    ===================================================
    #define BUILDER_DEBUG_APPLY
    
    Tracks and outputs information about the actions 
    of the APPLY script function to descriptor 20.
    ===================================================
*/

#define CATPKG_DEBUG

#ifdef CATPKG_DEBUG

#define CATPKG_STRINGIFY_(x) #x
#define CATPKG_STRINGIFY(x) CATPKG_STRINGIFY_(x)

#define CATPKG_ERROR \
    "ERROR_" CATPKG_STRINGIFY(__LINE__)

#else

#define CATPKG_ERROR \
    "ERROR"

#endif

#define CATPKG_BUILD_LOG_TREE(revert) \
    do { \
        catpkg_build_log( \
            "%s── ", \
            builder, \
            false, \
            ((revert \
                ? builder->applied_count - 1 <= 0 \
                : builder->applied_count >= builder->requests_size - 1) \
                    ? "└" \
                    : "├") \
        ); \
        build_log_line_started = true; \
    } while (0)

#define CATPKG_BUILD_LOG_ARGS(operation, path) \
    operation, path

#define CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination) \
    operation, source, destination

#define CATPKG_BUILD_LOG_CTPG_ARGS(name, vesrion) \
    name, vesrion

#define CATPKG_BUILD_LOG(format, result, revert, args) \
    catpkg_build_log( \
        format "%s%s\n", \
        builder, \
        true, \
        args, \
        (result)[0] != '\0' ? \
            ((revert ? builder->applied_count - 1 <= 0 : \
                       builder->applied_count >= builder->requests_size - 1) ? \
                "\n \t└── " : "\n│\t└── ") : \
            "", \
        (result)[0] != '\0' ? result : "")

#define CATPKG_BUILD_LOG_MOVE_ERROR(operation, source, destination) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\" -> \"%s\"", \
        CATPKG_ERROR, \
        0, \
        CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination))

#define CATPKG_BUILD_LOG_MOVE_OK(operation, source, destination) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\" -> \"%s\"", \
        "ok", \
        0, \
        CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination))

#define CATPKG_BUILD_LOG_PATH_OK(operation, path) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\"", \
        "ok", \
        0, \
        CATPKG_BUILD_LOG_ARGS(operation, path))

#define CATPKG_BUILD_LOG_PATH_ERROR(operation, path) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\"", \
        CATPKG_ERROR, \
        0, \
        CATPKG_BUILD_LOG_ARGS(operation, path))



/*
 *	Service state
 */

static bool build_log_active = 0;
static bool build_log_line_started = false;
static struct CtpgContext *ctpg_context = NULL;
static char *progress_info = NULL;
static size_t progress_info_height = 0;

#define PROGRESS_HEIGHT 10

static bool progress_drawn = false;

#define DEFAULT_CONSOLE_WIDTH 80

static size_t console_width_dsr(void)
{
    int tty = open(
        "/dev/tty",
        O_RDWR | O_NOCTTY | O_NONBLOCK
    );

    if (tty == -1)
        return 0;

    struct termios old_termios;
    struct termios raw_termios;

    if (tcgetattr(tty, &old_termios) == -1) {
        close(tty);
        return 0;
    }

    raw_termios = old_termios;

    raw_termios.c_lflag &= ~(ICANON | ECHO);
    raw_termios.c_cc[VMIN] = 0;
    raw_termios.c_cc[VTIME] = 0;

    if (tcsetattr(tty, TCSANOW, &raw_termios) == -1) {
        close(tty);
        return 0;
    }

    size_t width = 0;

    if (write(tty, "\033[s\033[999C\033[6n", 14) != 14)
        goto cleanup;

    struct pollfd pfd = {
        .fd = tty,
        .events = POLLIN
    };

    if (poll(&pfd, 1, 100) <= 0)
        goto cleanup;

    char buffer[64];
    ssize_t length = read(
        tty,
        buffer,
        sizeof(buffer) - 1
    );

    if (length <= 0)
        goto cleanup;

    buffer[length] = '\0';

    unsigned int row;
    unsigned int column;

    if (
        sscanf(
            buffer,
            "\033[%u;%uR",
            &row,
            &column
        ) == 2 &&
        column > 0
    )
        width = column;

cleanup:
    write(tty, "\033[u", 3);

    tcsetattr(
        tty,
        TCSANOW,
        &old_termios
    );

    close(tty);

    return width;
}

static size_t console_width(int fd)
{
    struct winsize ws = {0};

    if (
        ioctl(fd, TIOCGWINSZ, &ws) == 0 &&
        ws.ws_col > 0
    )
        return ws.ws_col;

    const char *columns = getenv("COLUMNS");

    if (columns != NULL) {
        char *end = NULL;

        unsigned long width = strtoul(
            columns,
            &end,
            10
        );

        if (
            end != columns &&
            *end == '\0' &&
            width > 0
        )
            return (size_t)width;
    }

    size_t width = console_width_dsr();

    if (width > 0)
        return width;

    return DEFAULT_CONSOLE_WIDTH;
}

static int stdout_buffering_start(int memfd, int stdout_fd)
{
    if (stdout_fd == -1)
        return -1;

    if (fflush(stdout) == EOF) {
        close(stdout_fd);
        return -1;
    }

    if (dup2(memfd, STDOUT_FILENO) == -1) {
        close(stdout_fd);
        return -1;
    }
    return 0;
}

static int stdout_buffering_flush(
    int memfd,
    int stdout_fd
)
{
    if (fflush(stdout) == EOF) {
        perror("stdout_buffering_flush: fflush");
        return 1;
    }

    off_t size = lseek(memfd, 0, SEEK_END);

    if (size == (off_t)-1) {
        perror("stdout_buffering_flush: lseek SEEK_END");
        return 1;
    }

    off_t offset = 0;

    while (offset < size) {
        ssize_t n = sendfile(
            stdout_fd,
            memfd,
            &offset,
            (size_t)(size - offset)
        );

        if (n == -1) {
            if (errno == EINTR)
                continue;

            perror("stdout_buffering_flush: sendfile");
            return 1;
        }

        if (n == 0) {
            fprintf(
                stderr,
                "stdout_buffering_flush: sendfile returned 0 "
                "(offset=%jd, size=%jd)\n",
                (intmax_t)offset,
                (intmax_t)size
            );

            return 1;
        }
    }

    if (ftruncate(memfd, 0) == -1) {
        perror("stdout_buffering_flush: ftruncate");
        return 1;
    }

    if (lseek(memfd, 0, SEEK_SET) == (off_t)-1) {
        perror("stdout_buffering_flush: lseek SEEK_SET");
        return 1;
    }

    if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
        close(stdout_fd);
        return -1;
    }

    return 0;
}

static int stdout_buffering_print(
    int memfd,
    int stdout_fd
)
{
    if (fflush(stdout) == EOF) {
        perror("stdout_buffering_flush: fflush");
        return 1;
    }

    off_t size = lseek(memfd, 0, SEEK_END);

    if (size == (off_t)-1) {
        perror("stdout_buffering_flush: lseek SEEK_END");
        return 1;
    }

    off_t offset = 0;

    while (offset < size) {
        ssize_t n = sendfile(
            stdout_fd,
            memfd,
            &offset,
            (size_t)(size - offset)
        );

        if (n == -1) {
            if (errno == EINTR)
                continue;

            perror("stdout_buffering_flush: sendfile");
            return 1;
        }

        if (n == 0) {
            fprintf(
                stderr,
                "stdout_buffering_flush: sendfile returned 0 "
                "(offset=%jd, size=%jd)\n",
                (intmax_t)offset,
                (intmax_t)size
            );

            return 1;
        }
    }

    return 0;
}

/*
 * ============================================================
 * Progress
 * ============================================================
 */

static void builder_clear_progress(int stdout_fd)
{
    if (!progress_drawn)
        return;

    for (size_t i = 0; i < PROGRESS_HEIGHT; i++)
        dprintf(stdout_fd, "\033[1A\r\033[2K");

    progress_drawn = false;
}

static void builder_progress_bar(
    size_t current,
    size_t total,
    int stdout_fd
)
{
    size_t width = console_width(stdout_fd);

    builder_clear_progress(stdout_fd);


    if (width < 10)
        return;

    if (current > total)
        current = total;

    double progress = total > 0
        ? (double)current / (double)total
        : 1.0;

    size_t percent =
        (size_t)(progress * 100.0);

    size_t bar_width =
        width - 7;

    size_t filled =
        (size_t)(progress * (double)bar_width);

    size_t progress_height =
        3 + progress_info_height;

    if (progress_height < PROGRESS_HEIGHT) {
        for (
            size_t i = progress_height;
            i < PROGRESS_HEIGHT;
            i++
        ) {
            dprintf(stdout_fd, "\r\033[2K\n");
        }
    }

    dprintf(stdout_fd, "\r\033[2K");

    for (size_t i = 0; i < bar_width; i++)
        dprintf(stdout_fd, "─");

    dprintf(stdout_fd, "\n");

    if (progress_info != NULL)
        dprintf(stdout_fd, "%s", progress_info);

    dprintf(stdout_fd, "\r\033[2K[");

    for (size_t i = 0; i < bar_width; i++) {
        if (i < filled)
            dprintf(stdout_fd, "█");
        else if (
            i == filled &&
            filled < bar_width
        )
            dprintf(stdout_fd, "▌");
        else
            dprintf(stdout_fd, "░");
    }

    dprintf(
        stdout_fd,
        "] %3zu%%\n",
        percent
    );

    dprintf(stdout_fd, "\r\033[2K");

    for (size_t i = 0; i < bar_width; i++)
        dprintf(stdout_fd, "─");

    dprintf(stdout_fd, "\n");

    progress_drawn = true;
}


static void catpkg_build_log(
    const char *format,
    struct CatpkgBuilder *builder,
    bool progressbar_enabled,
    ...
)
{
    if (build_log_active) {
        va_list args;

        va_start(args, progressbar_enabled);
        vprintf(format, args);
        va_end(args);

        fflush(stdout);
    }
}

static size_t catpkg_directory_count(const char *path)
{
    DIR *dir = opendir(path);

    if (dir == NULL)
        return 0;

    size_t count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (
            strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0
        )
            continue;

        count++;
    }

    closedir(dir);

    return count;
}

static int catpkg_builder_cache_change(
    const char *path
)
{
    int pid = fork();

    if (pid < 0) {
        perror("catpkg: cache: fork");
        return 1;
    }

    if (pid == 0) {
        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL) {
            fprintf(
                stderr,
                "catpkg: cache: failed to resolve path \"%s\"\n",
                path
            );

            _exit(1);
        }

        const char *catstrap =
            getenv("CATSTRAP-BIN");

        if (catstrap == NULL) {
            fprintf(
                stderr,
                "catpkg: cache: CATSTRAP-BIN is not set\n"
            );

            free(full_path);
            _exit(1);
        }

        if (
            access(
                CATPKG_CACHE "/.catpkg_builder_cache",
                F_OK
            ) == 0
        ) {
            execl(
                catstrap,
                catstrap,
                "tar",
                "-rf",
                CATPKG_CACHE "/.catpkg_builder_cache",
                "-C",
                "/",
                full_path + 1,
                (char *)NULL
            );
        }
        else {
            execl(
                catstrap,
                catstrap,
                "tar",
                "-cf",
                CATPKG_CACHE "/.catpkg_builder_cache",
                "-C",
                "/",
                full_path + 1,
                (char *)NULL
            );
        }

        int err = errno;

        fprintf(
            stderr,
            "catpkg: cache: failed to execute \"%s\": %s\n",
            catstrap,
            strerror(err)
        );

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("catpkg: cache: waitpid");
        return 1;
    }

    if (WIFSIGNALED(status)) {
        fprintf(
            stderr,
            "catpkg: cache: tar terminated by signal %d\n",
            WTERMSIG(status)
        );

        return 1;
    }

    if (!WIFEXITED(status)) {
        fprintf(
            stderr,
            "catpkg: cache: tar terminated abnormally\n"
        );

        return 1;
    }

    if (WEXITSTATUS(status) != 0) {
        fprintf(
            stderr,
            "catpkg: cache: tar exited with code %d "
            "while caching \"%s\"\n",
            WEXITSTATUS(status),
            path
        );

        return 1;
    }

    return 0;
}


static int catpkg_builder_cache_del(
    const char *path
)
{
    int pid = fork();

    if (pid < 0)
        return 1;

    if (pid == 0) {

        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL)
            _exit(1);

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "--delete",
            "-f",
            CATPKG_CACHE "/.catpkg_builder_cache",
            full_path + 1,
            (char *)NULL
        );

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        return 1;

    if (!WIFEXITED(status))
        return 1;

    if (WEXITSTATUS(status) != 0)
        return 1;

    return 0;
}


static int catpkg_builder_cache_extract(
    const char *path,
    const char *extract_to
)
{
    int pid = fork();

    if (pid < 0)
        return 1;

    if (pid == 0) {
        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL)
            _exit(1);

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "-xf",
            CATPKG_CACHE "/.catpkg_builder_cache",
            "-C",
            extract_to,
            full_path + 1,
            (char *)NULL
        );

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        return 1;

    if (!WIFEXITED(status))
        return 1;

    if (WEXITSTATUS(status) != 0)
        return 1;

    return 0;
}

static int catpkg_builder_cache_clear()
{
    if (access(CATPKG_CACHE "/.catpkg_builder_cache", F_OK) == 0)
        if (unlink(CATPKG_CACHE "/.catpkg_builder_cache") != 0) {
            perror("unlink");
        }
    return 0;
}

/*
 * ============================================================
 * UID parsing
 * ============================================================
 */

static int catpkg_parse_uid(
    const char *value,
    uid_t *uid
)
{
    if (value == NULL || uid == NULL)
        return 1;

    errno = 0;

    char *end;

    unsigned long parsed =
        strtoul(value, &end, 10);

    /*
     * Empty string.
     */
    if (end == value)
        return 1;

    /*
     * Invalid trailing characters.
     *
     * "1000abc" is invalid.
     */
    if (*end != '\0')
        return 1;

    /*
     * strtoul() overflow.
     */
    if (errno == ERANGE)
        return 1;

    /*
     * Make sure the value fits into uid_t.
     */
    if (parsed > (unsigned long)((uid_t)-1))
        return 1;

    *uid = (uid_t)parsed;

    return 0;
}

static int calc_size(
	off_t *change_size,
	enum CatpkgProtocol protocol,
	struct CatpkgRequest *request,
	const bool *revert,
    struct CatpkgBuilder *builder
) {
    struct stat statbuf;

    *change_size = 0;

    char *request_val =
        request->value;

	if(!revert)
		switch(protocol) {
            #include "builder/behaviors/calc_size.inc"
		}
	return 0;
}

static int request_protocol(
    enum CatpkgProtocol protocol,
    struct CatpkgRequest *request,
    struct CatpkgBuilder *builder
) {
    char *request_val =
        request->value;

    switch(protocol) {
        #include "builder/behaviors/request.inc"
    }
    return 0;
}

/*
 * ============================================================
 * Builder initialization
 * ============================================================
 */

int catpkg_builder_init(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    builder->requests = NULL;
    builder->requests_size = 0;
    builder->applied_count = 0;
    builder->change_size = 0;

    return 0;
}

int catpkg_builder_err_print(
    struct CatpkgBuilder *builder
) {
    stdout_buffering_print(builder -> stderr_buf, STDERR_FILENO);
    return 0;
}

/*
 * ============================================================
 * Request
 * ============================================================
 *
 *
 * result is not owned by Builder.
 *
 * ============================================================
 */

int catpkg_builder_request(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = NULL;
    request->context = NULL;
    request->previous_euid = 0;
    request->value_copy = 0;
    request->skip = 0;
    request->revert = 1;

    request->value = value;

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size * sizeof(*builder->requests)
        );

    if (tmp == NULL) {

        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size = new_size;

    off_t change_size = 0;
    
    calc_size(
    	&change_size, 
    	protocol,
    	request,
    	0,
        builder
    );
    
    builder -> change_size += change_size;

    return request_protocol(
        protocol,
        request,
        builder
    );;
}

int catpkg_builder_request_after_free(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = NULL;
    request->context = NULL;
    request->previous_euid = 0;
    request->value_copy = 1;
    request->skip = 0;
    request->revert = 1;
    request->value = value;


    /*
     * Add request to builder.
     */

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size *
                sizeof(*builder->requests)
        );

    if (tmp == NULL) {
        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size =
        new_size;


    /*
     * Calculate size change.
     */

    off_t change_size = 0;

    calc_size(
        &change_size,
        protocol,
        request,
        0,
        builder
    );

    builder->change_size +=
        change_size;

    progress_drawn = false;

    return request_protocol(
        protocol,
        request,
        builder
    );
}

/*
 * ============================================================
 * Request with result
 * ============================================================
 */

int catpkg_builder_request_value(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value,
    void *result
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = result;
    request->context = NULL;
    request->previous_euid = 0;
    request->skip = 0;
    request->revert = 1;
    request->value_copy = 0;

    request->value = value;

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size * sizeof(*builder->requests)
        );

    if (tmp == NULL) {

        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size = new_size;

    off_t change_size = 0;
    
    calc_size(
    	&change_size, 
    	protocol,
    	request,
    	0,
        builder
    );

    builder -> change_size += change_size;

    return request_protocol(
        protocol,
        request,
        builder
    );
}


/*
 * ============================================================
 * Apply
 * ============================================================
 */

static int apply(
    struct CatpkgBuilder *builder,
    int memfd,
    int stdout_fd
) {
    #ifdef BUILDER_DEBUG_APPLY
    #define X(name) #name,
    static const char *debug_protocols_names[] = {
            CATPKG_PROTOCOLS(X)
        };
    #undef X

    #define HEAD_PROTOCOL_DEBUG "\nPROTOCOL DEBUG INFO ( %s )\n"
    #endif

    for (
        size_t i = builder->applied_count;
        i < builder->requests_size;
        i++
    ) {
        struct CatpkgRequest *request =
            builder->requests[i];

        #ifdef BUILDER_DEBUG_APPLY
            builder_clear_progress(stdout_fd);
            dprintf(
                20,
                "\nBUILDER_DEBUG_APPLY ( %s )\n"
                "   STATUS: Start\n"
                "   protocol: %u\n"
                "   !!INFO!!\n"
                "   i: %u\n"
                "   value: %s\n"
                "   is_skip: %d\n"
                "   is_revert: %d\n",
                debug_protocols_names[request->protocol],
                request->protocol,
                i,
                request->value,
                request->skip,
                request->revert
            );
        #endif

        if (
            builder->applied_count %
            ((builder->requests_size + 9) / 10) == 0
        ) {
            builder_clear_progress(stdout_fd);
        
            stdout_buffering_flush(memfd, stdout_fd);
            stdout_buffering_start(memfd, stdout_fd);
        
            builder_progress_bar(
                builder->applied_count + 1,
                builder->requests_size,
                stdout_fd
            );
        }

        char *request_val =
            request->value;
        char *request_result =
            request->result;


        if(request->skip) {
            #ifdef BUILDER_DEBUG_APPLY
                builder_clear_progress(stdout_fd);
                dprintf(
                    20,
                    "\nBUILDER_DEBUG_APPLY ( %s )\n"
                    "   STATUS: SKIP\n"
                    "   protocol: %u\n"
                    "   !!INFO!!\n"
                    "   i: %u\n"
                    "   value: %s\n"
                    "   is_skip: %d\n"
                    "   is_revert: %d\n",
                    debug_protocols_names[request->protocol],
                    request->protocol,
                    i,
                    request->value,
                    request->skip,
                    request->revert
                );
            #endif
            catpkg_build_log("SKIP %u\n", builder, true, request->protocol);
            builder->applied_count++;
            continue;
        }
        #ifdef BUILDER_DEBUG_APPLY
            builder_clear_progress(stdout_fd);
            dprintf(
                20,
                "\nBUILDER_DEBUG_APPLY ( %s )\n"
                "   STATUS: BEFORE_APPLY\n"
                "   protocol: %u\n"
                "   !!INFO!!\n"
                "   i: %u\n"
                "   value: %s\n"
                "   is_skip: %d\n"
                "   is_revert: %d\n",
                debug_protocols_names[request->protocol],
                request->protocol,
                i,
                request->value,
                request->skip,
                request->revert
            );
        #endif
        switch (request->protocol) {
            #include "builder/behaviors/apply.inc"
        }

        #ifdef BUILDER_DEBUG_APPLY
            builder_clear_progress(stdout_fd);
            dprintf(
                20,
                "\nBUILDER_DEBUG_APPLY ( %s )\n"
                "   STATUS: AFTER_APPLY\n"
                "   protocol: %u\n"
                "   !!INFO!!\n"
                "   i: %u\n"
                "   value: %s\n"
                "   is_skip: %d\n"
                "   is_revert: %d\n",
                debug_protocols_names[request->protocol],
                request->protocol,
                i,
                request->value,
                request->skip,
                request->revert
            );
        #endif

        /*
         * Request was successfully applied.
         */
        builder->applied_count++;
    }

    printf("\n");
    return 0;
}

int catpkg_builder_apply(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    catpkg_builder_cache_clear();

    int memfd = memfd_create("catpkg_builder_log", 0);
    builder -> stderr_buf = memfd_create("catpkg_builder_err", 0);

    int stdout_fd = dup(STDOUT_FILENO);
    int stderr_fd = dup(STDERR_FILENO);

    stdout_buffering_start(builder -> stderr_buf, stderr_fd);

    if (fflush(stderr) == EOF) {
        close(stderr_fd);
        return -1;
    }

    if (dup2(builder->stderr_buf, STDERR_FILENO) == -1) {
        close(stderr_fd);
        return -1;
    }

    int result = -999;

    retry_apply:

    result = apply(
        builder,
        memfd,
        stdout_fd
    );

    if(result != 0) {
        // Try check EXCEPTION.
        if(
            builder->applied_count < builder->requests_size &&
            builder->requests[builder->applied_count+1]->protocol == EXCEPTION
        ) {
            // Exist skip EXCEPTION goto skip current protocol and continue to next.

            #ifdef BUILDER_DEBUG_APPLY
                builder_clear_progress(stdout_fd);
                dprintf(
                    20,
                    "\nBUILDER_DEBUG_APPLY ( EXCEPTION! )\n"
                );
            #endif

            char *error_code = calloc(16, sizeof(char));

            snprintf(
                error_code,
                16,
                "%d",
                result
            );

            builder->requests[builder->applied_count]->revert = 0;
            builder->requests[builder->applied_count]->skip = 1;
            builder->requests[builder->applied_count+1]->context = error_code;
            builder->requests[builder->applied_count+1]->skip = 0;

            builder->applied_count++; // Set to EXCEPTION protocol.

            result = -999;
            goto retry_apply;
        }
    }

    if (fflush(stderr) == EOF) {
        close(stderr_fd);
        return -1;
    }

    if (dup2(stderr_fd, STDERR_FILENO) == -1) {
        close(stderr_fd);
        return -1;
    }

    builder_clear_progress(stdout_fd);

    stdout_buffering_flush(memfd, stdout_fd);

    if(result != 0 && build_log_active) {
        printf("%s\n", "├ <ERROR SYS MESSAGE>");
        catpkg_builder_err_print(builder);
    }


    builder_progress_bar(
        builder->applied_count + 1,
        builder->requests_size,
        stdout_fd
    );

    close(memfd);
    return result;
}


/*
 * ============================================================
 * Revert
 * ============================================================
 */

#pragma push_macro("CATPKG_BUILD_LOG")
#pragma push_macro("CATPKG_BUILD_LOG_TREE")

#undef CATPKG_BUILD_LOG
#undef CATPKG_BUILD_LOG_TREE

#define CATPKG_BUILD_LOG_TREE(revert) \
    catpkg_build_log("%s── ", \
        builder, \
        false, \
        ((revert ? builder->applied_count - 1 <= 0 : builder->applied_count >= builder->requests_size - 1)) ? "└" : "├")

#define CATPKG_BUILD_LOG(format, result, revert, args) \
    catpkg_build_log( \
        format "%s%s\n", \
        builder, \
        false, \
        args, \
        (result)[0] != '\0' ? \
            ((revert ? builder->applied_count - 1 <= 0 : \
                       builder->applied_count >= builder->requests_size - 1) ? \
                "\n \t└── " : "\n│\t└── ") : \
            "", \
        (result)[0] != '\0' ? result : "")

int catpkg_builder_revert(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    printf("\n");

    while (builder->applied_count > 0) {

        size_t index =
            builder->applied_count - 1;

        struct CatpkgRequest *request =
            builder->requests[index];

        if(!request->revert) {
            builder->applied_count--;
            continue;
        }

        if(request->skip){
            catpkg_build_log("SKIP %u\n", builder, false, request->protocol);
            builder->applied_count--;
            continue;
        }

        char *request_val =
            request->value;

        switch (request->protocol) {
            #include "builder/behaviors/revert.inc"
        }

        /*
         * Request was successfully reverted.
         */
        builder->applied_count--;
    }

    return 0;
}

#pragma pop_macro("CATPKG_BUILD_LOG_TREE")
#pragma pop_macro("CATPKG_BUILD_LOG")


/*
 * ============================================================
 * Free
 * ============================================================
 */

int catpkg_builder_free(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    for (
        size_t i = 0;
        i < builder->requests_size;
        i++
    ) {
        void *context = builder->requests[i]->context;
        if(builder->requests[i]->value_copy)
            free(builder->requests[i]->value);

        switch(builder->requests[i] -> protocol) {
            #include "builder/behaviors/context_free.inc"
        }

        free(
            builder->requests[i]
        );
    }

    free(builder->requests);

    builder->requests = NULL;
    builder->requests_size = 0;
    builder->applied_count = 0;
    close(builder -> stderr_buf);

    return 0;
}