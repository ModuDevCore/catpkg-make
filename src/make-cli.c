#define _XOPEN_SOURCE 700

#include "install.h"
#include "remove.h"
#include "build.h"
#include "database.h"
#include "utils/fs.h"
#include "catpkg/pkginfo.h"
#include "../catstrap/cast/include/cast-format.h"
#include "../MAKE-CLI_CONFIG.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <fcntl.h>
#include <ftw.h>
#include <poll.h>



enum BuildOption {
    NONE,
#define X(option, description) option,
    BUILD_OPTIONS(X)
#undef X
};

static int remove_entry(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)st;
    (void)typeflag;
    (void)ftwbuf;

    return remove(path);
}

int remove_tmp_directory(void)
{
    printf(
        "[INFO] Removing the TMP directory: %s\n",
        ".tmp"
    );
    /*
     * Remove old temporary installer directory.
     */
    if (nftw(
            ".tmp",
            remove_entry,
            16,
            FTW_DEPTH | FTW_PHYS
        ) != 0) {

        if (errno != ENOENT)
            return 1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    int input = open(src, O_RDONLY);

    if (input < 0) {
        perror(src);
        return 1;
    }

    struct stat st;

    if (fstat(input, &st) < 0) {
        perror("fstat");
        close(input);
        return 1;
    }

    int output = open(
        dst,
        O_WRONLY | O_CREAT | O_TRUNC,
        st.st_mode & 0777
    );

    if (output < 0) {
        perror(dst);
        close(input);
        return 1;
    }

    if (fchmod(output, st.st_mode & 0777) < 0) {
        perror("fchmod");
        close(input);
        close(output);
        return 1;
    }

    char buffer[65536];
    ssize_t bytes_read;

    while (
        (bytes_read = read(
            input,
            buffer,
            sizeof(buffer)
        )) > 0
    ) {
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(
                output,
                buffer + total_written,
                (size_t)(bytes_read - total_written)
            );

            if (bytes_written < 0) {
                if (errno == EINTR)
                    continue;

                perror("write");
                close(input);
                close(output);
                return 1;
            }

            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(input);
        close(output);
        return 1;
    }

    if (close(input) < 0) {
        perror("close");
        close(output);
        return 1;
    }

    if (close(output) < 0) {
        perror("close");
        return 1;
    }

    return 0;
}

int catpkg_embed_binary(
    const char *input_path,
    FILE *output
)
{
    if (input_path == NULL || output == NULL) {
        errno = EINVAL;
        return 1;
    }

    FILE *input = fopen(input_path, "rb");

    if (input == NULL)
        return 1;

    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        return 1;
    }

    long file_size = ftell(input);

    if (file_size < 0) {
        fclose(input);
        return 1;
    }

    rewind(input);

    size_t package_size = (size_t)file_size;

    if (fprintf(
        output,
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "const uint8_t catpkg_embedded_package[] = {\n"
    ) < 0) {
        fclose(input);
        return 1;
    }

    uint8_t buffer[4096];
    size_t offset = 0;
    size_t read;

    while ((read = fread(buffer, 1, sizeof(buffer), input)) > 0) {

        for (size_t i = 0; i < read; i++) {

            if (fprintf(
                output,
                "    0x%02X%s",
                buffer[i],
                ((offset + 1) % 12 == 0) ? "\n" : ""
            ) < 0) {
                fclose(input);
                return 1;
            }

            if (offset + 1 < package_size) {
                if (fputc(',', output) == EOF) {
                    fclose(input);
                    return 1;
                }
            }

            offset++;
        }
    }

    if (ferror(input)) {
        fclose(input);
        return 1;
    }

    fclose(input);

    if (fprintf(
        output,
        "\n};\n\n"
        "const size_t catpkg_embedded_package_size = %zu;\n",
        package_size
    ) < 0) {
        return 1;
    }

    return 0;
}

int bootstrap_update()
{
    printf("Building bootstrap...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl(
            "update-bootstrap.sh",
            "update-bootstrap.sh",
            GCC_X86_64,
            GCC_AARCH64,
            "./extra/x86_64",
            "./extra/aarch64",
            (char *)NULL
        );

        perror("gcc");
        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0
    ) {
        fprintf(
            stderr,
            "gcc: bootstrap compilation failed\n"
        );

        return 1;
    }

    return 0;
}

static int run_script(const char *script)
{
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", script, (char *)NULL);
        perror("execl");
        _exit(127);
    }
    int status;
    do {
        if (waitpid(pid, &status, 0) >= 0)
            break;
        if (errno != EINTR) {
            perror("waitpid");
            return 1;
        }
    } while (1);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Build script failed.\n");
        return 1;
    }
    return 0;
}

static int read_choice(void)
{
    char input[64];
    if (!fgets(input, sizeof(input), stdin))
        return -1;
    char *end;
    errno = 0;
    long value = strtol(input, &end, 10);
    if (errno || end == input || (*end != '\n' && *end != '\0') || value < 1 || value > 1000)
        return -1;
    return (int)value;
}

