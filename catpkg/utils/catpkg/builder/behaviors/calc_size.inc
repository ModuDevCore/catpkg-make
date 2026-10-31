/*
 * ============================================================
 * Size calculation debug
 * ============================================================
 *
 * 1 - enabled
 * 0 - disabled
 */

#define CATPKG_CALC_SIZE_DEBUG 0

#if CATPKG_CALC_SIZE_DEBUG

    #define CALC_SIZE_DEBUG(protocol, value)                         \
        fprintf(                                                     \
            stderr,                                                  \
            "[CALC_SIZE] %-28s %10lld B | %s\n",                    \
            protocol,                                                \
            (long long)*change_size,                                 \
            (value) != NULL ? (value) : "-"                          \
        )

#else

    #define CALC_SIZE_DEBUG(protocol, value) \
        ((void)0)

#endif


case MOVE_FILE:
    /*
     * Moving within the same filesystem does not
     * change allocated disk space.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MOVE_FILE",
        request_val
    );

    break;


case MOVE_DIR:
    /*
     * Moving within the same filesystem does not
     * change allocated disk space.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MOVE_DIR",
        request_val
    );

    break;


case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "CTPG_REMOVE_DIR_OBSOLETE"
            : "REMOVE_DIR";

    if (request_val == NULL)
        return 1;

    if (
        request->protocol ==
        CTPG_REMOVE_DIR_OBSOLETE
    ) {
        // int is_obsolete =
        //     catpkg_builder_ctpg_path_obsolete(
        //         ctpg_context,
        //         request_val,
        //         1
        //     );

        // if (is_obsolete < 0)
        //     return 1;

        // if (is_obsolete == 0) {
        //     *change_size = 0;

        //     char debug_path[PATH_MAX];

        //     snprintf(
        //         debug_path,
        //         sizeof(debug_path),
        //         "%s (NOT OBSOLETE)",
        //         request_val
        //     );

        //     CALC_SIZE_DEBUG(
        //         protocol_name,
        //         debug_path
        //     );

        //     break;
        // }
    }

    struct stat st;

    if (
        lstat(
            request_val,
            &st
        ) != 0
    ) {
        return 1;
    }

    if (!S_ISDIR(st.st_mode))
        return 1;

    /*
     * POSIX st_blocks is measured in
     * 512-byte units.
     */
    *change_size =
        -(
            (off_t)st.st_blocks *
            512
        );

    CALC_SIZE_DEBUG(
        protocol_name,
        request_val
    );

    break;
}


case REMOVE_FILE:
case CTPG_REMOVE_FILE_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_FILE_OBSOLETE
            ? "CTPG_REMOVE_FILE_OBSOLETE"
            : "REMOVE_FILE";

    if (request_val == NULL)
        return 1;

    if (
        request->protocol ==
        CTPG_REMOVE_FILE_OBSOLETE
    ) {
        // int is_obsolete =
        //     catpkg_builder_ctpg_file_obsolete(
        //         ctpg_context,
        //         request_val
        //     );

        // if (is_obsolete < 0)
        //     return 1;

        // if (is_obsolete == 0) {
        //     *change_size = 0;

        //     char debug_path[PATH_MAX];

        //     snprintf(
        //         debug_path,
        //         sizeof(debug_path),
        //         "%s (NOT OBSOLETE)",
        //         request_val
        //     );

        //     CALC_SIZE_DEBUG(
        //         protocol_name,
        //         debug_path
        //     );

        //     break;
        // }
    }

    struct stat st;

    if (
        lstat(
            request_val,
            &st
        ) != 0
    ) {
        return 1;
    }

    *change_size =
        -(
            (off_t)st.st_blocks *
            512
        );

    CALC_SIZE_DEBUG(
        protocol_name,
        request_val
    );

    break;
}


case MKDIR:
{
    if (request_val == NULL)
        return 1;

    char path[PATH_MAX];

    if (
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        ) >= (int)sizeof(path)
    ) {
        return 1;
    }

    char *parent =
        dirname(path);

    struct statvfs fs;

    if (
        statvfs(
            parent,
            &fs
        ) != 0
    ) {
        return 1;
    }

    /*
     * Directory does not exist yet, so its exact
     * allocation cannot be queried.
     *
     * Estimate one filesystem block.
     */
    *change_size =
        (off_t)fs.f_bsize;

    CALC_SIZE_DEBUG(
        "MKDIR",
        request_val
    );

    break;
}


case MKDIR_SOFT:
{
    if (request_val == NULL)
        return 1;

    char path[PATH_MAX];

    if (
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        ) >= (int)sizeof(path)
    ) {
        return 1;
    }

    size_t created = 0;

    while (1) {
        struct stat st;

        if (
            stat(
                path,
                &st
            ) == 0
        ) {
            if (!S_ISDIR(st.st_mode))
                return 1;

            struct statvfs fs;

            if (
                statvfs(
                    path,
                    &fs
                ) != 0
            ) {
                return 1;
            }

            *change_size =
                (off_t)created *
                (off_t)fs.f_bsize;

            CALC_SIZE_DEBUG(
                "MKDIR_SOFT",
                request_val
            );

            break;
        }

        if (errno != ENOENT)
            return 1;

        created++;

        char *parent =
            dirname(path);

        if (parent == NULL)
            return 1;

        if (strcmp(parent, path) == 0)
            return 1;

        memmove(
            path,
            parent,
            strlen(parent) + 1
        );
    }

    break;
}


case MKFILE:
{
    if (request_val == NULL)
        return 1;

    /*
     * Empty regular file:
     * no data blocks are allocated.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MKFILE",
        request_val
    );

    break;
}


case SYMLINK:
{
    if (request_val == NULL)
        return 1;

    /*
     * Symlink allocation is ignored for now.
     * Short symlinks are commonly stored directly
     * in the inode.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "SYMLINK",
        request_val
    );

    break;
}


case CTPG_EXTRACT:
{
    // char **args = (char **)request->value;
    
    // char *source =
    //     args[0];

    // char *destination =
    //     args[1];

    // if (
    //     catpkg_builder_ctpg_extract_size(
    //         ctpg_context,
    //         request_val,
    //         change_size,
    //         NULL
    //     ) != 0
    // ) {
    //     return 1;
    // }

    // CALC_SIZE_DEBUG(
    //     "CTPG_EXTRACT",
    //     request_val
    // );

    break;
}


case CTPG_REGISTER_METADATA_PACKAGEINFO:
{
    if (
        catpkg_builder_ctpg_extract_size(
            ctpg_context,
            CATPKG_PACKAGEINFO,
            change_size,
            NULL
        ) != 0
    ) {
        return 1;
    }

    CALC_SIZE_DEBUG(
        "CTPG_REGISTER_METADATA_PACKAGEINFO",
        CATPKG_PACKAGEINFO
    );

    break;
}

case CATPKG_CREATE_FILEINFO:
{
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "CATPKG_CREATE_FILEINFO ( NC )",
        request_val
    );
    break;
}

case CATPKG_MERGE_DATABASE:
{
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "CATPKG_CREATE_FILEINFO ( NC )",
        request_val
    );

    break;
}


default:
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "UNKNOWN/IGNORED",
        request_val
    );

    break;