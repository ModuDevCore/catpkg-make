#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "configuration.h"
#include "utils/fs.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "verify.h"
#include "database.h"

int catpkg_integrity_catpackage(
    const char *package_path,
    char *hex
) {
    struct sha256_buff hash;
    catpkg_sign_catpackage(package_path, &hash);
    
    char hex_sign[65];

    sha256_read_hex(&hash, hex_sign);
    
    hex_sign[64] = '\0';

    if(strcmp(hex, hex_sign) == 0)
        return 1;
    else
        return 0;
}
int tar_header_is_empty(
    const char *header
)
{
    for (size_t i = 0; i < 512; i++) {

        if (header[i] != '\0')
            return 0;
    }

    return 1;
}
int catpkg_sign_catpackage(
    const char *package_path,
    struct sha256_buff *hash
)
{
    if (package_path == NULL ||
        hash == NULL) {

        return 1;
    }


    FILE *file =
        fopen(
            package_path,
            "rb"
        );

    if (file == NULL) {

        perror(
            "catpkg: failed to open package"
        );

        return 1;
    }


    char header[512];


    sha256_init(hash);


    while (fread(
               header,
               1,
               sizeof(header),
               file
           ) == sizeof(header)) {

        /*
         * TAR end-of-archive marker.
         *
         * TAR ends with two 512-byte blocks
         * filled with zero bytes.
         */
        if (tar_header_is_empty(header))
            break;

        /*
         * TAR header:
         *
         * name:  0-99
         * size:  124-135
         */

        char *name =
            header + 0;

        char *size_str =
            header + 124;


        size_t size =
            (size_t)strtoull(
                size_str,
                NULL,
                8
            );


        size_t padding =
            (512 - (size % 512)) % 512;


        char *data = NULL;


        if (size > 0) {

            data =
                malloc(size);

            if (data == NULL) {

                perror(
                    "catpkg: malloc"
                );

                fclose(file);

                return 1;
            }


            if (fread(
                    data,
                    1,
                    size,
                    file
                ) != size) {

                fprintf(
                    stderr,
                    "catpkg: failed to read TAR entry data\n"
                );

                free(data);
                fclose(file);

                return 1;
            }

        } else {

            /*
             * No data for directories,
             * symlinks, etc.
             */
            data = NULL;
        }


        /*
         * Skip TAR padding.
         */
        if (fseek(
                file,
                (long)padding,
                SEEK_CUR
            ) != 0) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            free(data);
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
            catpkg_normalize_tar_path(
                name
            );


        if (correct_path == NULL) {

            fprintf(
                stderr,
                "catpkg: failed to normalize TAR path\n"
            );

            free(data);
            fclose(file);

            return 1;
        }


        /*
         * PACKAGEINFO is deliberately excluded
         * from the package hash.
         */
        if (strcmp(
                correct_path,
                CATPKG_PACKAGEINFO
            ) != 0) {

            sha256_update(
                hash,
                "[PATH]",
                strlen("[PATH]")
            );

            sha256_update(
                hash,
                correct_path,
                strlen(correct_path)
            );

            sha256_update(
                hash,
                "[SIZE]",
                strlen("[SIZE]")
            );

            sha256_update(
                hash,
                size_str,
                12
            );

            sha256_update(
                hash,
                "[DATA]",
                strlen("[DATA]")
            );

            if (size > 0) {
                sha256_update(
                    hash,
                    data,
                    size
                );
            }
        }


        free(correct_path);
        free(data);
    }


    if (ferror(file)) {

        fprintf(
            stderr,
            "catpkg: failed while reading TAR archive\n"
        );

        fclose(file);

        return 1;
    }


    sha256_finalize(hash);


    fclose(file);

    return 0;
}

