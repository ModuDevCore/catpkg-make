#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"
#include "remove.h"
#include "database.h"
#include "utils/allow.h"
#include "utils/sha256.h"
#include "utils/fs.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "catpkg/builder.h"

#define REMOVE_VAL(name) remove_protocols_##name
#define INSTALL_VAL(name) install_protocols_##name

#define BUILDER_PROGRESS_INFO_TITLE "UPDATE PACKAGES"
#define BUILDER_PROGRESS_PACKAGES_COUNT update_packages_count
#define BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT updated_packages_count

int catpkg_update(
    const char **paths,
    const size_t paths_count
)
{
    bool builder_initialized = false;

    struct PackageInfo *database_fileinfo = NULL;

    struct PackageInfo *INSTALL_VAL(info) = NULL;
    const struct PackageField *INSTALL_VAL(name_field) = NULL;
    const struct PackageField *INSTALL_VAL(version_field) = NULL;
    const struct PackageField *INSTALL_VAL(sha256_field) = NULL;

    char *INSTALL_VAL(metadata_path) = NULL;
    char *INSTALL_VAL(files_path) = NULL;
    char *INSTALL_VAL(fileinfo_path) = NULL;
    char *INSTALL_VAL(package_path) = NULL;
    char *INSTALL_VAL(package_fullname) = NULL;
    char *INSTALL_VAL(package_info_path) = NULL;
    char *INSTALL_VAL(package_commands_path) = NULL;

    struct PackageInfo *REMOVE_VAL(info) = NULL;
    struct PackageInfo *REMOVE_VAL(fileinfo) = NULL;
    const struct PackageField *REMOVE_VAL(name_field) = NULL;
    const struct PackageField *REMOVE_VAL(version_field) = NULL;
    const struct PackageField *REMOVE_VAL(sha256_field) = NULL;

    char *REMOVE_VAL(path) = NULL;
    char *REMOVE_VAL(fileinfo_path) = NULL;
    char *REMOVE_VAL(package_fullname) = NULL;

    struct CatpkgBuilder builder = {0};

    /*
     * --------------------------------------------------------
     * Initialize Builder
     * --------------------------------------------------------
     */

    if (catpkg_builder_init(&builder) != 0)
        goto cleanup;

    struct stat st;



    builder_initialized = true;

    size_t update_packages_count = paths_count;
    size_t updated_packages_count = 0;

    size_t total_package_size = 0;

    char packages_for_updating[1024] = "";
    
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


    for(; updated_packages_count < update_packages_count; updated_packages_count++) {
        const char *path = paths[updated_packages_count];
        
        if (path == NULL) {
            fprintf(stderr, "catpkg: package path is NULL\n");
            goto cleanup;
        }

        if (stat(path, &st) != 0) {
            perror("stat");
            goto cleanup;
        }

        total_package_size += st.st_size;

        /*
         * --------------------------------------------------------
         * Validate package
         * --------------------------------------------------------
         */

        if (path == NULL) {

            fprintf(
                stderr,
                "catpkg: package path is NULL\n"
            );

            return 1;
        }

        if (!is_catpackage(path)) {

            fprintf(
                stderr,
                "catpkg: \"%s\" is not a .catpackage file\n",
                path
            );

            return 1;
        }


        /*
         * --------------------------------------------------------
         * Parse database PACKAGEINFO
         * --------------------------------------------------------
         */


        database_fileinfo =
            CATPKG_Parse(
                CATPKG_DATABASE_DIR_PATH "/" CATPKG_DATABASE,
                NULL
            );
        

        if (database_fileinfo == NULL)
            goto cleanup;

        /*
         * --------------------------------------------------------
         * Parse INSTALL PACKAGEINFO
         * --------------------------------------------------------
         */

        INSTALL_VAL(info) =
            CATPKG_Parse(path, NULL);

        CATPKG_ReadSections(INSTALL_VAL(info), 5);


        if (INSTALL_VAL(info) == NULL)
            goto cleanup;

        INSTALL_VAL(name_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "name"
            );

        INSTALL_VAL(version_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "version"
            );

        INSTALL_VAL(sha256_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "SHA"
            );

        if (
            INSTALL_VAL(name_field) == NULL ||
            INSTALL_VAL(version_field) == NULL ||
            INSTALL_VAL(sha256_field) == NULL
        )
            goto cleanup;

        /*
         * --------------------------------------------------------
         * INSTALL package fullname
         * --------------------------------------------------------
         */

        size_t INSTALL_VAL(package_fullname_size) =
            strlen(INSTALL_VAL(name_field)->value) +
            1 +
            strlen(INSTALL_VAL(version_field)->value) +
            1;

        INSTALL_VAL(package_fullname) =
            malloc(
                INSTALL_VAL(package_fullname_size)
            );

        if (INSTALL_VAL(package_fullname) == NULL)
            goto cleanup;

        snprintf(
            INSTALL_VAL(package_fullname),
            INSTALL_VAL(package_fullname_size),
            "%s@%s",
            INSTALL_VAL(name_field)->value,
            INSTALL_VAL(version_field)->value
        );

        /*
         * --------------------------------------------------------
         * Calculate INSTALL paths
         * --------------------------------------------------------
         */

        INSTALL_VAL(package_commands_path) =
            make_catpkg_path(
                CATPKG_PACKAGE_DATA
                "/.catpkg/commands",
                INSTALL_VAL(name_field) -> value
            );


        #define __PACKAGEINFO__ INSTALL_VAL(info)
        #define __PACKAGE_COMMANDS_PATH__ INSTALL_VAL(package_commands_path)
        #include "../common/INSTALL_POSTPROCESS_PACKAGEINFO.inc"
        #undef __PACKAGEINFO__
        #undef __PACKAGE_COMMANDS_PATH__

        INSTALL_VAL(name_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "name"
            );

        INSTALL_VAL(version_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "version"
            );

        INSTALL_VAL(sha256_field) =
            PackageInfo_Find(
                INSTALL_VAL(info),
                "SHA"
            );

        INSTALL_VAL(metadata_path) =
            make_catpkg_path(
                CATPKG_METADATA_PATH,
                INSTALL_VAL(package_fullname)
            );

        INSTALL_VAL(files_path) =
            make_catpkg_path(
                CATPKG_FILES_PATH,
                INSTALL_VAL(package_fullname)
            );

        INSTALL_VAL(fileinfo_path) =
            make_catpkg_path(
                CATPKG_FILES_PATH "/" CATPKG_FILE,
                INSTALL_VAL(package_fullname)
            );

        INSTALL_VAL(package_path) =
            make_catpkg_path(
                CATPKG_PACKAGE_PATH,
                INSTALL_VAL(package_fullname)
            );

        INSTALL_VAL(package_info_path) =
            make_catpkg_path(
                CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
                INSTALL_VAL(package_fullname)
            );


        if (
            INSTALL_VAL(metadata_path) == NULL ||
            INSTALL_VAL(files_path) == NULL ||
            INSTALL_VAL(fileinfo_path) == NULL ||
            INSTALL_VAL(package_path) == NULL ||
            INSTALL_VAL(name_field) == NULL ||
            INSTALL_VAL(version_field) == NULL ||
            INSTALL_VAL(sha256_field) == NULL
        )
            goto cleanup;

        /*
         * --------------------------------------------------------
         * Parse REMOVE PACKAGEINFO
         * --------------------------------------------------------
         */

        struct PackageMatches match_version = catpkg_find_version(INSTALL_VAL(name_field) -> value);

        if(match_version.count > 1 || match_version.count == 0) {
            fprintf(stderr, "Error: Version error — a mismatch in the number of versions was found; %u versions were found, but 1 is required.\n", match_version.count);
            goto cleanup;
            return 1;
        }

        REMOVE_VAL(package_fullname) = make_catpkg_path(
            "%s@%s",
            INSTALL_VAL(name_field) -> value,
            match_version.items[0]
        );

        PackageMatches_Free(&match_version);

        REMOVE_VAL(fileinfo_path) =
            make_catpkg_path(
                CATPKG_FILES_PATH "/" CATPKG_FILE,
                REMOVE_VAL(package_fullname)
            );

        REMOVE_VAL(path) = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            REMOVE_VAL(package_fullname)
        );

        REMOVE_VAL(info) =
            CATPKG_Parse(REMOVE_VAL(path), NULL);
        CATPKG_ReadSections(REMOVE_VAL(info), 5);

        REMOVE_VAL(fileinfo) =
            CATPKG_Parse(REMOVE_VAL(fileinfo_path), NULL);
        CATPKG_ReadAll(REMOVE_VAL(fileinfo));

        if (REMOVE_VAL(info) == NULL)
            goto cleanup;

        REMOVE_VAL(name_field) =
            PackageInfo_Find(
                REMOVE_VAL(info),
                "name"
            );

        REMOVE_VAL(version_field) =
            PackageInfo_Find(
                REMOVE_VAL(info),
                "version"
            );

        REMOVE_VAL(sha256_field) =
            PackageInfo_Find(
                REMOVE_VAL(info),
                "SHA"
            );
        if (
            REMOVE_VAL(name_field) == NULL ||
            REMOVE_VAL(version_field) == NULL ||
            REMOVE_VAL(sha256_field) == NULL
        )
            goto cleanup;

        /*
         * --------------------------------------------------------
         * REMOVE package fullname
         * --------------------------------------------------------
         */

        size_t REMOVE_VAL(package_fullname_size) =
            strlen(REMOVE_VAL(name_field)->value) +
            1 +
            strlen(REMOVE_VAL(version_field)->value) +
            1;

        REMOVE_VAL(package_fullname) =
            malloc(
                REMOVE_VAL(package_fullname_size)
            );

        if (REMOVE_VAL(package_fullname) == NULL)
            goto cleanup;

        snprintf(
            REMOVE_VAL(package_fullname),
            REMOVE_VAL(package_fullname_size),
            "%s@%s",
            REMOVE_VAL(name_field)->value,
            REMOVE_VAL(version_field)->value
        );

        #define CATPKG_TRANSACTION_UPDATE
        /*
         * --------------------------------------------------------
         * Prepare REMOVE protocol
         * --------------------------------------------------------
         */

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
          
        char section_buffer[32];

        {
            struct PackageInfo *info =
                REMOVE_VAL(info);            
            struct PackageInfo *fileinfo =
                REMOVE_VAL(fileinfo);

            char *package_name =
                REMOVE_VAL(name_field)->value;
            char *package_fullname =
                REMOVE_VAL(package_fullname);

            #include "remove/BUILDER_REMOVE_PROTOCOLS.inc"
        }

        /*
         * --------------------------------------------------------
         * Prepare INSTALL protocol
         * --------------------------------------------------------
         */

        {
            struct PackageInfo *info =
                INSTALL_VAL(info);

            char *package_fullname =
                INSTALL_VAL(package_fullname);

            char *metadata_path =
                INSTALL_VAL(metadata_path);

            char *files_path =
                INSTALL_VAL(files_path);

            char *fileinfo_path =
                INSTALL_VAL(fileinfo_path);

            char *package_path =
                INSTALL_VAL(package_path);

            char *package_info_path =
                INSTALL_VAL(package_info_path);

            char *package_commands_path =
                INSTALL_VAL(package_commands_path);

            #include "install/BUILDER_INSTALL_PROTOCOLS.inc"

            prepare_error:
        }
        if (
            catpkg_builder_request(
                &builder,
                OPSEC_CHECK,
                "2"
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
            OPSEC_CHECK_FILEINFO_CONTEXT_UPDATE,
            NULL
        );

        catpkg_builder_request(
            &builder,
            CATPKG_CREATE_FILEINFO,
            INSTALL_VAL(fileinfo_path)
        );

        catpkg_builder_request(
            &builder,
            CTPG_CLOSE,
            NULL
        );

        if (updated_packages_count > 0)
            strcat(packages_for_updating, " ");

        strcat(packages_for_updating, INSTALL_VAL(package_fullname));
    }

    char package_proggress_info_buffer[2048];

    sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
        INSTALL_VAL(package_fullname), updated_packages_count, update_packages_count,
        "Successful!"
    );

    catpkg_builder_request_after_free(
        &builder,
        SET_PROGRESS_INFO,
        strdup(package_proggress_info_buffer) 
    );
    #undef CATPKG_TRANSACTION_UPDATE


    /*
     * --------------------------------------------------------
     * Calculate update size
     * --------------------------------------------------------
     */

    char package_size[64] = "-";
    char will_be_installed[64] = "-";
    char changing[64] = "-";
    char sign = '\0';
    
    catpkg_format_size(
        total_package_size,
        package_size,
        sizeof(package_size)
    );
    

    catpkg_format_size(
        builder.change_size,
        will_be_installed,
        sizeof(will_be_installed)
    );

    printf(
        ALLOW_UPDATE,
        packages_for_updating,
        package_size,
        will_be_installed
    );

    int answer = 0;

    if ((answer = catpkg_confirm()) == EOF){
        printf("\033[1A\033[%uC", strlen(ALLOW_UPDATE_ASK));
        printf("[x]\n");
        goto cleanup;
    }

    printf("\033[1A\033[%uC", strlen(ALLOW_UPDATE_ASK));
    printf("[+]\n");

    catpkg_builder_request(
        &builder,
        BUILD_LOG_END,
        NULL
    );

    /*
     * --------------------------------------------------------
     * Apply
     * --------------------------------------------------------
     */

    printf("Starting the update of the \"%s\" package...\n", INSTALL_VAL(package_fullname));
    if (catpkg_builder_apply(&builder) != 0) {
        /*
         * Builder only reverts requests which were successfully
         * applied, according to applied_count.
         */

        printf("\nERROR Recovery: Reverting the changes...\n");
        catpkg_builder_revert(&builder);

        catpkg_builder_err_print(&builder);
    
        goto error;
    }


    /*
     * --------------------------------------------------------
     * Success cleanup
     * --------------------------------------------------------
     */

    if (builder_initialized)
        catpkg_builder_free(&builder);

    printf("The package “%s” has been updated!\n", INSTALL_VAL(package_fullname));

    PackageInfo_Free(INSTALL_VAL(info));
    PackageInfo_Free(REMOVE_VAL(info));
    PackageInfo_Free(database_fileinfo);

    free(INSTALL_VAL(metadata_path));
    free(INSTALL_VAL(files_path));
    free(INSTALL_VAL(fileinfo_path));
    free(INSTALL_VAL(package_path));
    free(INSTALL_VAL(package_info_path));
    free(INSTALL_VAL(package_fullname));
    free(REMOVE_VAL(package_fullname));

    printf("Continued.\n");

    return 0;

error:
    printf("\nAn error occurred during the package installation.\n");
    goto cleanup;

    return 1;

cleanup:

    /*
     * Builder may contain pointers into PackageInfo.
     * Therefore it must be destroyed first.
     */

    if (builder_initialized)
        catpkg_builder_free(&builder);

    PackageInfo_Free(INSTALL_VAL(info));
    PackageInfo_Free(REMOVE_VAL(info));
    PackageInfo_Free(database_fileinfo);

    free(INSTALL_VAL(metadata_path));
    free(INSTALL_VAL(files_path));
    free(INSTALL_VAL(fileinfo_path));
    free(INSTALL_VAL(package_path));
    free(INSTALL_VAL(package_info_path));
    free(INSTALL_VAL(package_fullname));
    free(INSTALL_VAL(package_commands_path));
    free(REMOVE_VAL(package_fullname));
    free(REMOVE_VAL(path));
    
    printf("Continued.\n");

    return 1;
    
}
#undef BUILDER_PROGRESS_INFO_TITLE