#include "catpkg/ctpg.h"
#include "catpkg/pkginfo.h"
#include "utils/fs.h"
#include "verify.h"
#include "database.h"
#include "configuration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

int ctpg_read_tar_header(
    struct header_posix_ustar *header,
    FILE *file
)
{
    if (header == NULL || file == NULL)
        return 1;

    if (fread(
        header,
        1,
        sizeof(*header),
        file
    ) != sizeof(*header)) {
        return 1;
    }

    /*
     * TAR end-of-archive marker.
     *
     * TAR ends with two 512-byte blocks
     * filled with zero bytes.
     */
    if (tar_header_is_empty((char *)header))
        return 1;

    return 0;
}

int ctpg_context_init(
    struct CtpgContext **ctpg_context,
    char *path
) {
    
    if (ctpg_context == NULL) 
        return 1;

    if (*ctpg_context == NULL) {
        *ctpg_context = malloc(sizeof(**ctpg_context));

        if (*ctpg_context == NULL)
            return 1;

        (*ctpg_context)->path = NULL;
    }

    struct CtpgContext *context = *ctpg_context;

    struct PackageInfo *info = CATPKG_Parse(path, NULL);
    CATPKG_ReadSections(info, 1);

    if (info == NULL)
        return 1;

    struct PackageField *last_field =
        info->fields;

    if (last_field != NULL) {
        while (last_field->next_field != NULL)
            last_field = last_field->next_field;
    }

    const struct PackageField *name_field = PackageInfo_Find(info, "name");

    char *commands_data_dir =
        make_catpkg_path(
            CATPKG_PACKAGE_DATA
            "/.catpkg/commands/DATA",
            name_field -> value
        );

    #define __PACKAGEINFO__ info
    #define __PACKAGE_COMMANDS_PATH__ commands_data_dir
    #include "../../../common/INSTALL_POSTPROCESS_PACKAGEINFO.inc"
    #undef __PACKAGEINFO__
    #undef __PACKAGE_COMMANDS_PATH__

    free(commands_data_dir);

    context->path = path;

    context->pkginfo             = info;
    context->name_field          = PackageInfo_Find(info, "name");
    context->version_field       = PackageInfo_Find(info, "version");
    context->description_field   = PackageInfo_Find(info, "description");
    context->sha256_field        = PackageInfo_Find(info, "SHA");
    context->size_field          = PackageInfo_Find(info, "size");
    context->dependency_field    = PackageInfo_Find(info, "dependency");
    context->operations_field    = PackageInfo_Find(info, "operations");

    if (
        context->name_field == NULL ||
        context->version_field == NULL ||
        context->sha256_field == NULL
    ) {
        PackageInfo_Free(info);
        return 1;
    }

    char package_fullname[256];

    int package_fullname_length =
        snprintf(
            package_fullname,
            sizeof(package_fullname),
            "%s@%s",
            context->name_field->value,
            context->version_field->value
        );

    if (
        package_fullname_length < 0 ||
        (size_t)package_fullname_length >= sizeof(package_fullname)
    ) {
        fprintf(
            stderr,
            "catpkg: package name and version are too long\n"
        );

        PackageInfo_Free(info);
        return 1;
    }

    context->full_name =
        malloc((size_t)package_fullname_length + 1);

    if (context->full_name == NULL) {
        PackageInfo_Free(info);
        return 1;
    }

    memcpy(
        context->full_name,
        package_fullname,
        (size_t)package_fullname_length + 1
    );

    return 0;
}

void ctpg_context_close(
    struct CtpgContext **ctpg_context
)
{
    if (ctpg_context == NULL || *ctpg_context == NULL)
        return;

    struct CtpgContext *context = *ctpg_context;

    PackageInfo_Free(context->pkginfo);
    free(context->full_name);

    free(context);

    *ctpg_context = NULL;
}