static int prepare_arch(char **arch)
{
    printf("Select the processor architecture:\n1) x86_64\n2) aarch64\nEnter a number: ");
    fflush(stdout);
    int selected = read_choice();
    if (selected == 1){
        *arch = "x86_64";
        setenv("a10e", "x86_64", 1);
        setenv("compiler", GCC_X86_64, 1);
    }
    else if (selected == 2) {
        *arch = "aarch64";
        setenv("a10e", "aarch64", 1);
        setenv("compiler", GCC_AARCH64, 1);
    }
    else {
        fprintf(stderr, "Invalid architecture.\n");
        return 1;
    }
    return 0;
}

int main(void)
{

    fputs(MAKE_CLI_WELCOME_TEXT, stdout);
#define X(option, description) printf("%d) %s\n", option, description);
    BUILD_OPTIONS(X)
#undef X
    printf("\nEnter a number: ");
    fflush(stdout);
    int code = read_choice();

    #define X(option, env_name, env_value) \
        if(code == option) \
            setenv(env_name, env_value, 1);
        BUILD_ENV(X)
    #undef X

    int valid = 0;
#define X(option, description) if (code == option) valid = 1;
    BUILD_OPTIONS(X)
#undef X
    if (!valid) {
        fprintf(stderr, "Unknown option.\n");
        return 1;
    }

    if (remove_tmp_directory() != 0) {
        fprintf(stderr, "[FATAL] Failed to remove old temporary directory.\n");
        return 1;
    }

    char *arch = NULL;
    int prepare_package_arch = 0;
#define X(option) if (code == option) prepare_package_arch = 1;
    BUILD_ARCH_PREPARE(X)
#undef X
    if (prepare_package_arch && prepare_arch(&arch) != 0)
        return 1;

    int use_tmp = 0;
#define X(option) if (code == option) use_tmp = 1;
    BUILD_TMP(X)
#undef X
    if (use_tmp && mkdir(".tmp", 0755) != 0 && errno != EEXIST) {
        perror("mkdir .tmp");
        return 1;
    }

#define X(option, path) \
    if (code == option) { \
        char destination[PATH_MAX]; \
        int n = snprintf(destination, sizeof(destination), ".tmp/%s", path); \
        if (n < 0 || (size_t)n >= sizeof(destination)) return 1; \
        if (mkdir(destination, 0755) != 0 && errno != EEXIST) { \
            perror(destination); \
            return 1; \
        } \
    }
    BUILD_DIRECTORIES(X)
#undef X

#define X(option, source, destination) \
    if (code == option) { \
        char target[PATH_MAX]; \
        int n = snprintf(target, sizeof(target), ".tmp/%s", destination); \
        if (n < 0 || (size_t)n >= sizeof(target)) return 1; \
        if (copy_file(source, target) != 0) return 1; \
    }
    BUILD_DATA_IMPORT(X)
#undef X

    char postprocess_macros[2048] = {0};
    size_t postprocess_size = 0;
#define X(option, key, value) \
    if (code == option) { \
        int n = snprintf(postprocess_macros + postprocess_size, \
            sizeof(postprocess_macros) - postprocess_size, \
            "%s%s=%s", postprocess_size ? ":" : "", key, value); \
        if (n < 0 || (size_t)n >= sizeof(postprocess_macros) - postprocess_size) { \
            fprintf(stderr, "Postprocess macros are too long.\n"); \
            return 1; \
        } \
        postprocess_size += (size_t)n; \
    }
    BUILD_MACRO(X)
#undef X
    if (postprocess_size && setenv("POSTPROCESS_CUSTOM_MACRO", postprocess_macros, 1) != 0) {
        perror("setenv");
        return 1;
    }

    int need_bootstrap = 0;
#define X(option) if (code == option) need_bootstrap = 1;
    BUILD_BOOTSTRAP(X)
#undef X
    if (need_bootstrap && bootstrap_update() != 0)
        return 1;

    if (code == CATSTRAPLIB_CATPACKAGE) {
        char source[PATH_MAX];
        char destination[PATH_MAX];
        int a = snprintf(source, sizeof(source), "extra/%s/catstraplib.so.0", arch);
        int b = snprintf(destination, sizeof(destination), ".tmp/usr/lib/catpkg/catstraplib.so.%s", CATSTRAPLIB_CATPACKAGE_VERSION);
        if (a < 0 || b < 0 || (size_t)a >= sizeof(source) || (size_t)b >= sizeof(destination))
            return 1;
        if (copy_file(source, destination) != 0)
            return 1;
    }

    const char *script = NULL;
#define X(option, body) if (code == option) script = body;
    BUILD_SCRIPTS(X)
#undef X

    int script_tmp = 0;
#define X(option) if (code == option) script_tmp = 1;
    BUILD_SCRIPT_TMP(X)
#undef X
    if (script) {
        if (script_tmp && chdir(".tmp") != 0) {
            perror("chdir .tmp");
            return 1;
        }
        if (run_script(script) != 0)
            return 1;
    } else if (use_tmp && chdir(".tmp") != 0) {
        perror("chdir .tmp");
        return 1;
    }

    int package = 0;
#define X(option) if (code == option) package = 1;
    BUILD_PACKAGE(X)
#undef X
    if (package) {
        if (use_tmp && !script_tmp && script && chdir(".tmp") != 0) {
            perror("chdir .tmp");
            return 1;
        }
        if (setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0) != 0) {
            perror("setenv");
            return 1;
        }

        catpkg_build(arch);
    }
    printf("Build completed.\n");
    return 0;
}
