#ifndef CATPKG_UTILS_PATH_H
#define CATPKG_UTILS_PATH_H

char *make_catpkg_path(
    const char *format,
    ...
);

int catpkg_path_contains(
    const char *directory,
    const char *path
);

#endif