int catpkg_builder_catpackage_extract(
    struct CtpgContext *ctpg_context,
    const char *path,
    const char *extract_to,
    char *exclude[]
)
{
    size_t exclude_count = 0;

    if (exclude != NULL) {
        while (exclude[exclude_count] != NULL)
            exclude_count++;
    }


    char *correct_path =
        catpkg_normalize_tar_path(path);

    if (correct_path == NULL)
        return 1;


    /*
     * tar
     * -xf
     * archive
     * -C
     * extract_to
     * [--exclude=...]*
     * path
     * NULL
     */

    size_t argc =
        8 + exclude_count;

    char **argv =
        calloc(argc, sizeof(*argv));

    if (argv == NULL) {
        free(correct_path);
        return 1;
    }


    size_t i = 0;

    argv[i++] = getenv("CATSTRAP-BIN");

    argv[i++] = "tar";
    argv[i++] = "-xf";
    argv[i++] = (char *)ctpg_context->path;
    argv[i++] = "-C";

    argv[i++] = (char *)extract_to;

    size_t exclude_start = i;

    if (exclude != NULL) {
        for (
            size_t j = 0;
            j < exclude_count;
            j++
        ) {
            size_t len =
                strlen("--exclude=") +
                strlen(exclude[j]) +
                1;

            argv[i] = malloc(len);

            if (argv[i] == NULL) {
                for (
                    size_t k = exclude_start;
                    k < i;
                    k++
                )
                    free(argv[k]);

                free(argv);
                free(correct_path);

                return 1;
            }

            snprintf(
                argv[i],
                len,
                "--exclude=%s",
                exclude[j]
            );

            i++;
        }
    }


    argv[i++] = correct_path;
    argv[i] = NULL;



    pid_t pid = fork();

    if (pid < 0) {
        for (
            size_t j = exclude_start;
            j < exclude_start + exclude_count;
            j++
        )
            free(argv[j]);

        free(argv);
        free(correct_path);

        return 1;
    }


    if (pid == 0) {

        const char *catstrap = getenv("CATSTRAP-BIN");

        execv(catstrap, argv);
        perror("execv");

        _exit(127);
    }


    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");

        for (
            size_t j = exclude_start;
            j < exclude_start + exclude_count;
            j++
        )
            free(argv[j]);

        free(argv);
        free(correct_path);

        return 1;
    }

    for (
        size_t j = exclude_start;
        j < exclude_start + exclude_count;
        j++
    )
        free(argv[j]);

    free(argv);
    free(correct_path);

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            printf(
                "tar: child process exited with status %d\n",
                exit_code
            );

            return 1;
        }
    } else if (WIFSIGNALED(status)) {
        printf(
            "tar: child process terminated by signal %d (%s)\n",
            WTERMSIG(status),
            strsignal(WTERMSIG(status))
        );

        return 1;
    } else {
        printf(
            "tar: child process terminated unexpectedly\n"
        );

        return 1;
    }
    return 0;
}
int find_ctpg_hpu_info(
    const char *path,
    FILE *tarfile,
    struct header_posix_ustar *hpu
) {
    char *correct_search_path =
        catpkg_normalize_tar_path(
            path
        );

    while (
        ctpg_read_tar_header(
            hpu,
            tarfile
        ) == 0
    ) {
        char size_string[13];

        memcpy(
            size_string,
            hpu -> size,
            sizeof(hpu -> size)
        );

        size_string[
            sizeof(hpu ->size)
        ] = '\0';

        off_t data_size =
            (off_t)strtoull(
                size_string,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        if (
            fseek(
                tarfile,
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR data"
            );

            fclose(tarfile);

            return 1;
        }

        if (
            fseek(
                tarfile,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            fclose(tarfile);

            return 1;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu -> name
            );

        if (correct_path == NULL) {

            fclose(tarfile);

            return 1;
        }

        if(strncmp(
                correct_path,
                correct_search_path,
                strlen(correct_search_path)
            ) == 0 &&
            correct_path[strlen(correct_search_path)] == '/') {
            free(correct_search_path);
            return 0;
        }

        free(correct_path);
    }

    free(correct_search_path);
    return 1;
}
int catpkg_builder_catpackage_extract_mapped(
    struct CtpgContext *ctpg_context,
    const char *path,
    const char *extract_to,
    char *exclude[]
)
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL ||
        path == NULL ||
        extract_to == NULL
    )
        return 1;


    char *correct_path =
        catpkg_normalize_tar_path(path);

    if (correct_path == NULL)
        return 1;

    if (correct_path[0] == '\0') {
        free(correct_path);
        return 1;
    }


    /*
     * Count parent components.
     *
     * extra/COMMAND_DATA/catpkg-build-cli
     *
     * --strip-components=2
     *
     * Result:
     * catpkg-build-cli
     */

    size_t strip_count = 0;

    for (
        const char *p = correct_path;
        *p != '\0';
        p++
    ) {
        if (
            *p == '/' &&
            p[1] != '\0'
        )
            strip_count++;
    }


    char strip_argument[64];

    int strip_len = snprintf(
        strip_argument,
        sizeof(strip_argument),
        "--strip-components=%zu",
        strip_count
    );

    if (
        strip_len < 0 ||
        (size_t)strip_len >= sizeof(strip_argument)
    ) {
        free(correct_path);
        return 1;
    }


    /*
     * Get source basename.
     *
     * extra/COMMAND_DATA/catpkg-build-cli
     *                    ↓
     *             catpkg-build-cli
     */

    const char *source_name =
        strrchr(correct_path, '/');

    if (source_name != NULL)
        source_name++;
    else
        source_name = correct_path;

    if (*source_name == '\0') {
        free(correct_path);
        return 1;
    }


    /*
     * extract_to is the exact destination:
     *
     * /var/lib/.../DATA/build
     *
     * tar requires:
     *
     * -C /var/lib/.../DATA
     */

    char *extract_dir_buffer =
        strdup(extract_to);

    if (extract_dir_buffer == NULL) {
        free(correct_path);
        return 1;
    }

    char *extract_dir =
        dirname(extract_dir_buffer);


    /*
     * Build the path that tar itself will create:
     *
     * extract_dir/source_name
     *
     * Example:
     *
     * /var/lib/.../DATA/catpkg-build-cli
     */

    size_t extract_dir_len =
        strlen(extract_dir);

    size_t source_name_len =
        strlen(source_name);

    bool root_dir =
        extract_dir_len == 1 &&
        extract_dir[0] == '/';

    size_t extracted_path_size =
        extract_dir_len +
        (root_dir ? 0 : 1) +
        source_name_len +
        1;

    char *extracted_path =
        malloc(extracted_path_size);

    if (extracted_path == NULL) {
        free(extract_dir_buffer);
        free(correct_path);
        return 1;
    }

    if (root_dir) {
        snprintf(
            extracted_path,
            extracted_path_size,
            "/%s",
            source_name
        );
    }
    else {
        snprintf(
            extracted_path,
            extracted_path_size,
            "%s/%s",
            extract_dir,
            source_name
        );
    }


    /*
     * Count excludes.
     */

    size_t exclude_count = 0;

    if (exclude != NULL) {
        while (exclude[exclude_count] != NULL)
            exclude_count++;
    }


    /*
     * argv:
     *
     * catstrap
     * tar
     * -xf
     * archive
     * -C
     * extract_dir
     * --strip-components=N
     * [--exclude=...]*
     * correct_path
     * NULL
     */

    size_t argc =
        9 + exclude_count;

    char **argv =
        calloc(
            argc,
            sizeof(*argv)
        );

    if (argv == NULL) {
        free(extracted_path);
        free(extract_dir_buffer);
        free(correct_path);
        return 1;
    }


    char *catstrap =
        getenv("CATSTRAP-BIN");

    if (catstrap == NULL) {
        free(argv);
        free(extracted_path);
        free(extract_dir_buffer);
        free(correct_path);
        return 1;
    }


    size_t i = 0;

    argv[i++] = catstrap;
    argv[i++] = "tar";
    argv[i++] = "-xf";
    argv[i++] = (char *)ctpg_context->path;
    argv[i++] = "-C";
    argv[i++] = extract_dir;
    argv[i++] = strip_argument;


    size_t exclude_start = i;

    if (exclude != NULL) {
        for (
            size_t j = 0;
            j < exclude_count;
            j++
        ) {
            size_t len =
                strlen("--exclude=") +
                strlen(exclude[j]) +
                1;

            argv[i] =
                malloc(len);

            if (argv[i] == NULL) {
                for (
                    size_t k = exclude_start;
                    k < i;
                    k++
                )
                    free(argv[k]);

                free(argv);
                free(extracted_path);
                free(extract_dir_buffer);
                free(correct_path);

                return 1;
            }

            snprintf(
                argv[i],
                len,
                "--exclude=%s",
                exclude[j]
            );

            i++;
        }
    }


    argv[i++] = correct_path;
    argv[i] = NULL;


    pid_t pid =
        fork();

    if (pid < 0) {
        for (
            size_t j = exclude_start;
            j < exclude_start + exclude_count;
            j++
        )
            free(argv[j]);

        free(argv);
        free(extracted_path);
        free(extract_dir_buffer);
        free(correct_path);

        return 1;
    }


    if (pid == 0) {
        execv(
            catstrap,
            argv
        );

        perror(
            "catpkg: CTPG_EXTRACT_MAPPED: execv"
        );

        _exit(127);
    }


    int status;

    if (
        waitpid(
            pid,
            &status,
            0
        ) < 0
    ) {
        for (
            size_t j = exclude_start;
            j < exclude_start + exclude_count;
            j++
        )
            free(argv[j]);

        free(argv);
        free(extracted_path);
        free(extract_dir_buffer);
        free(correct_path);

        return 1;
    }


    for (
        size_t j = exclude_start;
        j < exclude_start + exclude_count;
        j++
    )
        free(argv[j]);

    free(argv);


    /*
     * tar failed.
     */

    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0
    ) {
        free(extracted_path);
        free(extract_dir_buffer);
        free(correct_path);

        return 1;
    }


    /*
     * tar extracted:
     *
     * extract_dir/source_name
     *
     * Move it to the exact mapped destination.
     *
     * If both paths are already identical,
     * nothing has to be done.
     */

    if (
        strcmp(
            extracted_path,
            extract_to
        ) != 0
    ) {
        if (
            rename(
                extracted_path,
                extract_to
            ) != 0
        ) {
            perror(
                "catpkg: CTPG_EXTRACT_MAPPED: rename"
            );

            free(extracted_path);
            free(extract_dir_buffer);
            free(correct_path);

            return 1;
        }
    }


    free(extracted_path);
    free(extract_dir_buffer);
    free(correct_path);

    return 0;
}