int catpkg_verify(
    const char *package_name
)
{
    /*
     * Find package by name.
     */
    struct PackageMatches matches =
        catpkg_find_package(package_name);

    if (matches.count > 1) {
        printf("The name has more than one match:\n");

        for (size_t i = 0; i < matches.count; i++) {
            printf("%s\n", matches.items[i]);
        }

        PackageMatches_Free(&matches);
        return 1;
    }

    if (matches.count == 0) {
        printf("None of the packages match the name.\n");

        PackageMatches_Free(&matches);
        return 1;
    }

    /*
     * Find installed versions of the package.
     */
    struct PackageMatches versions =
        catpkg_find_version(matches.items[0]);

    if (versions.count == 0) {
        printf(
            "No installed versions of the package were found.\n"
        );

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);
        return 1;
    }

    if (versions.count > 1) {
        printf(
            "Several versions of \"%s\" have been found:\n",
            matches.items[0]
        );

        for (size_t i = 0; i < versions.count; i++) {
            printf(
                "  %s\n",
                versions.items[i]
            );
        }

        printf(
            "Please specify the full package name "
            "(name@version).\n"
        );

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);
        return 1;
    }

    /*
     * Build:
     *
     * package_name@version
     */
    char *package_fullname = make_catpkg_path(
        "%s@%s",
        matches.items[0],
        versions.items[0]
    );

    if (package_fullname == NULL) {
        perror("catpkg: make_catpkg_path");

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);

        return 1;
    }

    /*
     * Build package file metadata path.
     */
    char *catpkg_files_path = make_catpkg_path(
        CATPKG_FILES_PATH,
        package_fullname
    );

    if (catpkg_files_path == NULL) {
        perror("catpkg: make_catpkg_path");

        free(package_fullname);

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);

        return 1;
    }

    printf(
        "Checking the \"%s\" package\n",
        package_fullname
    );

    struct PackageInfo **files_info = NULL;
    size_t files_info_count = 0;

    /*
     * Read information about every file in the package.
     *
     * Files are expected to be numbered:
     *
     * 0
     * 1
     * 2
     * ...
     */
    for (size_t i = 0;; i++) {

        char file_path[PATH_MAX];

        int written = snprintf(
            file_path,
            sizeof(file_path),
            "%s/%zu",
            catpkg_files_path,
            i
        );

        if (written < 0 ||
            (size_t)written >= sizeof(file_path)) {

            fprintf(
                stderr,
                "catpkg: file path is too long\n"
            );

            for (size_t j = 0; j < files_info_count; j++)
                PackageInfo_Free(files_info[j]);

            free(files_info);
            free(catpkg_files_path);
            free(package_fullname);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }

        FILE *file = fopen(
            file_path,
            "r"
        );

        if (file == NULL) {

            if (errno == ENOENT)
                break;

            perror("catpkg: fopen");

            for (size_t j = 0; j < files_info_count; j++)
                PackageInfo_Free(files_info[j]);

            free(files_info);
            free(catpkg_files_path);
            free(package_fullname);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }

        fclose(file);

        struct PackageInfo **tmp = realloc(
            files_info,
            (files_info_count + 1) *
            sizeof(*files_info)
        );

        if (tmp == NULL) {
            perror("catpkg: realloc");

            for (size_t j = 0; j < files_info_count; j++)
                PackageInfo_Free(files_info[j]);

            free(files_info);
            free(catpkg_files_path);
            free(package_fullname);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }

        files_info = tmp;

        files_info[files_info_count] = CATPKG_Parse(file_path, NULL);
        CATPKG_ReadSections(files_info[files_info_count], 2);

        if (files_info[files_info_count] == NULL) {
            fprintf(
                stderr,
                "catpkg: failed to parse \"%s\"\n",
                file_path
            );

            for (size_t j = 0; j < files_info_count; j++)
                PackageInfo_Free(files_info[j]);

            free(files_info);
            free(catpkg_files_path);
            free(package_fullname);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }

        files_info_count++;
    }

    printf(
        "Found %zu file information records.\n",
        files_info_count
    );

    /*
     * Verify files.
     */
    size_t missing = 0;

    for (size_t i = 0; i < files_info_count; i++) {

        const struct PackageField *path_field =
            PackageInfo_Find(
                files_info[i],
                "path"
            );

        if (path_field == NULL ||
            path_field->value == NULL) {

            fprintf(
                stderr,
                "catpkg: FILEINFO has no \"path\" field\n"
            );

            PackageInfo_Free(files_info[i]);
            continue;
        }

        if (access(path_field->value, F_OK) != 0) {

            if (errno == ENOENT) {
                printf(
                    "Warning: \"%s\" file not found.\n",
                    path_field->value
                );

                missing++;
            }
            else {
                fprintf(
                    stderr,
                    "catpkg: cannot access \"%s\": ",
                    path_field->value
                );

                perror(NULL);
            }
        }

        PackageInfo_Free(files_info[i]);
    }

    /*
     * Verification result.
     */
    if (missing > 0) {

        printf(
            "A discrepancy has been detected:\n"
            "Missing files: %zu\n",
            missing
        );

    } else {

        printf(
            "The package check was successful, "
            "no issues were found!\n"
            "Missing files: %zu\n",
            missing
        );
    }

    free(files_info);
    free(catpkg_files_path);
    free(package_fullname);

    PackageMatches_Free(&versions);
    PackageMatches_Free(&matches);

    return 0;
}