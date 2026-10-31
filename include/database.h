#ifndef CATPKG_PACKAGEMANAGER_DATABASE_H
#define CATPKG_PACKAGEMANAGER_DATABASE_H

#include <stddef.h>
#include <stdio.h>

#include "catpkg/pkginfo.h"

struct PackageMatches {
    char **items;
    size_t count;
};

struct MergeDatabaseDependency {
    size_t section;
    char *package;
};

int update_database_package_ids();

size_t database_get_package_id(
    char *target_package_name
);

int catpkg_database_ensure(void);

int catpkg_merge_database_get_or_resolve_entry(
    struct PackageInfo *database_fileinfo,
    size_t *last_database_section,
    const char *type,
    const char *path,
    size_t *section
);

int catpkg_database_sync_obsolete(
    const char *database_path,
    const char *dependencies_path
);

struct PackageInfo *catpkg_find_package_info(
    const char *package_fullname
);

struct PackageMatches catpkg_find_package(
    const char *package_name
);
struct PackageMatches catpkg_find_package_fullname(
    const char *package_fullname
);

void PackageMatches_Free(
    struct PackageMatches *matches
);

struct PackageMatches catpkg_find_version(
    const char *package_name
);

struct PackageMatches catpkg_find_cached_package(
    const char *package_fullname
);

int catpkg_database_path_persistent(
    const struct PackageInfo *database_fileinfo,
    const char *package_name,
    const char *path,
    int include_children
);

#endif