#ifndef CATPKG_PACKAGEMANAGER_INSTALL_H
#define CATPKG_PACKAGEMANAGER_INSTALL_H

#include <stdint.h>
#include "catpkg/pkginfo.h"

int catpkg_calc_installed(
    uint64_t *will_be_installed,
    const struct PackageInfo *info
);

int catpkg_install(
    const char **paths,
    const size_t paths_count,
    bool assume_yes
);

#endif