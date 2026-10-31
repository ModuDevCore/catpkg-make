#ifndef CATPKG_UTILS_PKGINFO_H
#define CATPKG_UTILS_PKGINFO_H

#include <stddef.h>
#include <stdio.h>

struct PackageReader {
    const char *base_path;

    int fd_r;

    int fd_bf_w;
    int fd_bf_r;

    int fd_main_w;
    int fd_main_r;

    long bytepos_buffer_offset;
    long bytepos;
};

struct PackageInfo {
    struct PackageField *fields;
    size_t fields_count;
    int patch;

    struct PackageReader *reader;
};

typedef struct PackageField {
    char *name;
    char *value;
    long bytepos;

    size_t section;

    struct PackageField *next_field;
} PackageField;


struct PackageInfo *CATPKG_FD_Parse(
    int fd_r,
    const char *base_path,
    struct PackageInfo *reuse
);

struct PackageInfo *CATPKG_Parse(
    const char *file_path,
    struct PackageInfo *reuse
);

ssize_t CATPKG_ReadAll(
    struct PackageInfo *info
);
ssize_t CATPKG_ReadSections(
    struct PackageInfo *info,
    size_t sections_lim
);
ssize_t CATPKG_ReadSection(
    struct PackageInfo *info,
    size_t section_read
);

const struct PackageField *PackageInfo_Find(
    const struct PackageInfo *info,
    const char *name
);

void PackageInfo_Free(
    struct PackageInfo *info
);

struct PackageFieldMatches {
    const struct PackageField **items;
    size_t count;
};

struct PackageFieldMatches PackageInfo_FindAll(
    const struct PackageInfo *info,
    const char *name
);

void PackageFieldMatches_Free(
    struct PackageFieldMatches *matches
);

int is_catpackage(
    const char *path
);

#endif