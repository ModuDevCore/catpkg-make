#ifndef CATPKG_PACKAGEMANAGER_UPDATE_H
#define CATPKG_PACKAGEMANAGER_UPDATE_H
#include <stddef.h>

int catpkg_update(
    const char **paths,
    const size_t paths_count
);

#endif