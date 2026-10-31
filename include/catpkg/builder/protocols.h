#define CATPKG_PROTOCOLS(X)                     \
    X(NONE)                                     \
    X(OPSEC_CREATE)                             \
    X(OPSEC_CHECK)                              \
                                                \
    X(MOVE_FILE)                                \
    X(MOVE_DIR)                                 \
    X(REMOVE_BATCH_OPEN)                        \
    X(REMOVE_BATCH_CLOSE)                       \
    X(REMOVE_DIR)                               \
    X(CTPG_REMOVE_DIR_OBSOLETE)                 \
    X(REMOVE_FILE)                              \
    X(CTPG_REMOVE_FILE_OBSOLETE)                \
    X(REMOVE)                                   \
    X(MKDIR)                                    \
    X(MKDIR_SOFT)                               \
                                                \
    X(MKFILE)                                   \
    X(SYMLINK)                                  \
                                                \
    X(CATPKG_MERGE_DATABASE)                    \
    X(DATABASE_REMOVE_REQUIRED)                 \
    X(DATABASE_OBSOLETE_SECTION)                \
    X(CATPKG_REMOVE_PACKAGE_RECORDS)            \
    X(FILEINFO_EXCLUDE)                         \
    X(CATPKG_CREATE_FILEINFO)                   \
                                                \
    X(PAYLOAD_EXCLUDE)                          \
                                                \
    X(CTPG_OPEN)                                \
    X(CTPG_INFO)                                \
    X(CTPG_MKCMD)                               \
    X(CTPG_READFILE)                            \
    X(CTPG_EXTRACT)                             \
    X(CTPG_TO_CACHE)                            \
    X(CTPG_EXTRACT_PAYLOAD)                     \
    X(CTPG_EXTRACT_PAYLOAD_END)                 \
    X(CTPG_REGISTER_METADATA_PACKAGEINFO)       \
    X(CTPG_UPDATE_PACKAGEINFO)                  \
    X(CTPG_CLOSE)                               \
                                                \
    X(SU)                                       \
                                                \
    X(BUILD_LOG_START)                          \
    X(BUILD_LOG)                                \
    X(BUILD_LOG_SEP)                            \
    X(SET_PROGRESS_INFO)                        \
    X(BUILD_LOG_END)                            \
                                                \
    X(CTPG_MKCMD_BUILD)                         \
    X(CTPG_EXTRACT_MAPPED)                      \
                                                \
    X(EXCEPTION)                                \
    X(FILEINFO_DECLAR_EXIST_NOT_PACKAGE)        \
    X(OPSEC_CHECK_FILEINFO_CONTEXT_CREATE)      \
    X(OPSEC_CHECK_FILEINFO_CONTEXT_UPDATE)

#define X(name) name,
enum CatpkgProtocol {
    CATPKG_PROTOCOLS(X)
};
#undef X

enum OPSEC_FLAGS {
    OPSEC_FLAG_REMOVE  = 1u << 0,
    OPSEC_FLAG_UPDATE  = 1u << 1,
    OPSEC_FLAG_INSTALL = 1u << 2,
    OPSEC_FLAG_PROT_OP = 1u << 3,
};