int catpkg_builder_ctpg_path_exists(
    struct CtpgContext *ctpg_context,
    const char *path,
    int include_children
)
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL ||
        path == NULL
    )
        return -1;

    char *correct_path =
        catpkg_normalize_tar_path(path);

    if (correct_path == NULL)
        return -1;

    if (include_children) {
        pid_t pid = fork();

        if (pid < 0) {
            free(correct_path);
            return -1;
        }

        if (pid == 0) {
            int devnull =
                open(
                    "/dev/null",
                    O_WRONLY
                );

            if (devnull >= 0) {
                dup2(
                    devnull,
                    STDOUT_FILENO
                );

                dup2(
                    devnull,
                    STDERR_FILENO
                );

                close(devnull);
            }

            execl(
                getenv("CATSTRAP-BIN"),
                getenv("CATSTRAP-BIN"),
                "tar",
                "-tf",
                ctpg_context->path,
                correct_path,
                (char *)NULL
            );

            _exit(127);
        }

        int status;

        if (
            waitpid(
                pid,
                &status,
                0
            ) < 0
        ) {
            free(correct_path);
            return -1;
        }

        free(correct_path);

        if (!WIFEXITED(status))
            return -1;

        return
            WEXITSTATUS(status) == 0
                ? 1
                : 0;
    }

    int pipefd[2];

    if (pipe(pipefd) != 0) {
        free(correct_path);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);

        free(correct_path);

        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);

        if (
            dup2(
                pipefd[1],
                STDOUT_FILENO
            ) < 0
        )
            _exit(127);

        close(pipefd[1]);

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "-tf",
            ctpg_context->path,
            (char *)NULL
        );

        _exit(127);
    }

    close(pipefd[1]);

    FILE *stream =
        fdopen(
            pipefd[0],
            "r"
        );

    if (stream == NULL) {
        close(pipefd[0]);

        int status;

        waitpid(
            pid,
            &status,
            0
        );

        free(correct_path);

        return -1;
    }

    char *line = NULL;
    size_t line_capacity = 0;

    int found = 0;

    while (
        getline(
            &line,
            &line_capacity,
            stream
        ) != -1
    ) {
        size_t line_len =
            strlen(line);

        while (
            line_len > 0 &&
            (
                line[line_len - 1] == '\n' ||
                line[line_len - 1] == '\r'
            )
        ) {
            line[--line_len] = '\0';
        }

        char *entry =
            catpkg_normalize_tar_path(
                line
            );

        if (entry == NULL)
            continue;

        size_t entry_len =
            strlen(entry);

        while (
            entry_len > 0 &&
            entry[entry_len - 1] == '/'
        ) {
            entry[--entry_len] = '\0';
        }

        size_t path_len =
            strlen(correct_path);

        while (
            path_len > 0 &&
            correct_path[path_len - 1] == '/'
        ) {
            correct_path[--path_len] = '\0';
        }

        if (
            strcmp(
                entry,
                correct_path
            ) == 0
        ) {
            found = 1;
        }

        free(entry);
    }

    free(line);

    fclose(stream);

    int status;

    if (
        waitpid(
            pid,
            &status,
            0
        ) < 0
    ) {
        free(correct_path);
        return -1;
    }

    free(correct_path);

    if (!WIFEXITED(status))
        return -1;

    if (WEXITSTATUS(status) != 0)
        return -1;

    return found;
}

int catpkg_builder_ctpg_file_obsolete(
    struct CtpgContext *ctpg_context,
    const char *path
)
{
    if (
        ctpg_context == NULL ||
        path == NULL ||
        ctpg_context->name_field->value == NULL
    ) {
        return -1;
    }


    /*
     * File exists in the new CTPG payload.
     */

    int exists =
        catpkg_builder_ctpg_path_exists(
            ctpg_context,
            path,
            0
        );

    if (exists < 0)
        return -1;

    if (exists == 1)
        return 0;


    /*
     * Parse global DATABASE.
     *
     * DATABASE contains:
     *
     *     #use-options DEPENDENCIES
     *
     * therefore CATPKG_Parse() also exposes
     * #persistent records.
     */

    struct PackageInfo *database_fileinfo =
        CATPKG_Parse(
            CATPKG_DATABASE_DIR_PATH "/"
            CATPKG_DATABASE,
            NULL
        );

    if (database_fileinfo == NULL)
        return -1;

    int persistent =
        catpkg_database_path_persistent(
            database_fileinfo,
            ctpg_context->name_field->value,
            path,
            0
        );

    PackageInfo_Free(
        database_fileinfo
    );

    if (persistent < 0)
        return -1;

    if (persistent == 1)
        return 0;


    /*
     * File is absent from the new payload and
     * has no persistent ownership.
     */

    return 1;
}
int catpkg_builder_ctpg_path_obsolete(
    struct CtpgContext *ctpg_context,
    const char *path,
    int include_children
)
{
    if (
        ctpg_context == NULL ||
        path == NULL
    ) {
        return -1;
    }


    /*
     * --------------------------------------------------------
     * Check path in the new CTPG payload.
     * --------------------------------------------------------
     *
     *  1 - path exists
     *  0 - path does not exist
     * -1 - error
     */

    int exists =
        catpkg_builder_ctpg_path_exists(
            ctpg_context,
            path,
            include_children
        );

    if (exists < 0)
        return -1;

    /*
     * Path is still provided by the new package.
     */

    if (exists == 1)
        return 0;


    /*
     * --------------------------------------------------------
     * Normalize searched path.
     * --------------------------------------------------------
     */

    char *correct_search_path =
        catpkg_normalize_tar_path(
            path
        );

    if (correct_search_path == NULL)
        return -1;

    size_t search_path_len =
        strlen(correct_search_path);


    /*
     * --------------------------------------------------------
     * Check #folder declarations from the new PACKAGEINFO.
     * --------------------------------------------------------
     *
     * A directory may not exist explicitly in the CTPG
     * payload but can still be required by:
     *
     *     #folder /some/path
     *
     * With include_children enabled, a parent directory
     * is also required if a #folder exists below it.
     */

    const struct PackageField *folder_field =
        ctpg_context->folder_field;

    while (folder_field != NULL) {

        if (
            folder_field->name == NULL ||
            strcmp(
                folder_field->name,
                "#folder"
            ) != 0
        ) {
            folder_field =
                folder_field->next_field;

            continue;
        }

        if (
            folder_field->value == NULL ||
            folder_field->value[0] == '\0'
        ) {
            folder_field =
                folder_field->next_field;

            continue;
        }


        char *correct_folder_path =
            catpkg_normalize_tar_path(
                folder_field->value
            );

        if (correct_folder_path == NULL) {
            free(correct_search_path);
            return -1;
        }


        /*
         * Exact match.
         */

        if (
            strcmp(
                correct_search_path,
                correct_folder_path
            ) == 0
        ) {
            free(correct_folder_path);
            free(correct_search_path);

            return 0;
        }


        /*
         * Child match.
         *
         * searched:
         *
         *     /var/lib/catpkg
         *
         * #folder:
         *
         *     /var/lib/catpkg/packages
         */

        if (
            include_children &&
            search_path_len > 0 &&
            strncmp(
                correct_folder_path,
                correct_search_path,
                search_path_len
            ) == 0 &&
            correct_folder_path[
                search_path_len
            ] == '/'
        ) {
            free(correct_folder_path);
            free(correct_search_path);

            return 0;
        }


        free(correct_folder_path);

        folder_field =
            folder_field->next_field;
    }


    /*
     * correct_search_path is no longer needed.
     *
     * catpkg_database_path_persistent() performs
     * its own normalization.
     */

    free(correct_search_path);


    /*
     * --------------------------------------------------------
     * Check persistent ownership in DATABASE.
     * --------------------------------------------------------
     *
     * DATABASE:
     *
     *     -
     *     #type file
     *     #path /var/lib/catpkg/database/DATABASE
     *
     * DEPENDENCIES:
     *
     *     #required 11=catpkg
     *     #persistent 11=catpkg
     *
     * DATABASE contains:
     *
     *     #use-options DEPENDENCIES
     *
     * therefore CATPKG_Parse() exposes the dependency
     * fields together with DATABASE.
     */

    struct PackageInfo *database_fileinfo = CATPKG_Parse(
        CATPKG_DATABASE_DIR_PATH "/"
        CATPKG_DATABASE,
        NULL
    );
    CATPKG_ReadAll(database_fileinfo);

    if (database_fileinfo == NULL)
        return -1;


    /*
     * IMPORTANT:
     *
     * Replace ctpg_context->name_field->value below if the
     * package name is stored under another member in
     * CtpgContext.
     */

    if (ctpg_context->name_field->value == NULL) {
        PackageInfo_Free(
            database_fileinfo
        );

        return -1;
    }


    int persistent =
        catpkg_database_path_persistent(
            database_fileinfo,
            ctpg_context->name_field->value,
            path,
            include_children
        );


    PackageInfo_Free(
        database_fileinfo
    );


    if (persistent < 0)
        return -1;


    /*
     * The path itself is persistent, or when
     * include_children is enabled, it contains
     * a persistent object.
     */

    if (persistent == 1)
        return 0;


    /*
     * --------------------------------------------------------
     * Obsolete.
     * --------------------------------------------------------
     *
     * The path:
     *
     * - does not exist in the new CTPG payload;
     * - is not required by #folder;
     * - does not have persistent ownership;
     * - does not contain a required persistent object
     *   when include_children is enabled.
     */

    return 1;
}
int catpkg_builder_ctpg_extract_size(
    struct CtpgContext *ctpg_context,
    const char *path,
    off_t *size,
    char *exclude[]
)
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL ||
        size == NULL
    ) {
        return 1;
    }

    /*
     * Normalize TAR path.
     *
     * ./usr/bin/catpkg
     *        ↓
     * usr/bin/catpkg
     */
    char *path_prefix =
        catpkg_normalize_tar_path(path);

    if (path_prefix == NULL)
        return 1;

    FILE *file =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (file == NULL) {
        perror(
            "catpkg: failed to open package"
        );

        free(path_prefix);

        return 1;
    }

    struct header_posix_ustar hpu;

    *size = 0;

    while (ctpg_read_tar_header(&hpu, file) == 0) {
        off_t data_size =
            (off_t)strtoull(
                hpu.size,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        /*
         * Skip TAR data.
         */
        if (
            fseek(
                file,
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {
            perror(
                "catpkg: failed to skip TAR data"
            );

            free(path_prefix);
            fclose(file);

            return 1;
        }

        /*
         * Skip TAR padding.
         */
        if (
            fseek(
                file,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {
            perror(
                "catpkg: failed to skip TAR padding"
            );

            free(path_prefix);
            fclose(file);

            return 1;
        }

        /*
         * Normalize TAR path.
         *
         * ./usr/bin/catpkg
         *        ↓
         * usr/bin/catpkg
         */
        char *correct_path =
            catpkg_normalize_tar_path(hpu.name);

        if (correct_path == NULL) {
            fprintf(
                stderr,
                "catpkg: failed to normalize TAR path\n"
            );

            free(path_prefix);
            fclose(file);

            return 1;
        }

        size_t prefix_len =
            strlen(path_prefix);

        if (
            is_excluded_tar_path(
                correct_path,
                exclude,
                1
            ) == 0 &&
            (
                prefix_len == 0 ||
                (
                    strncmp(
                        correct_path,
                        path_prefix,
                        prefix_len
                    ) == 0 &&
                    (
                        correct_path[prefix_len] == '\0' ||
                        correct_path[prefix_len] == '/'
                    )
                )
            )
        ) {
            /*
             * Build the destination path.
             *
             * Example:
             *
             * path:
             * /tmp/root
             *
             * correct_path:
             * usr/bin/catpkg
             *
             * result:
             * /tmp/root/usr/bin/catpkg
             */
            char destination[PATH_MAX];

            int destination_length =
                snprintf(
                    destination,
                    sizeof(destination),
                    "%s/%s",
                    path,
                    correct_path
                );

            if (
                destination_length < 0 ||
                (size_t)destination_length >=
                    sizeof(destination)
            ) {
                fprintf(
                    stderr,
                    "catpkg: destination path is too long\n"
                );

                free(correct_path);
                free(path_prefix);
                fclose(file);

                return 1;
            }

            /*
             * Calculate the actual size change.
             *
             * File does not exist:
             *
             *     0 -> new_size
             *     change = +new_size
             *
             * File already exists:
             *
             *     old_size -> new_size
             *     change = new_size - old_size
             */
            struct stat st;

            if (
                lstat(
                    destination,
                    &st
                ) == 0
            ) {
                if (S_ISREG(st.st_mode)) {
                    *size +=
                        data_size -
                        st.st_size;
                }
                else {
                    /*
                     * The existing object is not a
                     * regular file.
                     *
                     * Its previous file-data size is
                     * not represented by st_size in
                     * the same way.
                     */
                    *size += data_size;
                }
            }
            else if (errno == ENOENT) {
                /*
                 * File does not exist.
                 */
                *size += data_size;
            }
            else {
                perror(
                    "catpkg: lstat"
                );

                free(correct_path);
                free(path_prefix);
                fclose(file);

                return 1;
            }
        }

        free(correct_path);
    }

    fclose(file);
    free(path_prefix);

    return 0;
}
void *catpkg_builder_ctpg_read_data(
    struct CtpgContext *ctpg_context,
    const char *path
)
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL ||
        path == NULL
    ) {
        return NULL;
    }

    char *correct_search_path =
        catpkg_normalize_tar_path(
            path
        );

    if (correct_search_path == NULL)
        return NULL;


    FILE *file =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (file == NULL) {
        perror(
            "catpkg: failed to open package"
        );

        free(correct_search_path);

        return NULL;
    }


    struct header_posix_ustar hpu;


    while (
        ctpg_read_tar_header(
            &hpu,
            file
        ) == 0
    ) {
        size_t data_size =
            (size_t)strtoull(
                hpu.size,
                NULL,
                8
            );

        size_t padding =
            (512 - (data_size % 512)) % 512;


        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {
            fprintf(
                stderr,
                "catpkg: failed to normalize TAR path\n"
            );

            fclose(file);
            free(correct_search_path);

            return NULL;
        }


        /*
         * Found requested TAR entry.
         */

        if (
            strcmp(
                correct_path,
                correct_search_path
            ) == 0
        ) {
            free(correct_path);

            void *data =
                malloc(
                    data_size + 1
                );

            if (data == NULL) {
                fclose(file);
                free(correct_search_path);

                return NULL;
            }


            if (
                data_size > 0 &&
                fread(
                    data,
                    1,
                    data_size,
                    file
                ) != data_size
            ) {
                fprintf(
                    stderr,
                    "catpkg: failed to read TAR data\n"
                );

                free(data);
                fclose(file);
                free(correct_search_path);

                return NULL;
            }


            /*
             * Allow text data to be used
             * directly as a C string.
             */

            ((char *)data)[data_size] = '\0';


            fclose(file);
            free(correct_search_path);

            return data;
        }


        free(correct_path);


        /*
         * Skip TAR entry data + padding.
         */

        if (
            fseek(
                file,
                (long)(data_size + padding),
                SEEK_CUR
            ) != 0
        ) {
            perror(
                "catpkg: failed to skip TAR entry"
            );

            fclose(file);
            free(correct_search_path);

            return NULL;
        }
    }


    fclose(file);
    free(correct_search_path);

    return NULL;
}