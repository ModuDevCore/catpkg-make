#ifndef CATPKG_PACKAGEMANAGER_REMOVE_H
#define CATPKG_PACKAGEMANAGER_REMOVE_H

#include <stdint.h>

int catpkg_remove(
    const char **package_names,
    const size_t package_names_count,
    bool assume_yes
);

int catpkg_calc_released(
    uint64_t *will_be_released,
    const char *package_fullname,
    bool excludeCache
);

#endif