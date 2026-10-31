#ifndef CATPKG_UTILS_CACHE_H
#define CATPKG_UTILS_CACHE_H

int catpkg_cache_package(
    const char *source,
    const char *packageFullName
);

int compress_zstd(
    const char *source,
    const char *packageFullName
);

int catpkg_clear_package_cache(
    const char *packageName,
    const char *version
);

#endif