#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "configuration.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "utils/fs.h"
#include "database.h"

#define DATABASE_PACKAGE_NONE_ID SIZE_MAX

char **database_package_ids = NULL;
size_t database_package_ids_size = 0;

int update_database_package_ids() {
    DIR *dir = opendir(CATPKG_PACKAGES_PATH);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return 1;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;

        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info = CATPKG_Parse(package_info_path, NULL);
        CATPKG_ReadSections(info, 1);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        if (name_field == NULL ||
            name_field->value == NULL) {
            PackageInfo_Free(info);
            continue;
        }

        char **new_items =
            realloc(
                database_package_ids,
                (database_package_ids_size + 1) * sizeof(*database_package_ids)
            );

        if (new_items == NULL)
            return ENOMEM;

        database_package_ids = new_items;

        database_package_ids[database_package_ids_size] =
            strdup(name_field -> value);

        if (database_package_ids[database_package_ids_size] == NULL)
            return ENOMEM;

        database_package_ids_size++;

        PackageInfo_Free(info);
    }

    closedir(dir);

    return 0;
}

static int ensure_empty_file(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0)
        return 0;

    if (errno != ENOENT) {
        perror("stat");
        return 1;
    }

    FILE *file = fopen(path, "w");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }

    return 0;
}

int catpkg_database_ensure(void)
{
    struct stat st;

    /*
     * Ensure /var/lib/catpackage
     */
    if (stat("/var/lib/catpackage", &st) != 0) {
        if (errno != ENOENT) {
            perror("stat");
            return 1;
        }

        if (mkdir("/var/lib/catpackage", 0755) != 0) {
            perror("mkdir");
            return 1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(
            stderr,
            "\"/var/lib/catpackage\" exists but is not a directory\n"
        );

        return 1;
    }

    /*
     * Ensure database directory
     */
    if (stat(CATPKG_DATABASE_DIR_PATH, &st) != 0) {
        if (errno != ENOENT) {
            perror("stat");
            return 1;
        }

        if (mkdir(CATPKG_DATABASE_DIR_PATH, 0755) != 0) {
            perror("mkdir");
            return 1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(
            stderr,
            "\"%s\" exists but is not a directory\n",
            CATPKG_DATABASE_DIR_PATH
        );

        return 1;
    }

    /*
     * Ensure DATABASE
     */
    char database_path[PATH_MAX];

    snprintf(
        database_path,
        sizeof(database_path),
        "%s/%s",
        CATPKG_DATABASE_DIR_PATH,
        CATPKG_DATABASE
    );

    if (stat(database_path, &st) != 0) {
        if (errno != ENOENT) {
            perror("stat");
            return 1;
        }

        FILE *file = fopen(database_path, "w");

        if (file == NULL) {
            perror("fopen");
            return 1;
        }

        if (
            fprintf(
                file,
                "CATPKG 1.1\n"
                "#catpkg-version 2.0.0-beta\n"
                "#use-options DEPENDENCIES\n"
                "#use-options POLICY\n"
            ) < 0
        ) {
            fclose(file);
            return 1;
        }

        if (fclose(file) != 0) {
            perror("fclose");
            return 1;
        }
    }

    /*
     * Ensure DEPENDENCIES
     */
    char dependencies_path[PATH_MAX];

    snprintf(
        dependencies_path,
        sizeof(dependencies_path),
        "%s/%s",
        CATPKG_DATABASE_DIR_PATH,
        CATPKG_DEPENDENCIES
    );

    if (ensure_empty_file(dependencies_path) != 0)
        return 1;

    /*
     * Ensure POLICY
     */
    char policy_path[PATH_MAX];

    snprintf(
        policy_path,
        sizeof(policy_path),
        "%s/%s",
        CATPKG_DATABASE_DIR_PATH,
        CATPKG_POLICY
    );

    if (ensure_empty_file(policy_path) != 0)
        return 1;

    return 0;
}

size_t database_get_package_id(
    char *target_package_name
) {
    for (size_t package_id = 0; package_id < database_package_ids_size; ++package_id)
    {
        char *package_name = database_package_ids[package_id];
    
        if(strcmp(target_package_name, package_name) == 0)
            return package_id;
    }
    return DATABASE_PACKAGE_NONE_ID;
}

struct PackageInfo *catpkg_find_package_info(const char *package_fullname)
{
    char *package_path = make_catpkg_path(
        CATPKG_METADATA_PATH "/%s",
        package_fullname,
        CATPKG_PACKAGEINFO
    );

    if (package_path == NULL) {
        return NULL;
    }

    struct PackageInfo *info = CATPKG_Parse(package_path, NULL);

    free(package_path);
    return info;
}

struct PackageMatches catpkg_find_package(
    const char *package_name
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };


    if (access(CATPKG_PACKAGES_PATH, F_OK) != 0)
        return matches;

    DIR *dir = opendir(CATPKG_PACKAGES_PATH);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;

        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info = CATPKG_Parse(package_info_path, NULL);
        CATPKG_ReadSections(info, 1);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        if (name_field == NULL ||
            name_field->value == NULL) {
            PackageInfo_Free(info);
            continue;
        }

        /*
         * Check for an exact package name match first.
         */
        if (strcmp(name_field->value, package_name) == 0) {

            /*
             * Remove any previously collected partial matches.
             */
            for (size_t i = 0; i < matches.count; i++)
                free(matches.items[i]);

            free(matches.items);

            matches.items = malloc(sizeof(*matches.items));

            if (matches.items == NULL) {
                perror("catpkg: malloc");
                matches.count = 0;
                PackageInfo_Free(info);
                break;
            }

            matches.items[0] = strdup(name_field->value);

            if (matches.items[0] == NULL) {
                perror("catpkg: strdup");
                free(matches.items);
                matches.items = NULL;
                matches.count = 0;
                PackageInfo_Free(info);
                break;
            }

            matches.count = 1;

            PackageInfo_Free(info);

            /*
             * An exact match was found.
             * No further search is necessary.
             */
            break;
        }

        /*
         * No exact match was found yet.
         *
         * Check whether the package name starts
         * with the requested name.
         */
        size_t package_name_length = strlen(package_name);

        if (strncmp(
                name_field->value,
                package_name,
                package_name_length
            ) == 0) {

            char **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                perror("catpkg: realloc");
                PackageInfo_Free(info);
                break;
            }

            matches.items = tmp;

            matches.items[matches.count] =
                strdup(name_field->value);

            if (matches.items[matches.count] == NULL) {
                perror("catpkg: strdup");
                PackageInfo_Free(info);
                break;
            }

            matches.count++;
        }

        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
struct PackageMatches catpkg_find_version(
    const char *package_name
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (access(CATPKG_PACKAGES_PATH, F_OK) != 0)
        return matches;

    DIR *dir = opendir(CATPKG_PACKAGES_PATH);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;


        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info = CATPKG_Parse(package_info_path, NULL);
        CATPKG_ReadSections(info, 2);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        const struct PackageField *version_field =
            PackageInfo_Find(info, "version");

        if (name_field == NULL ||
            name_field->value == NULL ||
            version_field == NULL ||
            version_field->value == NULL) {

            PackageInfo_Free(info);
            continue;
        }

        if (strcmp(name_field->value, package_name) != 0) {
            PackageInfo_Free(info);
            continue;
        }

        char **tmp = realloc(
            matches.items,
            (matches.count + 1) *
            sizeof(*matches.items)
        );

        if (tmp == NULL) {
            perror("catpkg: realloc");
            PackageInfo_Free(info);
            break;
        }

        matches.items = tmp;

        matches.items[matches.count] =
            strdup(version_field->value);

        if (matches.items[matches.count] == NULL) {
            perror("catpkg: strdup");
            PackageInfo_Free(info);
            break;
        }

        matches.count++;

        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
void PackageMatches_Free(
    struct PackageMatches *matches
)
{
    if (matches == NULL)
        return;

    for (size_t i = 0; i < matches->count; i++)
        free(matches->items[i]);

    free(matches->items);

    matches->items = NULL;
    matches->count = 0;
}

struct PackageMatches catpkg_find_package_fullname(
    const char *package_fullname
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (access(CATPKG_PACKAGES_PATH, F_OK) != 0)
        return matches;

    DIR *dir = opendir(CATPKG_PACKAGES_PATH);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;

        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info = CATPKG_Parse(package_info_path, NULL);
        CATPKG_ReadSections(info, 2);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        const struct PackageField *version_field =
            PackageInfo_Find(info, "version");

        if (name_field == NULL ||
            name_field->value == NULL ||
            version_field == NULL ||
            version_field->value == NULL) {

            PackageInfo_Free(info);
            continue;
        }

        char *current_fullname = make_catpkg_path(
            "%s@%s",
            name_field->value,
            version_field->value
        );

        if (current_fullname == NULL) {
            PackageInfo_Free(info);
            continue;
        }

        if (strcmp(current_fullname, package_fullname) == 0) {

            char **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                perror("catpkg: realloc");
                free(current_fullname);
                PackageInfo_Free(info);
                break;
            }

            matches.items = tmp;

            matches.items[matches.count] =
                strdup(current_fullname);

            if (matches.items[matches.count] == NULL) {
                perror("catpkg: strdup");
                free(current_fullname);
                PackageInfo_Free(info);
                break;
            }

            matches.count++;
        }

        free(current_fullname);
        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
struct PackageMatches catpkg_find_cached_package(
    const char *package_fullname
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (package_fullname == NULL ||
        package_fullname[0] == '\0')
        return matches;

    if (access(CATPKG_PACKAGES_PATH, F_OK) != 0)
        return matches;

    DIR *dir = opendir(CATPKG_CACHE_PACKAGES);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    size_t search_length = strlen(package_fullname);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        /*
         * Check for .catpackage.zst
         */
        const char *zst_suffix = ".catpackage.zst";
        size_t zst_suffix_length = strlen(zst_suffix);

        /*
         * Check for .catpackage
         */
        const char *catpackage_suffix = ".catpackage";
        size_t catpackage_suffix_length =
            strlen(catpackage_suffix);

        const char *matched_suffix = NULL;
        size_t matched_suffix_length = 0;

        size_t entry_length = strlen(entry->d_name);

        if (entry_length > zst_suffix_length &&
            strcmp(
                entry->d_name +
                entry_length -
                zst_suffix_length,
                zst_suffix
            ) == 0) {

            matched_suffix = zst_suffix;
            matched_suffix_length = zst_suffix_length;

        } else if (
            entry_length > catpackage_suffix_length &&
            strcmp(
                entry->d_name +
                entry_length -
                catpackage_suffix_length,
                catpackage_suffix
            ) == 0
        ) {

            matched_suffix = catpackage_suffix;
            matched_suffix_length =
                catpackage_suffix_length;
        }

        /*
         * Not a CatPackage cache file.
         */
        if (matched_suffix == NULL)
            continue;

        /*
         * Calculate the package full name without
         * the cache file extension.
         */
        size_t fullname_length =
            entry_length - matched_suffix_length;

        /*
         * Check whether the package name matches
         * the requested name.
         */
        if (fullname_length < search_length)
            continue;

        if (strncmp(
                entry->d_name,
                package_fullname,
                search_length
            ) != 0)
            continue;

        /*
         * Make sure the requested name is a complete
         * package fullname prefix.
         *
         * For example:
         *
         * foo@1.0  -> foo@1.0.0
         *
         * is not considered a match when an exact
         * version was requested.
         *
         * But:
         *
         * foo@ -> foo@1.0.0
         *
         * is valid.
         */
        if (search_length > 0 &&
            package_fullname[search_length - 1] != '@') {

            if (fullname_length != search_length)
                continue;
        }

        /*
         * Check whether this package fullname was
         * already added.
         *
         * This prevents:
         *
         * foo@1.0.0.catpackage
         * foo@1.0.0.catpackage.zst
         *
         * from producing two matches.
         */
        int already_found = 0;

        for (size_t i = 0; i < matches.count; i++) {

            if (strcmp(
                    matches.items[i],
                    entry->d_name
                ) == 0) {

                already_found = 1;
                break;
            }

            /*
             * Compare against the fullname without
             * the extension.
             */
            if (strlen(matches.items[i]) == fullname_length &&
                strncmp(
                    matches.items[i],
                    entry->d_name,
                    fullname_length
                ) == 0) {

                already_found = 1;
                break;
            }
        }

        if (already_found)
            continue;

        /*
         * Allocate space for the package fullname.
         */
        char *fullname = malloc(
            fullname_length + 1
        );

        if (fullname == NULL) {
            perror("catpkg: malloc");
            break;
        }

        memcpy(
            fullname,
            entry->d_name,
            fullname_length
        );

        fullname[fullname_length] = '\0';

        /*
         * Add the package fullname to the results.
         */
        char **tmp = realloc(
            matches.items,
            (matches.count + 1) *
            sizeof(*matches.items)
        );

        if (tmp == NULL) {
            perror("catpkg: realloc");
            free(fullname);
            break;
        }

        matches.items = tmp;

        matches.items[matches.count] = fullname;
        matches.count++;
    }

    closedir(dir);

    return matches;
}

static int catpkg_merge_database_add_parent_section(
    size_t **parent_sections,
    size_t *parent_sections_count,
    size_t section
)
{
    if (
        parent_sections == NULL ||
        parent_sections_count == NULL
    ) {
        return 1;
    }


    /*
     * Do not add the same parent section twice.
     */

    for (
        size_t i = 0;
        i < *parent_sections_count;
        i++
    ) {
        if ((*parent_sections)[i] == section)
            return 0;
    }


    /*
     * Check allocation size overflow.
     */

    if (
        *parent_sections_count >
        (SIZE_MAX / sizeof(**parent_sections)) - 1
    ) {
        return 1;
    }

    size_t new_count =
        *parent_sections_count + 1;

    size_t *new_parent_sections =
        realloc(
            *parent_sections,
            new_count *
                sizeof(**parent_sections)
        );

    if (new_parent_sections == NULL)
        return 1;

    *parent_sections =
        new_parent_sections;

    (*parent_sections)[
        *parent_sections_count
    ] = section;

    *parent_sections_count =
        new_count;

    return 0;
}

int catpkg_merge_database_get_or_resolve_entry(
    struct PackageInfo *database_fileinfo,
    size_t *last_database_section,
    const char *type,
    const char *path,
    size_t *section
)
{
    if (
        database_fileinfo == NULL ||
        last_database_section == NULL ||
        type == NULL ||
        path == NULL ||
        section == NULL
    ) {
        return -1;
    }

    char *correct_path =
        catpkg_normalize_tar_path(
            path
        );

    if (correct_path == NULL)
        return -1;

    const struct PackageField *db_info =
        PackageInfo_Find(
            database_fileinfo,
            "info"
        );

    while (db_info != NULL) {
        if (
            db_info->name == NULL ||
            strcmp(
                db_info->name,
                "info"
            ) != 0
        ) {
            db_info =
                db_info->next_field;

            continue;
        }

        const char *db_type =
            NULL;

        const char *db_path =
            NULL;

        const struct PackageField *db_field =
            database_fileinfo->fields;

        while (db_field != NULL) {
            if (
                db_field->section ==
                    db_info->section &&
                db_field->name != NULL
            ) {
                if (
                    strcmp(
                        db_field->name,
                        "#type"
                    ) == 0
                ) {
                    db_type =
                        db_field->value;
                }
                else if (
                    strcmp(
                        db_field->name,
                        "#path"
                    ) == 0
                ) {
                    db_path =
                        db_field->value;
                }
            }

            db_field =
                db_field->next_field;
        }

        if (
            db_type != NULL &&
            db_path != NULL &&
            strcmp(
                db_type,
                type
            ) == 0
        ) {
            char *correct_db_path =
                catpkg_normalize_tar_path(
                    db_path
                );

            if (correct_db_path == NULL) {
                free(correct_path);
                return -1;
            }

            int path_equal =
                strcmp(
                    correct_db_path,
                    correct_path
                ) == 0;

            free(
                correct_db_path
            );

            if (path_equal) {
                *section =
                    db_info->section;

                free(
                    correct_path
                );

                return 1;
            }
        }

        db_info =
            db_info->next_field;
    }

    /*
     * Entry was not found in the existing database.
     * Resolve the section it would receive.
     */

    if (*last_database_section == SIZE_MAX) {
        free(correct_path);
        return -1;
    }

    *last_database_section += 1;

    *section =
        *last_database_section;

    free(
        correct_path
    );

    return 0;
}

int catpkg_database_path_persistent(
    const struct PackageInfo *database_fileinfo,
    const char *package_name,
    const char *path,
    int include_children
)
{
    if (
        database_fileinfo == NULL ||
        package_name == NULL ||
        path == NULL
    ) {
        return -1;
    }

    char *correct_search_path =
        catpkg_normalize_tar_path(path);

    if (correct_search_path == NULL)
        return -1;

    size_t search_path_len =
        strlen(correct_search_path);

    const struct PackageField *field =
        database_fileinfo->fields;

    while (field != NULL) {

        /*
         * We only need DATABASE #path fields.
         */

        if (
            field->name == NULL ||
            field->value == NULL ||
            strcmp(
                field->name,
                "#path"
            ) != 0
        ) {
            field = field->next_field;
            continue;
        }

        char *correct_database_path =
            catpkg_normalize_tar_path(
                field->value
            );

        if (correct_database_path == NULL) {
            free(correct_search_path);
            return -1;
        }


        /*
         * Check whether this DATABASE path is the
         * searched path or one of its children.
         */

        bool path_matches = false;

        if (
            strcmp(
                correct_search_path,
                correct_database_path
            ) == 0
        ) {
            path_matches = true;
        }

        else if (
            include_children &&
            search_path_len > 0 &&
            strncmp(
                correct_database_path,
                correct_search_path,
                search_path_len
            ) == 0 &&
            correct_database_path[
                search_path_len
            ] == '/'
        ) {
            path_matches = true;
        }

        free(correct_database_path);

        if (!path_matches) {
            field = field->next_field;
            continue;
        }


        /*
         * DATABASE PackageField.section starts at 1.
         * DEPENDENCIES section index starts at 0.
         */

        if (field->section == 0) {
            field = field->next_field;
            continue;
        }

        size_t dependency_section =
            field->section - 1;


        /*
         * DATABASE was parsed together with
         * #use-options DEPENDENCIES, so #persistent
         * fields are available in the same PackageInfo.
         */

        const struct PackageField *persistent_field =
            PackageInfo_Find(
                database_fileinfo,
                "#persistent"
            );

        while (persistent_field != NULL) {

            if (
                persistent_field->name == NULL ||
                persistent_field->value == NULL ||
                strcmp(
                    persistent_field->name,
                    "#persistent"
                ) != 0
            ) {
                persistent_field =
                    persistent_field->next_field;

                continue;
            }


            /*
             * Format:
             *
             *     #persistent N=package
             */

            const char *separator =
                strchr(
                    persistent_field->value,
                    '='
                );

            if (separator == NULL) {
                persistent_field =
                    persistent_field->next_field;

                continue;
            }

            errno = 0;

            char *end = NULL;

            unsigned long long parsed_section =
                strtoull(
                    persistent_field->value,
                    &end,
                    10
                );

            if (
                errno != 0 ||
                end ==
                    persistent_field->value ||
                end != separator ||
                parsed_section > SIZE_MAX
            ) {
                persistent_field =
                    persistent_field->next_field;

                continue;
            }

            const char *persistent_package =
                separator + 1;

            if (
                (size_t)parsed_section ==
                    dependency_section &&
                strcmp(
                    persistent_package,
                    package_name
                ) == 0
            ) {
                free(correct_search_path);

                return 1;
            }

            persistent_field =
                persistent_field->next_field;
        }

        field =
            field->next_field;
    }

    free(correct_search_path);

    return 0;
}

/*
 * Check whether a DEPENDENCIES section is obsolete.
 *
 * DEPENDENCIES section:
 *
 *     0 -> DATABASE section 1
 *     1 -> DATABASE section 2
 *     ...
 */

static bool catpkg_database_section_is_obsolete(
    const size_t *obsolete_sections,
    size_t obsolete_count,
    size_t section
)
{
    for (
        size_t i = 0;
        i < obsolete_count;
        i++
    ) {
        if (
            obsolete_sections[i] ==
            section
        ) {
            return true;
        }
    }

    return false;
}


/*
 * Convert an old DEPENDENCIES section number to its
 * new number after obsolete sections are removed.
 *
 * Example:
 *
 * obsolete:
 *
 *     1
 *     4
 *
 * old:
 *
 *     0 -> 0
 *     2 -> 1
 *     3 -> 2
 *     5 -> 3
 */

static size_t catpkg_database_remap_section(
    const size_t *obsolete_sections,
    size_t obsolete_count,
    size_t old_section
)
{
    size_t removed_before = 0;

    for (
        size_t i = 0;
        i < obsolete_count;
        i++
    ) {
        if (
            obsolete_sections[i] <
            old_section
        ) {
            removed_before++;
        }
    }

    return old_section -
        removed_before;
}


/*
 * Parse:
 *
 *     <section>=<package>
 *
 * Returns:
 *
 *     section
 *     package pointer inside value
 *
 * value is modified.
 */

static int catpkg_database_parse_dependency(
    char *value,
    size_t *section,
    char **package
)
{
    if (
        value == NULL ||
        section == NULL ||
        package == NULL
    ) {
        return 1;
    }

    char *separator =
        strchr(
            value,
            '='
        );

    if (separator == NULL)
        return 1;

    *separator = '\0';

    char *section_text =
        value;

    char *package_text =
        separator + 1;

    if (
        section_text[0] == '\0' ||
        package_text[0] == '\0'
    ) {
        return 1;
    }

    char *end = NULL;

    errno = 0;

    unsigned long long parsed =
        strtoull(
            section_text,
            &end,
            10
        );

    if (
        errno != 0 ||
        end == section_text ||
        *end != '\0' ||
        parsed > SIZE_MAX
    ) {
        return 1;
    }

    *section =
        (size_t)parsed;

    *package =
        package_text;

    return 0;
}

int catpkg_database_sync_obsolete(
    const char *database_path,
    const char *dependencies_path
)
{
    if (
        database_path == NULL ||
        dependencies_path == NULL
    ) {
        return 1;
    }

    FILE *dependencies = NULL;
    FILE *database_source = NULL;
    FILE *database_tmp = NULL;
    FILE *dependencies_tmp = NULL;

    size_t *obsolete_sections = NULL;
    size_t obsolete_count = 0;

    char *database_tmp_path = NULL;
    char *dependencies_tmp_path = NULL;

    char *line = NULL;
    size_t line_capacity = 0;

    char *section_buffer = NULL;
    size_t section_buffer_size = 0;

    ssize_t line_length;

    bool success = false;

    dependencies =
        fopen(
            dependencies_path,
            "rb"
        );

    if (dependencies == NULL) {
        perror(
            "catpkg: failed to open DEPENDENCIES"
        );

        goto cleanup;
    }

    while (
        (
            line_length =
                getline(
                    &line,
                    &line_capacity,
                    dependencies
                )
        ) != -1
    ) {
        const char prefix[] =
            "#obsolete ";

        const size_t prefix_length =
            sizeof(prefix) - 1;

        if (
            (size_t)line_length <
                prefix_length ||
            strncmp(
                line,
                prefix,
                prefix_length
            ) != 0
        ) {
            continue;
        }

        char *section_text =
            line +
            prefix_length;

        char *newline =
            strchr(
                section_text,
                '\n'
            );

        if (newline != NULL)
            *newline = '\0';

        char *carriage =
            strchr(
                section_text,
                '\r'
            );

        if (carriage != NULL)
            *carriage = '\0';

        char *end = NULL;

        errno = 0;

        unsigned long long parsed =
            strtoull(
                section_text,
                &end,
                10
            );

        if (
            errno != 0 ||
            end == section_text ||
            *end != '\0' ||
            parsed > SIZE_MAX
        ) {
            goto cleanup;
        }

        size_t section =
            (size_t)parsed;

        if (
            catpkg_database_section_is_obsolete(
                obsolete_sections,
                obsolete_count,
                section
            )
        ) {
            continue;
        }

        size_t *tmp =
            realloc(
                obsolete_sections,
                (obsolete_count + 1) *
                    sizeof(*obsolete_sections)
            );

        if (tmp == NULL)
            goto cleanup;

        obsolete_sections =
            tmp;

        obsolete_sections[
            obsolete_count
        ] = section;

        obsolete_count++;
    }

    if (ferror(dependencies))
        goto cleanup;

    free(line);

    line = NULL;
    line_capacity = 0;

    if (
        fclose(
            dependencies
        ) != 0
    ) {
        dependencies = NULL;

        goto cleanup;
    }

    dependencies = NULL;

    if (obsolete_count == 0) {
        success = true;

        goto cleanup;
    }

    database_source =
        fopen(
            database_path,
            "rb"
        );

    if (database_source == NULL) {
        perror(
            "catpkg: failed to open DATABASE"
        );

        goto cleanup;
    }

    size_t database_tmp_length =
        strlen(database_path) +
        sizeof(".tmp");

    database_tmp_path =
        malloc(
            database_tmp_length
        );

    if (database_tmp_path == NULL)
        goto cleanup;

    snprintf(
        database_tmp_path,
        database_tmp_length,
        "%s.tmp",
        database_path
    );

    database_tmp =
        fopen(
            database_tmp_path,
            "wb"
        );

    if (database_tmp == NULL) {
        perror(
            "catpkg: failed to create DATABASE.tmp"
        );

        goto cleanup;
    }

    bool global_section = true;

    size_t database_section = 0;

    while (
        (
            line_length =
                getline(
                    &line,
                    &line_capacity,
                    database_source
                )
        ) != -1
    ) {
        size_t content_length =
            (size_t)line_length;

        while (
            content_length > 0 &&
            (
                line[
                    content_length - 1
                ] == '\n' ||
                line[
                    content_length - 1
                ] == '\r'
            )
        ) {
            content_length--;
        }

        bool separator =
            content_length == 1 &&
            line[0] == '-';

        if (global_section) {
            if (separator) {
                global_section = false;

                continue;
            }

            if (
                fwrite(
                    line,
                    1,
                    (size_t)line_length,
                    database_tmp
                ) !=
                    (size_t)line_length
            ) {
                goto cleanup;
            }

            continue;
        }

        if (!separator) {
            size_t append_size =
                (size_t)line_length;

            if (
                append_size >
                SIZE_MAX -
                    section_buffer_size
            ) {
                goto cleanup;
            }

            size_t new_size =
                section_buffer_size +
                append_size;

            char *tmp =
                realloc(
                    section_buffer,
                    new_size
                );

            if (tmp == NULL)
                goto cleanup;

            section_buffer =
                tmp;

            memcpy(
                section_buffer +
                    section_buffer_size,
                line,
                append_size
            );

            section_buffer_size =
                new_size;

            continue;
        }

        if (
            !catpkg_database_section_is_obsolete(
                obsolete_sections,
                obsolete_count,
                database_section
            )
        ) {
            if (
                fprintf(
                    database_tmp,
                    "-\n"
                ) < 0
            ) {
                goto cleanup;
            }

            if (
                section_buffer_size != 0 &&
                fwrite(
                    section_buffer,
                    1,
                    section_buffer_size,
                    database_tmp
                ) !=
                    section_buffer_size
            ) {
                goto cleanup;
            }
        }

        free(
            section_buffer
        );

        section_buffer = NULL;
        section_buffer_size = 0;

        database_section++;
    }

    if (ferror(database_source))
        goto cleanup;

    if (section_buffer_size != 0) {
        if (
            !catpkg_database_section_is_obsolete(
                obsolete_sections,
                obsolete_count,
                database_section
            )
        ) {
            if (
                fprintf(
                    database_tmp,
                    "-\n"
                ) < 0
            ) {
                goto cleanup;
            }

            if (
                fwrite(
                    section_buffer,
                    1,
                    section_buffer_size,
                    database_tmp
                ) !=
                    section_buffer_size
            ) {
                goto cleanup;
            }
        }
    }

    free(
        section_buffer
    );

    section_buffer = NULL;
    section_buffer_size = 0;

    free(line);

    line = NULL;
    line_capacity = 0;

    if (
        fclose(
            database_source
        ) != 0
    ) {
        database_source = NULL;

        goto cleanup;
    }

    database_source = NULL;

    if (
        fflush(
            database_tmp
        ) != 0
    ) {
        goto cleanup;
    }

    if (
        fclose(
            database_tmp
        ) != 0
    ) {
        database_tmp = NULL;

        goto cleanup;
    }

    database_tmp = NULL;

    dependencies =
        fopen(
            dependencies_path,
            "rb"
        );

    if (dependencies == NULL) {
        perror(
            "catpkg: failed to reopen DEPENDENCIES"
        );

        goto cleanup;
    }

    size_t dependencies_tmp_length =
        strlen(dependencies_path) +
        sizeof(".tmp");

    dependencies_tmp_path =
        malloc(
            dependencies_tmp_length
        );

    if (dependencies_tmp_path == NULL)
        goto cleanup;

    snprintf(
        dependencies_tmp_path,
        dependencies_tmp_length,
        "%s.tmp",
        dependencies_path
    );

    dependencies_tmp =
        fopen(
            dependencies_tmp_path,
            "wb"
        );

    if (dependencies_tmp == NULL) {
        perror(
            "catpkg: failed to create DEPENDENCIES.tmp"
        );

        goto cleanup;
    }

    while (
        (
            line_length =
                getline(
                    &line,
                    &line_capacity,
                    dependencies
                )
        ) != -1
    ) {
        if (
            strncmp(
                line,
                "#obsolete ",
                sizeof("#obsolete ") - 1
            ) == 0
        ) {
            continue;
        }

        const char *dependency_name =
            NULL;

        size_t dependency_name_length =
            0;

        if (
            strncmp(
                line,
                "#required ",
                sizeof("#required ") - 1
            ) == 0
        ) {
            dependency_name =
                "#required";

            dependency_name_length =
                sizeof("#required") - 1;
        }
        else if (
            strncmp(
                line,
                "#persistent ",
                sizeof("#persistent ") - 1
            ) == 0
        ) {
            dependency_name =
                "#persistent";

            dependency_name_length =
                sizeof("#persistent") - 1;
        }

        if (dependency_name == NULL) {
            if (
                fwrite(
                    line,
                    1,
                    (size_t)line_length,
                    dependencies_tmp
                ) !=
                    (size_t)line_length
            ) {
                goto cleanup;
            }

            continue;
        }

        char *value =
            strdup(
                line +
                    dependency_name_length +
                    1
            );

        if (value == NULL)
            goto cleanup;

        char *newline =
            strchr(
                value,
                '\n'
            );

        if (newline != NULL)
            *newline = '\0';

        char *carriage =
            strchr(
                value,
                '\r'
            );

        if (carriage != NULL)
            *carriage = '\0';

        size_t old_section = 0;

        char *package = NULL;

        if (
            catpkg_database_parse_dependency(
                value,
                &old_section,
                &package
            ) != 0
        ) {
            free(value);

            goto cleanup;
        }

        if (
            catpkg_database_section_is_obsolete(
                obsolete_sections,
                obsolete_count,
                old_section
            )
        ) {
            free(value);

            continue;
        }

        size_t new_section =
            catpkg_database_remap_section(
                obsolete_sections,
                obsolete_count,
                old_section
            );

        if (
            fprintf(
                dependencies_tmp,
                "%s %zu=%s\n",
                dependency_name,
                new_section,
                package
            ) < 0
        ) {
            free(value);

            goto cleanup;
        }

        free(value);
    }

    if (ferror(dependencies))
        goto cleanup;

    free(line);

    line = NULL;
    line_capacity = 0;

    if (
        fflush(
            dependencies_tmp
        ) != 0
    ) {
        goto cleanup;
    }

    if (
        fclose(
            dependencies_tmp
        ) != 0
    ) {
        dependencies_tmp = NULL;

        goto cleanup;
    }

    dependencies_tmp = NULL;

    if (
        fclose(
            dependencies
        ) != 0
    ) {
        dependencies = NULL;

        goto cleanup;
    }

    dependencies = NULL;

    if (
        rename(
            database_tmp_path,
            database_path
        ) != 0
    ) {
        perror(
            "catpkg: failed to replace DATABASE"
        );

        goto cleanup;
    }

    free(
        database_tmp_path
    );

    database_tmp_path = NULL;

    if (
        rename(
            dependencies_tmp_path,
            dependencies_path
        ) != 0
    ) {
        perror(
            "catpkg: failed to replace DEPENDENCIES"
        );

        goto cleanup;
    }

    free(
        dependencies_tmp_path
    );

    dependencies_tmp_path = NULL;

    success = true;


cleanup:

    free(line);
    free(section_buffer);

    if (dependencies_tmp != NULL)
        fclose(dependencies_tmp);

    if (database_tmp != NULL)
        fclose(database_tmp);

    if (dependencies != NULL)
        fclose(dependencies);

    if (database_source != NULL)
        fclose(database_source);

    if (dependencies_tmp_path != NULL)
        remove(dependencies_tmp_path);

    if (database_tmp_path != NULL)
        remove(database_tmp_path);

    free(dependencies_tmp_path);
    free(database_tmp_path);
    free(obsolete_sections);

    return success
        ? 0
        : 1;
}