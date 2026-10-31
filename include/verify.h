#ifndef CATPKG_PACKAGEMANAGER_VERIFY_H
#define CATPKG_PACKAGEMANAGER_VERIFY_H

#include "utils/sha256.h"

int catpkg_integrity_catpackage(
    const char *package_path,
    char *hex
);

int catpkg_sign_catpackage(
    const char *package_path,
    struct sha256_buff *hash
);

int catpkg_verify(
	const char *path
);

int tar_header_is_empty(
    const char *header
);

#endif