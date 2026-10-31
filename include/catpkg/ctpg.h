#ifndef CATPKG_CTPG_H
#define CATPKG_CTPG_H

#include <sys/types.h>
#include <stdio.h>

struct CtpgContext {
    char *path;

    struct PackageInfo *pkginfo;
    
    const struct PackageField *name_field;
    const struct PackageField *version_field;
    const struct PackageField *description_field;
    const struct PackageField *sha256_field;
    const struct PackageField *size_field;
    const struct PackageField *dependency_field;
    const struct PackageField *folder_field;
    const struct PackageField *persistent_file_field;
    const struct PackageField *operations_field;
    const struct PackageField *symlink_field;
    const struct PackageField *command_field;
    
    char *full_name;
};

struct header_posix_ustar {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

int ctpg_context_init(
    struct CtpgContext **ctpg_context,
    char *path
);

void ctpg_context_close(
    struct CtpgContext **ctpg_context
);

int catpkg_builder_ctpg_extract_size(
    struct CtpgContext *ctpg_context,
    const char *path,
    off_t *size,
    char *exclude[]
);

int catpkg_builder_catpackage_extract(
    struct CtpgContext *ctpg_context,
    const char *path,
    const char *extract_to,
    char *exclude[]
);

int catpkg_builder_catpackage_extract_mapped(
    struct CtpgContext *ctpg_context,
    const char *path,
    const char *extract_to,
    char *exclude[]
);

int ctpg_read_tar_header(
    struct header_posix_ustar *header,
    FILE *file
);

int catpkg_builder_ctpg_path_exists(
    struct CtpgContext *ctpg_context,
    const char *path,
    int include_children
);

int catpkg_builder_ctpg_path_obsolete(
    struct CtpgContext *ctpg_context,
    const char *path,
    int include_children
);

int catpkg_builder_ctpg_file_obsolete(
    struct CtpgContext *ctpg_context,
    const char *path
);

void *catpkg_builder_ctpg_read_data(
    struct CtpgContext *ctpg_context,
    const char *path
);

int find_ctpg_hpu_info(
    const char *path,
    FILE *tarfile,
    struct header_posix_ustar *hpu
);

#endif