#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>  

#include "configuration.h"

#include "catpkg/path.h"
#include "catpkg/cache.h"
#include "utils/fs.h"
#include "database.h"

// packageFullName format by - packageName@version

static int copy_file(
    const char *src,
    const char *packageFullName
)
{
    char *des = make_catpkg_path(
        CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE,
        packageFullName
    );

    if (des == NULL) {
        fprintf(
            stderr,
            "catpkg: failed to create destination path\n"
        );

        return 1;
    }


    int in = open(
        src,
        O_RDONLY
    );

    if (in == -1) {

        perror(
            "catpkg: open source"
        );

        free(des);

        return 1;
    }


    int out = open(
        des,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (out == -1) {

        perror(
            "catpkg: open destination"
        );

        close(in);
        free(des);

        return 1;
    }


    char buffer[65536];
    ssize_t bytes_read;


    while ((bytes_read = read(
                in,
                buffer,
                sizeof(buffer)
            )) > 0) {

        ssize_t total_written = 0;

        while (total_written < bytes_read) {

            ssize_t bytes_written = write(
                out,
                buffer + total_written,
                (size_t)(bytes_read - total_written)
            );

            if (bytes_written == -1) {

                perror(
                    "catpkg: write"
                );

                close(in);
                close(out);
                free(des);

                return 1;
            }

            total_written += bytes_written;
        }
    }


    if (bytes_read == -1) {

        perror(
            "catpkg: read"
        );

        close(in);
        close(out);
        free(des);

        return 1;
    }


    close(in);
    close(out);

    free(des);

    return 0;
}

int compress_zstd(
    const char *source,
    const char *packageFullName
)
{
    char *des = make_catpkg_path(
        CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE_ZSTD,
        packageFullName
    );

    if (des == NULL) {

        fprintf(
            stderr,
            "catpkg: failed to create destination path\n"
        );

        return 1;
    }


    pid_t pid = fork();

    if (pid == -1) {

        perror(
            "catpkg: fork"
        );

        free(des);

        return 1;
    }


    if (pid == 0) {

        execl(
            "/usr/bin/zstd",
            "zstd",
            "-3",
            "-f",
            "-o",
            des,
            source,
            (char *)NULL
        );

        perror(
            "catpkg: execl zstd"
        );

        _exit(1);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) == -1) {

        perror(
            "catpkg: waitpid"
        );

        free(des);

        return 1;
    }


    free(des);


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: zstd compression failed\n"
        );

        return 1;
    }


    return 0;
}


int catpkg_cache_package(
    const char *source,
    const char *packageFullName
)
{
    /*
     * Prefer zstd when available.
     */

    if (access(
            "/usr/bin/zstd",
            X_OK
        ) == 0) {

        if (compress_zstd(
                source,
                packageFullName
            ) == 0) {

            return 0;
        }


        /*
         * zstd is available, but compression failed.
         * Fall back to the original package.
         */

        fprintf(
            stderr,
            "catpkg: zstd compression failed, "
            "falling back to uncompressed package\n"
        );
    }


    /*
     * zstd is unavailable or compression failed.
     */

    return copy_file(
        source,
        packageFullName
    );
}
int catpkg_clear_package_cache(
    const char *packageName,
    const char *version
)
{
    char *package_fullname;

    if (version == NULL || version[0] == '\0') {

        package_fullname = make_catpkg_path(
            "%s@",
            packageName
        );

    } else {

        package_fullname = make_catpkg_path(
            "%s@%s",
            packageName,
            version
        );
    }

    if (package_fullname == NULL) {
        perror("catpkg: make_catpkg_path");
        return 1;
    }

    struct PackageMatches matches =
        catpkg_find_cached_package(package_fullname);

    free(package_fullname);

    if (matches.count == 0) {
        printf("Couldn't find the package cache.\n");

        PackageMatches_Free(&matches);
        return 1;
    }

    for (size_t i = 0; i < matches.count; i++) {

        /*
         * Remove the compressed package cache.
         */
        char *path = make_catpkg_path(
            CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE_ZSTD,
            matches.items[i]
        );

        if (path == NULL) {
            perror("catpkg: make_catpkg_path");
            continue;
        }

        if (remove(path) != 0 && errno != ENOENT) {
            perror("catpkg: remove");
        }

        free(path);

        /*
         * Remove the uncompressed package cache.
         */
        path = make_catpkg_path(
            CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE,
            matches.items[i]
        );

        if (path == NULL) {
            perror("catpkg: make_catpkg_path");
            continue;
        }

        if (remove(path) != 0 && errno != ENOENT) {
            perror("catpkg: remove");
        }

        free(path);
    }

    PackageMatches_Free(&matches);

    return 0;
}