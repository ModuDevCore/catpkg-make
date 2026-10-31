#ifndef CATPKG_UTILS_BUILDER_H
#define CATPKG_UTILS_BUILDER_H
#include <stddef.h>
#include <sys/types.h>
#include <catpkg/pkginfo.h>
#include "./builder/protocols.h"
#include "catpkg/ctpg.h"

enum CatpkgErrorCode {
    CATPKG_OK = 0,
    CATPKG_DEPENDENCY_ERROR = 1,
    CATPKG_INVALID_CONTEXT = 2,
    CATPKG_NOT_FOUND = 3
};

struct CatpkgError {
    enum CatpkgErrorCode code;
    const char *path;
    const char *required_by;
};

struct FILEINFO_Entry {
    char **names;
    char **values;

    size_t records_size;
};

struct OPSeC_Context {
    struct PackageInfo *package_info;
    struct PackageInfo *file_info;

    struct OPSeC_Context *last_opsec_context;

    char **G_package_ids;   // Package reference for analysis within the context.
    struct FILEINFO_Entry *G_entries_ids;   // Entry reference for analysis within the context.

    uint64_t p ;            // Operation package mask 
    uint64_t p_page; 
    uint64_t p_mask; 

    uint64_t *G_pages;
    uint64_t *R_pages;      // Packages belonging to the current section.
    uint64_t *RD_pages;     // Reverse dependencies
    uint64_t *D_pages;      // Dependencies
    uint64_t *P_pages;
    uint64_t *N_pages;      // The total set of current sections.
    uint64_t *C_pages;      // Sections that were affected by protocols in the context of OPSEC.

    size_t G_package_ids_size;
    size_t G_entries_ids_size;

    size_t G_pages_size;
    size_t RD_pages_size;
    size_t D_pages_size;
    size_t N_pages_size;
    size_t C_pages_size;
    size_t R_pages_size;
    size_t P_pages_size;
};

struct FILEINFO_CREATE_Context {
    struct FILEINFO_Entry *G_records_ids;

    size_t G_records_ids_size;

    uint64_t *N_pages;
    uint64_t *P_pages;

    size_t N_pages_size;
    size_t P_pages_size;
};

struct PAYLOAD_EXCLUDE_Context {
    uint64_t *E_pages;

    size_t E_pages_size;
};

struct MkdirSoftContext {
    size_t created_paths_size;
    size_t data_size;

    /*
     * Created paths stored sequentially:
     *
     * path1\0
     * path2\0
     * ...
     */
    char data[];
};

struct CatpkgRequest {
    enum CatpkgProtocol protocol;
    void *value;
    void *result;
    void *context;

    uid_t previous_euid;
    bool value_copy;
    bool skip;
    bool revert;
};

struct CatpkgBuilder {
    struct CatpkgRequest **requests;
    size_t requests_size;
    size_t applied_count;
    off_t change_size;

    int stderr_buf;
};

int catpkg_builder_err_print(
    struct CatpkgBuilder *builder
);

int catpkg_builder_init(
    struct CatpkgBuilder *builder
);

int catpkg_builder_request(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
);

int catpkg_builder_request_value(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value,
    void *result
);

int catpkg_builder_request_after_free(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
);

int catpkg_builder_apply(
    struct CatpkgBuilder *builder
);

int catpkg_builder_revert(
    struct CatpkgBuilder *builder
);

int catpkg_builder_free(
    struct CatpkgBuilder *builder
);
#endif