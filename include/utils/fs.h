#ifndef CATPKG_UTILS_FS_H
#define CATPKG_UTILS_FS_H

#include <sys/types.h>
#include <stdint.h>

int is_excluded_tar_path(
    const char *path,
    char *exclude[],
    int include_children
);

int catpkg_move_file(
    const char *source,
    const char *destination
);

int catpkg_move_directory(
    const char *source,
    const char *destination
);

void catpkg_format_size(
    off_t size, 
    char *buffer, 
    size_t buffer_size
);

char *catpkg_normalize_tar_path(
    const char *path
);
uint64_t get_directory_size(
    const char *path
);

int catpkg_remove_directory(
    const char *path
);

int catpkg_remove_directory_estimate(
    const char *path,
    off_t *size
);

char *catpkg_absolute_path(
    const char *path
);

#endif