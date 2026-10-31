#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdbool.h>

#include "configuration.h"
#include "install.h"
#include "update.h"
#include "verify.h"

#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "catpkg/builder.h"
#include "catpkg/cache.h"
#include "catpkg/command.h"
#include "utils/fs.h"
#include "utils/allow.h"
#include "utils/sha256.h"
#include "utils/postprocess.h"

#define BUILDER_PROGRESS_INFO_TITLE "INSTALL PACKAGES"
#define BUILDER_PROGRESS_PACKAGES_COUNT install_packages_count - packages_for_update_count
#define BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT installed_packages_count

struct CatpkgInstallPackage {
    struct PackageInfo *info;

    const char *source_path;
    char *package_fullname;
    char *metadata_path;
    char *files_path;
    char *fileinfo_path;
    char *package_path;
    char *package_info_path;
    char *package_commands_path;
};

int catpkg_install(
    const char **paths,
    const size_t paths_count,
    bool assume_yes
)
{
    catpkg_database_ensure();

    (void)assume_yes;

    bool builder_initialized = false;
    int result = 1;

    size_t install_packages_count = paths_count;
    size_t installed_packages_count = 0;
    size_t total_package_size = 0;

    size_t packages_for_install_count = 0;
    size_t packages_for_update_count = 0;

    struct CatpkgBuilder builder = {0};

    struct CatpkgInstallPackage *packages =
        calloc(
            paths_count,
            sizeof(*packages)
        );

    struct CatpkgInstallPackage **packages_for_install =
        calloc(
            paths_count,
            sizeof(*packages_for_install)
        );

    struct CatpkgInstallPackage **packages_for_update =
        calloc(
            paths_count,
            sizeof(*packages_for_update)
        );

    const char **update_paths = NULL;

    if (
        (packages == NULL ||
         packages_for_install == NULL ||
         packages_for_update == NULL) &&
        paths_count > 0
    )
        goto cleanup;

    if (paths_count == 0)
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

    for (
        size_t i = 0;
        i < paths_count;
        i++
    ) {
        const char *path = paths[i];

        struct CatpkgInstallPackage *package =
            &packages[i];

        if (path == NULL) {
            fprintf(
                stderr,
                "catpkg: package path is NULL\n"
            );

            goto cleanup;
        }

        package->source_path = path;

        if (!is_catpackage(path)) {
            fprintf(
                stderr,
                "catpkg: \"%s\" is not a .catpackage file\n",
                path
            );

            goto cleanup;
        }

        struct stat st;

        if (
            stat(
                path,
                &st
            ) != 0
        ) {
            perror("stat");
            goto cleanup;
        }

        total_package_size +=
            (size_t)st.st_size;

        package->info = CATPKG_Parse(path, NULL);
        CATPKG_ReadSections(package->info, 5);

        const struct PackageField *name_field =
            PackageInfo_Find(
                package->info,
                "name"
            );

        package->package_commands_path =
            make_catpkg_path(
                CATPKG_PACKAGE_DATA
                "/.catpkg/commands",
                name_field->value
            );

        #define __PACKAGEINFO__ package->info
        #define __PACKAGE_COMMANDS_PATH__ package->package_commands_path
        #include "../common/INSTALL_POSTPROCESS_PACKAGEINFO.inc"
        #undef __PACKAGEINFO__
        #undef __PACKAGE_COMMANDS_PATH__

        if (package->info == NULL)
            goto cleanup;


        const struct PackageField *version_field =
            PackageInfo_Find(
                package->info,
                "version"
            );

        const struct PackageField *sha256_field =
            PackageInfo_Find(
                package->info,
                "SHA"
            );

        if (
            name_field == NULL ||
            version_field == NULL ||
            sha256_field == NULL
        )
            goto cleanup;

        package->package_fullname =
            make_catpkg_path(
                "%s@%s",
                name_field->value,
                version_field->value
            );

        if (
            package->package_fullname == NULL ||
            package->package_commands_path == NULL
        )
            goto cleanup;

        {
            struct PackageInfo *info =
                package->info;

            char *package_commands_path =
                package->package_commands_path;

            #define __PACKAGEINFO__ info
            #define __PACKAGE_COMMANDS_PATH__ package_commands_path
            #include "../common/INSTALL_POSTPROCESS_PACKAGEINFO.inc"
            #undef __PACKAGEINFO__
            #undef __PACKAGE_COMMANDS_PATH__
            package->info = info;
        }

        if (package->info == NULL)
            goto cleanup;

        if (
            name_field == NULL ||
            version_field == NULL ||
            sha256_field == NULL
        )
            goto cleanup;

        package->metadata_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH,
                package->package_fullname
            );

        package->files_path =
            make_catpkg_path(
                CATPKG_FILES_PATH,
                package->package_fullname
            );

        package->fileinfo_path =
            make_catpkg_path(
                CATPKG_FILES_PATH
                "/"
                CATPKG_FILE,
                package->package_fullname
            );

        package->package_path =
            make_catpkg_path(
                CATPKG_PACKAGE_PATH,
                package->package_fullname
            );

        package->package_info_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH
                "/"
                CATPKG_PACKAGEINFO,
                package->package_fullname
            );

        if (
            package->metadata_path == NULL ||
            package->files_path == NULL ||
            package->fileinfo_path == NULL ||
            package->package_path == NULL ||
            package->package_info_path == NULL
        )
            goto cleanup;

        struct PackageMatches match_version =
            catpkg_find_package(
                name_field->value
            );

        if (match_version.count > 0) {
            packages_for_update[
                packages_for_update_count++
            ] = package;
        } else {
            packages_for_install[
                packages_for_install_count++
            ] = package;
        }

        PackageMatches_Free(
            &match_version
        );
    }

    for (size_t i = 0; i < packages_for_install_count; i++) {
        for (size_t j = i; j < packages_for_install_count; j++) {
            struct CatpkgInstallPackage *package =
                packages_for_install[j];

            const struct PackageField *dependency_field =
                PackageInfo_Find(
                    package->info,
                    "dependency"
                );

            while (
                dependency_field != NULL &&
                dependency_field->section == 6
            ) {
                for (
                    size_t check_i = j + 1;
                    check_i < packages_for_install_count;
                    check_i++
                ) {
                    struct CatpkgInstallPackage *check_package =
                        packages_for_install[check_i];

                    const struct PackageField *name_field =
                        PackageInfo_Find(
                            check_package->info,
                            "name"
                        );

                    if (
                        name_field != NULL &&
                        strcmp(
                            name_field->value,
                            dependency_field->value
                        ) == 0
                    ) {
                        struct CatpkgInstallPackage *tmp =
                            packages_for_install[j];

                        packages_for_install[j] =
                            packages_for_install[check_i];

                        packages_for_install[check_i] = tmp;

                        j--;
                        break;
                    }
                }

                dependency_field = dependency_field->next_field;
            }
        }
    }
    for (
        installed_packages_count = 0;
        installed_packages_count <
            packages_for_install_count;
        installed_packages_count++
    ) {

        struct CatpkgInstallPackage *package =
            packages_for_install[
                installed_packages_count
            ];

        const char *path =
            package->source_path;

        catpkg_builder_request_after_free(
            &builder,
            CTPG_OPEN,
            strdup(path)
        );

        if (
            catpkg_builder_request_after_free(
                &builder,
                OPSEC_CREATE,
                strdup(path)
            ) != 0
        ) {
            return 1;
        }

        struct PackageInfo *info =
            package->info;

        char *package_fullname =
            package->package_fullname;

        char *metadata_path =
            package->metadata_path;

        char *files_path =
            package->files_path;

        char *fileinfo_path =
            package->fileinfo_path;

        char *package_path =
            package->package_path;

        char *package_info_path =
            package->package_info_path;

        char *package_commands_path =
            package->package_commands_path;

        #include "install/BUILDER_INSTALL_PROTOCOLS.inc"

        if (
            catpkg_builder_request(
                &builder,
                OPSEC_CHECK,
                "4"
            ) != 0
        ) {
            return 1;
        }

        catpkg_builder_request(
            &builder,
            BUILD_LOG_SEP,
            "Generate FileInfo"
        );

        catpkg_builder_request_after_free(
            &builder,
            OPSEC_CHECK_FILEINFO_CONTEXT_CREATE,
            NULL
        );

        catpkg_builder_request(
            &builder,
            CATPKG_CREATE_FILEINFO,
            fileinfo_path
        );

        catpkg_builder_request(
            &builder,
            CTPG_CLOSE,
            NULL
        );
    }

    if (packages_for_install_count > 0) {
        struct CatpkgInstallPackage *package =
            packages_for_install[
                packages_for_install_count - 1
            ];

        char package_proggress_info_buffer[2048];

        sprintf(
            package_proggress_info_buffer,
            PACKAGE_PROGRESS_INFO_SHEME,
            BUILDER_PROGRESS_INFO_TITLE,
            package->package_fullname,
            BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT,
            BUILDER_PROGRESS_PACKAGES_COUNT,
            "Successful!"
        );

        catpkg_builder_request_after_free(
            &builder,
            SET_PROGRESS_INFO,
            strdup(
                package_proggress_info_buffer
            )
        );
    }

    catpkg_builder_request(
        &builder,
        CATPKG_MERGE_DATABASE,
        CATPKG_DATABASE_DIR_PATH "/" CATPKG_DATABASE
    );

    char package_size[64] = "-";
    char will_be_installed[64] = "-";
    char changing[64] = "-";

    catpkg_format_size(
        total_package_size,
        package_size,
        sizeof(package_size)
    );

    size_t absolute =
        (size_t)(
            builder.change_size < 0
                ? -builder.change_size
                : builder.change_size
        );

    catpkg_format_size(
        absolute,
        will_be_installed,
        sizeof(will_be_installed)
    );

    int64_t changing_size =
        (int64_t)builder.change_size -
        (int64_t)total_package_size;

    char sign =
        changing_size < 0
            ? '-'
            : '+';

    absolute =
        (size_t)(
            changing_size < 0
                ? -changing_size
                : changing_size
        );

    catpkg_format_size(
        absolute,
        changing,
        sizeof(changing)
    );

    char packages_for_installing[1024] = "";

    for (
        size_t i = 0;
        i < paths_count;
        i++
    ) {
        if (i > 0)
            strcat(
                packages_for_installing,
                " "
            );

        bool is_update = false;

        for (
            size_t ai = 0;
            ai < packages_for_update_count;
            ai++
        ) {
            if (
                packages_for_update[ai] ==
                &packages[i]
            ) {
                is_update = true;

                strcat(
                    packages_for_installing,
                    packages[i].package_fullname
                );

                strcat(
                    packages_for_installing,
                    " ( UPDATE )"
                );

                break;
            }
        }

        if (is_update)
            continue;

        strcat(
            packages_for_installing,
            packages[i].package_fullname
        );
    }

    catpkg_builder_request(
        &builder,
        BUILD_LOG_END,
        NULL
    );

    printf(
        ALLOW_INSTALLATION,
        packages_for_installing,
        package_size,
        will_be_installed,
        sign,
        changing
    );

    int answer = 0;

    if ((answer = catpkg_confirm()) == EOF){
        printf("\033[1A\033[%uC", strlen(ALLOW_INSTALLATION_ASK));
        printf("[x]\n");
        goto cleanup;
    }

    printf("\033[1A\033[%uC", strlen(ALLOW_INSTALLATION_ASK));
    printf("[+]\n");

    printf(
        "Starting the installation of packages \"%s\"...\n",
        packages_for_installing
    );

    if (packages_for_install_count > 0) {
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
                "\nAn error occurred during the package installation.\n"
            );

            goto cleanup;
        }
    }

    if (packages_for_update_count > 0) {
        update_paths =
            calloc(
                packages_for_update_count + 1,
                sizeof(*update_paths)
            );

        if (update_paths == NULL)
            goto cleanup;

        for (
            size_t i = 0;
            i < packages_for_update_count;
            i++
        ) {
            update_paths[i] =
                packages_for_update[i]->source_path;
        }

        update_paths[
            packages_for_update_count
        ] = NULL;

        result =
            catpkg_update(
                update_paths,
                packages_for_update_count
            );

        if (result != 0) {
            fprintf(
                stderr,
                "[ERROR] catpkg updating failed "
                "with exit code %d.\n",
                result
            );

            goto cleanup;
        }
    }

    printf(
        "Packages \"%s\" have been installed!\n",
        packages_for_installing
    );

    result = 0;

cleanup:

    if (builder_initialized)
        catpkg_builder_free(
            &builder
        );

    if (packages != NULL) {
        for (
            size_t i = 0;
            i < paths_count;
            i++
        ) {
            PackageInfo_Free(
                packages[i].info
            );

            free(
                packages[i].package_fullname
            );

            free(
                packages[i].metadata_path
            );

            free(
                packages[i].files_path
            );

            free(
                packages[i].fileinfo_path
            );

            free(
                packages[i].package_path
            );

            free(
                packages[i].package_info_path
            );

            free(
                packages[i].package_commands_path
            );
        }

        free(
            packages
        );
    }

    free(
        packages_for_install
    );

    free(
        packages_for_update
    );

    free(
        update_paths
    );

    printf(
        "Continued.\n"
    );

    return result;

prepare_error:
    goto cleanup;
}

#undef __POSTPROCESS_FILE_PATH_TMP__
#undef BUILDER_PROGRESS_INFO_TITLE
#undef BUILDER_PROGRESS_PACKAGES_COUNT
#undef BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT