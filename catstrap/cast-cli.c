#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>  
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>  
#include <sys/sendfile.h>

#include "cast-format.h"
#include "install.h"
#include "update.h"
#include "remove.h"
#include "database.h"

extern char **environ;

void print_package_separator(
    const char *action,
    const char *package_fullname
)
{
    const int width = 60;

    char text[256];

    snprintf(
        text,
        sizeof(text),
        "  %s %s  ",
        action,
        package_fullname
    );

    int text_len = (int)strlen(text);

    if (text_len >= width) {
        printf("%s\n", text);
        return;
    }

    int remaining = width - text_len;

    int left = remaining / 2;
    int right = remaining - left;

    for (int i = 0; i < left; i++)
        putchar('-');

    fputs(text, stdout);

    for (int i = 0; i < right; i++)
        putchar('-');

    putchar('\n');
}

int main(int argc, char *argv[])
{
    if(argc < 2)
        return 1;

     if (strcmp(argv[1], "--append") == 0) {
        printf("%s\n", "Preparation...");

        int fd = open(argv[0], O_RDWR);
        if (fd == -1) {
            perror("open");
            return 1;
        }

        off_t file_end = lseek(fd, 0, SEEK_END);
        if (file_end == (off_t)-1) {
            perror("lseek");
            close(fd);
            return 1;
        }

        off_t cast_header_free = (off_t)CAST_HEADER_FREE;

        if (file_end < cast_header_free) {
            close(fd);
            return 1;
        }

        int memfd = memfd_create("cast-tar-payload", 0);
        if (memfd == -1) {
            perror("memfd_create");
            close(fd);
            return 1;
        }

        off_t source_offset = cast_header_free;
        off_t remaining = file_end - cast_header_free;

        while (remaining > 0) {
            ssize_t n = sendfile(
                memfd,
                fd,
                &source_offset,
                (size_t)remaining
            );

            if (n <= 0) {
                if (n == -1)
                    perror("sendfile");

                close(memfd);
                close(fd);
                return 1;
            }

            remaining -= n;
        }

        char proc_fd_path[64];
        snprintf(
            proc_fd_path,
            sizeof(proc_fd_path),
            "/proc/self/fd/%d",
            memfd
        );

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            close(memfd);
            close(fd);
            return 1;
        }

        if (pid == 0) {
            char **tar_argv =
                malloc((argc + 2) * sizeof(*tar_argv));

            if (tar_argv == NULL)
                _exit(1);

            size_t i = 0;

            tar_argv[i++] = "tar";
            tar_argv[i++] = "-rf";
            tar_argv[i++] = proc_fd_path;

            for (int j = 2; j < argc; j++)
                tar_argv[i++] = argv[j];

            tar_argv[i] = NULL;

            execv("tar", tar_argv);

            perror("execv");
            _exit(1);
        }

        int status;

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            close(memfd);
            close(fd);
            return 1;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            close(memfd);
            close(fd);
            return 1;
        }

        struct stat st;

        if (fstat(memfd, &st) == -1) {
            perror("fstat");
            close(memfd);
            close(fd);
            return 1;
        }

        off_t memfd_size = st.st_size;

        if (ftruncate(fd, cast_header_free + memfd_size) == -1) {
            perror("ftruncate");
            close(memfd);
            close(fd);
            return 1;
        }

        if (lseek(fd, cast_header_free, SEEK_SET) == (off_t)-1) {
            perror("lseek");
            close(memfd);
            close(fd);
            return 1;
        }

        off_t memfd_offset = 0;
        remaining = memfd_size;

        while (remaining > 0) {
            ssize_t n = sendfile(
                fd,
                memfd,
                &memfd_offset,
                (size_t)remaining
            );

            if (n <= 0) {
                if (n == -1)
                    perror("sendfile");

                close(memfd);
                close(fd);
                return 1;
            }

            remaining -= n;
        }

        close(memfd);
        close(fd);

        printf("%s\n", "Successful!");
    }
    else if (strcmp(argv[1], "--list") == 0) {
        struct header_posix_ustar hpu;

        int fd = open(argv[0], O_RDONLY);
        if (fd == -1) {
            perror("open");
            return 1;
        }

        if (lseek(fd, CAST_HEADER_FREE, SEEK_SET) == (off_t)-1) {
            perror("lseek");
            close(fd);
            return 1;
        }

        while (read(fd, &hpu, sizeof(hpu)) == sizeof(hpu)) {
            if (memcmp(hpu.magic, "ustar", 5) != 0)
                break;

            unsigned long long file_size =
                strtoull(hpu.size, NULL, 8);

            printf("%s\n", hpu.name);

            off_t skip =
                (off_t)((file_size + 511) / 512) * 512;

            if (lseek(fd, skip, SEEK_CUR) == (off_t)-1) {
                perror("lseek");
                close(fd);
                return 1;
            }
        }

        close(fd);
    }
    else if (strcmp(argv[1], "--install") == 0 || strcmp(argv[1], "--update") == 0) {

        int fd = open(argv[0], O_RDONLY);
        if (fd == -1) {
            perror("open");
            return 1;
        }

        int result = 1;

        int *memfd_list = NULL;
        size_t memfd_list_size = 0;

        char *manifest = NULL;
        size_t manifest_size = 0;

        char **fd_paths = NULL;

        struct header_posix_ustar hpu;

        /*
         * First pass:
         * Find and read InstallManifest.
         */
        if (lseek(fd, CAST_HEADER_FREE, SEEK_SET) == (off_t)-1) {
            perror("lseek");
            goto cleanup;
        }

        while (read(fd, &hpu, sizeof(hpu)) == sizeof(hpu)) {
            if (memcmp(hpu.magic, "ustar", 5) != 0)
                break;

            unsigned long long file_size =
                strtoull(hpu.size, NULL, 8);

            size_t name_len =
                strnlen(hpu.name, sizeof(hpu.name));

            if (
                name_len == sizeof("InstallManifest") - 1 &&
                memcmp(
                    hpu.name,
                    "InstallManifest",
                    sizeof("InstallManifest") - 1
                ) == 0
            ) {
                manifest = malloc((size_t)file_size + 1);

                if (manifest == NULL)
                    goto cleanup;

                size_t total_read = 0;

                while (total_read < (size_t)file_size) {
                    ssize_t n = read(
                        fd,
                        manifest + total_read,
                        (size_t)file_size - total_read
                    );

                    if (n == -1) {
                        perror("read");
                        goto cleanup;
                    }

                    if (n == 0)
                        goto cleanup;

                    total_read += (size_t)n;
                }

                manifest[total_read] = '\0';
                manifest_size = total_read;

                break;
            }

            off_t skip =
                (off_t)(((file_size + 511) / 512) * 512);

            if (lseek(fd, skip, SEEK_CUR) == (off_t)-1) {
                perror("lseek");
                goto cleanup;
            }
        }

        if (manifest == NULL) {
            fprintf(stderr, "InstallManifest not found\n");
            goto cleanup;
        }

        /*
         * InstallManifest defines package order.
         *
         * For every manifest entry:
         *     1. Search TAR from the beginning.
         *     2. Find the corresponding package.
         *     3. Copy it into memfd.
         *     4. Append memfd in manifest order.
         */
        const char *line = manifest;
        const char *manifest_end =
            manifest + manifest_size;

        while (line < manifest_end) {
            const char *line_end = memchr(
                line,
                '\n',
                (size_t)(manifest_end - line)
            );

            if (line_end == NULL)
                line_end = manifest_end;

            size_t line_len =
                (size_t)(line_end - line);

            /*
             * Support CRLF manifests.
             */
            if (
                line_len > 0 &&
                line[line_len - 1] == '\r'
            ) {
                line_len--;
            }

            /*
             * Ignore empty lines.
             */
            if (line_len > 0) {
                bool found = false;

                if (
                    lseek(
                        fd,
                        CAST_HEADER_FREE,
                        SEEK_SET
                    ) == (off_t)-1
                ) {
                    perror("lseek");
                    goto cleanup;
                }

                while (
                    read(fd, &hpu, sizeof(hpu)) ==
                    sizeof(hpu)
                ) {
                    if (memcmp(hpu.magic, "ustar", 5) != 0)
                        break;

                    unsigned long long file_size =
                        strtoull(hpu.size, NULL, 8);

                    size_t name_len =
                        strnlen(
                            hpu.name,
                            sizeof(hpu.name)
                        );

                    if (
                        name_len == line_len &&
                        memcmp(
                            hpu.name,
                            line,
                            line_len
                        ) == 0
                    ) {
                        char memfd_name[
                            sizeof(hpu.name) + 1
                        ];

                        memcpy(
                            memfd_name,
                            hpu.name,
                            name_len
                        );

                        memfd_name[name_len] = '\0';

                        int memfd =
                            memfd_create(memfd_name, 0);

                        if (memfd == -1) {
                            perror("memfd_create");
                            goto cleanup;
                        }

                        off_t source_offset =
                            lseek(fd, 0, SEEK_CUR);

                        if (
                            source_offset ==
                            (off_t)-1
                        ) {
                            perror("lseek");
                            close(memfd);
                            goto cleanup;
                        }

                        size_t remaining =
                            (size_t)file_size;

                        while (remaining > 0) {
                            ssize_t n = sendfile(
                                memfd,
                                fd,
                                &source_offset,
                                remaining
                            );

                            if (n == -1) {
                                perror("sendfile");
                                close(memfd);
                                goto cleanup;
                            }

                            if (n == 0) {
                                close(memfd);
                                goto cleanup;
                            }

                            remaining -= (size_t)n;
                        }

                        if (
                            lseek(
                                memfd,
                                0,
                                SEEK_SET
                            ) == (off_t)-1
                        ) {
                            perror("lseek");
                            close(memfd);
                            goto cleanup;
                        }

                        int *new_list = realloc(
                            memfd_list,
                            (
                                memfd_list_size + 1
                            ) * sizeof(*memfd_list)
                        );

                        if (new_list == NULL) {
                            close(memfd);
                            goto cleanup;
                        }

                        memfd_list = new_list;

                        memfd_list[
                            memfd_list_size
                        ] = memfd;

                        memfd_list_size++;

                        found = true;
                        break;
                    }

                    off_t skip =
                        (off_t)(
                            ((file_size + 511) / 512) *
                            512
                        );

                    if (
                        lseek(
                            fd,
                            skip,
                            SEEK_CUR
                        ) == (off_t)-1
                    ) {
                        perror("lseek");
                        goto cleanup;
                    }
                }

                /*
                 * Manifest explicitly requested this package,
                 * therefore absence from TAR is an error.
                 */
                if (!found) {
                    fprintf(
                        stderr,
                        "InstallManifest entry not found in archive: %.*s\n",
                        (int)line_len,
                        line
                    );

                    goto cleanup;
                }
            }

            if (line_end == manifest_end)
                break;

            line = line_end + 1;
        }

        if (memfd_list_size == 0) {
            fprintf(
                stderr,
                "InstallManifest contains no packages\n"
            );

            goto cleanup;
        }

        /*
         * Build /proc/self/fd/... list.
         *
         * memfd_list already has exactly the same order
         * as InstallManifest.
         */
        fd_paths = calloc(
            memfd_list_size + 1,
            sizeof(*fd_paths)
        );

        if (fd_paths == NULL)
            goto cleanup;

        size_t fd_paths_size = 0;
        for (size_t i = 0; i < memfd_list_size; i++) {
            int length = snprintf(
                NULL,
                0,
                "/proc/self/fd/%d",
                memfd_list[i]
            );

            if (length < 0)
                goto cleanup;

            char *fd_path =
                malloc((size_t)length + 1);

            if (fd_path == NULL)
                goto cleanup;

            snprintf(
                fd_path,
                (size_t)length + 1,
                "/proc/self/fd/%d",
                memfd_list[i]
            );

            if (argc > 2) {
                struct PackageInfo *info = CATPKG_Parse(fd_path, NULL);

                CATPKG_ReadSections(info, 1);
                if (info == NULL) {
                    free(fd_path);
                    goto cleanup;
                }

                const struct PackageField *name_field =
                    PackageInfo_Find(
                        info,
                        "name"
                    );

                if (
                    name_field == NULL ||
                    name_field->value == NULL
                ) {
                    PackageInfo_Free(info);
                    free(fd_path);
                    goto cleanup;
                }

                bool found = false;

                for (int ai = 2; ai < argc; ai++) {
                    if (strcmp(name_field->value, argv[ai]) == 0) {
                        found = true;
                        break;
                    }
                }

                PackageInfo_Free(info);

                if (!found) {
                    free(fd_path);
                    continue;
                }
            }

            fd_paths[fd_paths_size++] = fd_path;
        }

        setenv("CATSTRAP-BIN", argv[0], 1);

        if(strcmp(argv[1], "--install") == 0) {
            result = catpkg_install(
                (const char **)fd_paths,
                fd_paths_size,
                false
            );
        }
        else
            result = catpkg_update(
                (const char **)fd_paths,
                fd_paths_size
            );

        cleanup:

        if (fd_paths != NULL) {
            for (size_t i = 0; i < memfd_list_size; i++)
                free(fd_paths[i]);

            free(fd_paths);
        }

        for (size_t i = 0; i < memfd_list_size; i++)
            close(memfd_list[i]);

        free(memfd_list);
        free(manifest);

        if (fd != -1)
            close(fd);

        return result;
    }
    else if(strcmp(argv[1], "--remove") == 0) {
        setenv("CATSTRAP-BIN", argv[0], 1);
        return catpkg_remove(
            (const char **) argv + 2,
            argc - 2,
            false
        );
    }
    else if(strcmp(argv[1], "--packages") == 0) {
        struct PackageMatches packages =
            catpkg_find_package("");

        for (
            size_t p_i = 0;
            p_i < packages.count;
            p_i++
        ) {
            struct PackageMatches versions =
                catpkg_find_version(
                    packages.items[p_i]
                );

            char package_fullname[256];

            snprintf(
                package_fullname,
                sizeof(package_fullname),
                "%s@%s",
                packages.items[p_i],
                versions.items[0]
            );

            struct PackageInfo *info = catpkg_find_package_info(package_fullname);
            CATPKG_ReadSections(info, 5);
            const struct PackageField *description_field = PackageInfo_Find(info, "description");
            const struct PackageField *dependency_field = PackageInfo_Find(info, "dependency");

            print_package_separator("", package_fullname);

            printf(
                "Name: %s\n",
                packages.items[p_i]
            );
            printf(
                "Version:"
            );


            for (
                size_t v_i = 0;
                v_i < versions.count;
                v_i++
            ) {

                printf(
                    " %s",
                    versions.items[v_i]
                );
            }


            printf(
                "\n"
            );

            printf("Description: ");
            if(description_field != NULL)
                printf("%s\n", description_field -> value);
            else
                printf("\n");

            printf("Dependencies: ");
            while(dependency_field != NULL && strcmp(dependency_field -> name, "dependency") == 0) {
                printf("%s ", dependency_field -> value);

                dependency_field = dependency_field -> next_field;
            }

            printf(
                "\n"
            );
            printf(
                "\n"
            );

            PackageMatches_Free(
                &versions
            );
            PackageInfo_Free(
                info
            );
        }

        PackageMatches_Free(
            &packages
        );
    }

    struct header_posix_ustar hpu;

    int fd = open(argv[0], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    if (lseek(fd, CAST_HEADER_FREE, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        close(fd);
        return 1;
    }

    while (read(fd, &hpu, sizeof(hpu)) == sizeof(hpu)) {
        if (memcmp(hpu.magic, "ustar", 5) != 0)
            break;

        unsigned long long file_size =
            strtoull(hpu.size, NULL, 8);

        if (hpu.typeflag[0] == '0' && strncmp("bin/", hpu.name, 4) == 0 && strcmp(hpu.name + 4, argv[1]) == 0) {
            int memfd =
                memfd_create(hpu.name, 0);
        
            sendfile(memfd, fd, NULL, file_size);

            fexecve(
                memfd,
                argv + 1,
                environ
            );
        }

        off_t skip =
            (off_t)((file_size + 511) / 512) * 512;

        if (lseek(fd, skip, SEEK_CUR) == (off_t)-1) {
            perror("lseek");
            close(fd);
            return 1;
        }
    }

    close(fd);

	return 0;
}