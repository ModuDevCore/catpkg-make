#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <ftw.h>
#include <stdint.h>
#include <stdbool.h>

#include "database.h"
#include "catpkg/builder.h"
#include "utils/fs.h"
#include "utils/allow.h"
#include "configuration.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "catpkg/cache.h"

#define BUILDER_PROGRESS_INFO_TITLE "REMOVE PACKAGES"
#define BUILDER_PROGRESS_PACKAGES_COUNT remove_packages_count
#define BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT removed_packages_count

struct CatpkgRemovePackage {
    struct PackageInfo *info;
    struct PackageInfo *fileinfo;

    struct PackageMatches matches;
    struct PackageMatches versions;

    bool matches_initialized;
    bool versions_initialized;

    char *selected_version;
    char *package_fullname;
    char *catpkg_packageinfo_path;
    char *catpkg_fileinfo_path;
};

int catpkg_remove(
    const char **package_names,
    const size_t package_names_count,
    bool assume_yes
)
{
    update_database_package_ids();

    (void)assume_yes;

    bool builder_initialized = false;
    int result = 1;

    size_t remove_packages_count =
        package_names_count;

    size_t removed_packages_count =
        0;

    struct PackageInfo *database_fileinfo =
        NULL;

    struct CatpkgBuilder builder = {0};

    struct CatpkgRemovePackage *packages =
        calloc(
            package_names_count,
            sizeof(*packages)
        );

    if (
        packages == NULL &&
        package_names_count > 0
    )
        goto cleanup;

    if (package_names_count == 0)
        goto cleanup;

    database_fileinfo = CATPKG_Parse(
        CATPKG_DATABASE_DIR_PATH
        "/"
        CATPKG_DATABASE,
        NULL
    );
    
    CATPKG_ReadAll(database_fileinfo);

    if (database_fileinfo == NULL)
        goto cleanup;

    if (
        catpkg_builder_init(
            &builder
        ) != 0
    )
        goto cleanup;

    builder_initialized = true;
    bool verbose =
        getenv(
            "CATPKG_VERBOSE"
        ) != NULL;

    if(verbose)
        catpkg_builder_request(
            &builder,
            BUILD_LOG_START,
            NULL
        );

    char packages_for_removing[1024] = "";

    struct CatpkgRemovePackage *package = NULL;


    for (
        ;
        removed_packages_count <
            remove_packages_count;
        removed_packages_count++
    ) {
        const char *package_name =
            package_names[
                removed_packages_count
            ];

        package =
            &packages[
                removed_packages_count
            ];

        char section_buffer[32];

        if (package_name == NULL) {
            fprintf(
                stderr,
                "catpkg: package name is NULL\n"
            );

            goto cleanup;
        }

        package->matches =
            catpkg_find_package(
                package_name
            );

        package->matches_initialized =
            true;

        if (package->matches.count == 0) {
            printf(
                "None of the packages match the name \"%s\".\n",
                package_name
            );

            goto cleanup;
        }

        package->versions =
            catpkg_find_version(
                package_name
            );

        package->versions_initialized =
            true;

        if (package->versions.count == 0) {
            printf(
                "No installed versions of \"%s\" were found.\n",
                package_name
            );

            goto cleanup;
        }

        if (package->versions.count == 1) {
            package->selected_version =
                strdup(
                    package->versions.items[0]
                );

            if (
                package->selected_version ==
                NULL
            ) {
                perror(
                    "catpkg: strdup"
                );

                goto cleanup;
            }
        }
        else {
            printf(
                "Several versions of \"%s\" have been found.\n",
                package_name
            );

            printf(
                "Available versions:\n"
            );

            for (
                size_t i = 0;
                i < package->versions.count;
                i++
            ) {
                printf(
                    "  %s\n",
                    package->versions.items[i]
                );
            }

            printf(
                "Enter the version to remove: "
            );

            fflush(
                stdout
            );

            char input[256];

            if (
                fgets(
                    input,
                    sizeof(input),
                    stdin
                ) == NULL
            ) {
                fprintf(
                    stderr,
                    "catpkg: failed to read version\n"
                );

                goto cleanup;
            }

            input[
                strcspn(
                    input,
                    "\n"
                )
            ] = '\0';

            input[
                strcspn(
                    input,
                    "\r"
                )
            ] = '\0';

            if (input[0] == '\0') {
                fprintf(
                    stderr,
                    "catpkg: version cannot be empty\n"
                );

                goto cleanup;
            }

            for (
                size_t i = 0;
                i < package->versions.count;
                i++
            ) {
                if (
                    strcmp(
                        input,
                        package->versions.items[i]
                    ) != 0
                )
                    continue;

                package->selected_version =
                    strdup(
                        package->versions.items[i]
                    );

                if (
                    package->selected_version ==
                    NULL
                ) {
                    perror(
                        "catpkg: strdup"
                    );

                    goto cleanup;
                }

                break;
            }

            if (
                package->selected_version ==
                NULL
            ) {
                fprintf(
                    stderr,
                    "catpkg: version \"%s\" was not found\n",
                    input
                );

                goto cleanup;
            }
        }

        package->package_fullname =
            make_catpkg_path(
                "%s@%s",
                package_name,
                package->selected_version
            );

        if (
            package->package_fullname ==
            NULL
        )
            goto cleanup;

        package->catpkg_packageinfo_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH
                "/"
                CATPKG_PACKAGEINFO,
                package->package_fullname
            );

        if (
            package->catpkg_packageinfo_path ==
            NULL
        )
            goto cleanup;

        package->catpkg_fileinfo_path =
            make_catpkg_path(
                CATPKG_FILES_PATH
                "/"
                CATPKG_FILE,
                package->package_fullname
            );

        if (
            package->catpkg_fileinfo_path ==
            NULL
        )
            goto cleanup;

        package->info = CATPKG_Parse(
                package->catpkg_packageinfo_path,
                NULL
            );

        CATPKG_ReadSections(package->info, 5);

        package->fileinfo = CATPKG_Parse(
                package->catpkg_fileinfo_path,
                NULL
            );
        CATPKG_ReadAll(package->fileinfo);

        if (package->info == NULL)
            goto cleanup;

        if (
            catpkg_builder_request(
                &builder,
                OPSEC_CREATE,
                package->catpkg_packageinfo_path
            ) != 0
        ) {
            return 1;
        }

        {
            struct PackageInfo *info =
                package->info;

            struct PackageInfo *fileinfo =
                package->fileinfo;

            char *package_name =
                (char *)package_names[
                    removed_packages_count
                ];

            char *package_fullname =
                package->package_fullname;

            #include "remove/BUILDER_REMOVE_PROTOCOLS.inc"
        }

        if (
            removed_packages_count > 0
        ) {
            if (
                strlen(packages_for_removing) +
                1 <
                sizeof(packages_for_removing)
            )
                strcat(
                    packages_for_removing,
                    " "
                );
        }

        size_t current_length =
            strlen(
                packages_for_removing
            );

        size_t fullname_length =
            strlen(
                package->package_fullname
            );

        if (
            current_length +
            fullname_length +
            1 >
            sizeof(packages_for_removing)
        ) {
            fprintf(
                stderr,
                "catpkg: package list is too long\n"
            );

            goto cleanup;
        }

        strcat(
            packages_for_removing,
            package->package_fullname
        );

        if (
            catpkg_builder_request(
                &builder,
                OPSEC_CHECK,
                "1"
            ) != 0
        ) {
            return 1;
        }
    }

    catpkg_builder_request(
        &builder,
        CATPKG_MERGE_DATABASE,
        CATPKG_DATABASE_DIR_PATH
        "/"
        CATPKG_DATABASE
    );

    char package_proggress_info_buffer[2048];

    sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
        package->package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
        "Successful!"
    );

    catpkg_builder_request_after_free(
        &builder,
        SET_PROGRESS_INFO,
        strdup(package_proggress_info_buffer) 
    );


    catpkg_builder_request(
        &builder,
        BUILD_LOG_END,
        NULL
    );

    char will_be_removed[64] = "-";

    size_t absolute =
        (size_t)(
            builder.change_size < 0
                ? -builder.change_size
                : builder.change_size
        );

    catpkg_format_size(
        absolute,
        will_be_removed,
        sizeof(will_be_removed)
    );

    printf(
        ALLOW_REMOVE,
        packages_for_removing,
        will_be_removed
    );

    int answer = 0;

    if ((answer = catpkg_confirm()) == EOF){
        printf("\033[1A\033[%uC", strlen(ALLOW_REMOVE_ASK));
        printf("[x]\n");
        goto cleanup;
    }

    printf("\033[1A\033[%uC", strlen(ALLOW_REMOVE_ASK));
    printf("[+]\n");

    printf(
        "Removing packages “%s”...\n",
        packages_for_removing
    );

    if (
        catpkg_builder_apply(
            &builder
        ) != 0
    ) {
        printf("\nERROR Recovery: Reverting the changes...\n");
        catpkg_builder_revert(&builder);

        printf("\n");
        
        catpkg_builder_err_print(&builder);

        printf(
            "\nFailed to delete the packages, try deleting the packages "
            "manually or resolve the error.\n"
            " We recommend: Try installing the packages again and then "
            "deleting them.\n"
        );

        goto cleanup;
    }

    printf(
        "Packages “%s” have been removed!\n",
        packages_for_removing
    );

    result = 0;

cleanup:

    if (builder_initialized)
        catpkg_builder_free(
            &builder
        );

    PackageInfo_Free(
        database_fileinfo
    );

    if (packages != NULL) {
        for (
            size_t i = 0;
            i < package_names_count;
            i++
        ) {
            PackageInfo_Free(
                packages[i].info
            );

            if (
                packages[i].versions_initialized
            ) {
                PackageMatches_Free(
                    &packages[i].versions
                );
            }

            if (
                packages[i].matches_initialized
            ) {
                PackageMatches_Free(
                    &packages[i].matches
                );
            }

            free(
                packages[i].selected_version
            );

            free(
                packages[i].package_fullname
            );

            free(
                packages[i].catpkg_packageinfo_path
            );
        }

        free(
            packages
        );
    }

    printf(
        "Continued.\n"
    );

    return result;
}

#undef BUILDER_PROGRESS_INFO_TITLE
#undef BUILDER_PROGRESS_PACKAGES_COUNT
#undef BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT