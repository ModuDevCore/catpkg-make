case CATPKG_MERGE_DATABASE:
    CATPKG_BUILD_LOG_TREE(1);
    
    if (
        catpkg_builder_cache_extract(
            CATPKG_DATABASE_DIR_PATH,
            "/"
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "CATPKG_MERGE_DATABASE",
            CATPKG_DATABASE_DIR_PATH
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "CATPKG_MERGE_DATABASE",
        CATPKG_DATABASE_DIR_PATH
    );
    break;

case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "REVERT_CTPG_REMOVE_DIR_OBSOLETE"
            : "REVERT_REMOVE_DIR";

    if (request_val == NULL)
        return 1;

    if (
        request->protocol ==
        CTPG_REMOVE_DIR_OBSOLETE
    ) {
        int is_obsolete =
            catpkg_builder_ctpg_path_obsolete(
                ctpg_context,
                request_val,
                1
            );

        if (is_obsolete < 0)
            return 1;

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "REVERT_CTPG_REMOVE_DIR_OBSOLETE: \"%s\" Nothing to restore.",
                "( NOT REMOVED )",
                0,
                request_val
            );

            break;
        }
    }

    CATPKG_BUILD_LOG_TREE(1);

    if (
        catpkg_builder_cache_extract(
            request_val,
            "/"
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
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
            ? "REVERT_CTPG_REMOVE_FILE_OBSOLETE"
            : "REVERT_REMOVE_FILE";

    if (request_val == NULL)
        return 1;


    /*
     * CTPG_REMOVE_FILE_OBSOLETE:
     *
     * If the file is NOT obsolete, APPLY did not
     * remove it.
     *
     * This includes files which:
     *
     * - exist in the package payload;
     * - are declared by #persistent-file.
     *
     * Therefore there is nothing to restore.
     */

    if (
        request->protocol ==
        CTPG_REMOVE_FILE_OBSOLETE
    ) {
        int is_obsolete =
            catpkg_builder_ctpg_file_obsolete(
                ctpg_context,
                request_val
            );

        if (is_obsolete < 0)
            return 1;

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "%s",
                "NOT REMOVED",
                0,
                "REVERT_CTPG_REMOVE_FILE_OBSOLETE: Nothing to restore."
            );

            break;
        }
    }


    /*
     * The file was actually removed.
     * Restore it from the builder cache.
     */

    CATPKG_BUILD_LOG_TREE(1);

    if (
        catpkg_builder_cache_extract(
            request_val,
            "/"
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        protocol_name,
        request_val
    );

    break;
}


case REMOVE:

    /*
     * TODO
     */

    break;

/*
 * MOVE_FILE
 *
 * APPLY:
 *     source -> destination
 *
 * REVERT:
 *     destination -> source
 *
 * If APPLY failed before destination was created,
 * there is nothing to revert.
 */

case MOVE_FILE: {

    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);


    /*
     * If destination does not exist, APPLY did not
     * complete the move.
     */

    if (access(destination, F_OK) != 0) {

        if (errno != ENOENT) {
            fprintf(
                stderr,
                "catpkg: access \"%s\": ",
                destination
            );

            perror(NULL);

            CATPKG_BUILD_LOG_MOVE_ERROR(
                "REVERT_MOVE_FILE",
                destination,
                source
            );

            return 1;
        }

        CATPKG_BUILD_LOG(
            "%s",
            "NOT MOVED",
            0,
            "REVERT_MOVE_FILE: Nothing to restore."
        );

        break;
    }


    /*
     * The file was moved.
     * Move it back.
     */

    if (catpkg_move_file(
            destination,
            source
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "REVERT_MOVE_FILE",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "REVERT_MOVE_FILE",
        source,
        destination
    );

    break;
}


/*
 * MOVE_DIR
 */

case MOVE_DIR: {

    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);


    /*
     * If destination does not exist, APPLY did not
     * complete the move.
     */

    if (access(destination, F_OK) != 0) {

        if (errno != ENOENT) {
            fprintf(
                stderr,
                "catpkg: access \"%s\": ",
                destination
            );

            perror(NULL);

            CATPKG_BUILD_LOG_MOVE_ERROR(
                "REVERT_MOVE_DIR",
                destination,
                source
            );

            return 1;
        }

        CATPKG_BUILD_LOG(
            "%s",
            "NOT MOVED",
            0,
            "REVERT_MOVE_DIR: Nothing to restore."
        );

        break;
    }


    /*
     * The directory was moved.
     * Move it back.
     */

    if (catpkg_move_directory(
            destination,
            source
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "REVERT_MOVE_DIR",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "REVERT_MOVE_DIR",
        source,
        destination
    );

    break;
}


/*
 * MKDIR
 *
 * If APPLY failed before creating the directory,
 * REVERT must do nothing.
 */

case MKDIR: {

    if (request_val == NULL)
        return 1;

    CATPKG_BUILD_LOG_TREE(1);

    struct stat st;

    if (lstat(request_val, &st) != 0) {

        if (errno != ENOENT) {
            fprintf(
                stderr,
                "catpkg: lstat \"%s\": ",
                request_val
            );

            perror(NULL);

            CATPKG_BUILD_LOG_PATH_ERROR(
                "REVERT_MKDIR",
                request_val
            );

            return 1;
        }

        CATPKG_BUILD_LOG(
            "%s",
            "NOT CREATED",
            0,
            "REVERT_MKDIR: Nothing to remove."
        );

        break;
    }


    /*
     * Something exists at the path.
     * Make sure it is actually a directory.
     */

    if (!S_ISDIR(st.st_mode)) {
        fprintf(
            stderr,
            "catpkg: cannot revert MKDIR \"%s\": "
            "path exists but is not a directory\n",
            request_val
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_MKDIR",
            request_val
        );

        return 1;
    }


    /*
     * Directory was created by APPLY.
     */

    if (catpkg_remove_directory(
            request_val
        ) != 0) {

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_MKDIR",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_MKDIR",
        request_val
    );

    break;
}

case MKDIR_SOFT:
{
    CATPKG_BUILD_LOG_TREE(1);

    struct MkdirSoftContext *context =
        request->context;

    if (context == NULL) {
        CATPKG_BUILD_LOG(
            "%s",
            "NOT CREATED",
            0,
            "REVERT_MKDIR_SOFT: Nothing to restore."
        );

        break;
    }


    /*
     * Paths are stored in creation order:
     *
     * /a\0
     * /a/b\0
     * /a/b/c\0
     *
     * They must be removed in reverse order:
     *
     * /a/b/c
     * /a/b
     * /a
     */

    for (
        size_t i = context->created_paths_size;
        i > 0;
        i--
    ) {
        char *path =
            context->data;


        /*
         * Find path [i - 1].
         */

        for (
            size_t j = 0;
            j < i - 1;
            j++
        ) {
            path +=
                strlen(path) + 1;
        }


        /*
         * The path was recorded only after mkdir()
         * succeeded, therefore it belongs to this
         * MKDIR_SOFT operation.
         */

        if (rmdir(path) != 0) {

            /*
             * ENOENT means that there is already
             * nothing left to revert.
             */

            if (errno == ENOENT) {
                CATPKG_BUILD_LOG(
                    "%s",
                    "NOT EXIST",
                    0,
                    path
                );

                continue;
            }

            fprintf(
                stderr,
                "catpkg: rmdir \"%s\": ",
                path
            );

            perror(NULL);

            CATPKG_BUILD_LOG_PATH_ERROR(
                "REVERT_MKDIR_SOFT",
                path
            );

            return 1;
        }

        CATPKG_BUILD_LOG_PATH_OK(
            "REVERT_MKDIR_SOFT",
            path
        );
    }


    CATPKG_BUILD_LOG(
        "%s",
        "ok",
        0,
        "REVERT_MKDIR_SOFT"
    );

    break;
}

/*
 * MKFILE
 *
 * Same principle as MKDIR.
 */

case MKFILE: {

    if (request_val == NULL)
        return 1;

    CATPKG_BUILD_LOG_TREE(1);

    struct stat st;

    if (lstat(request_val, &st) != 0) {

        if (errno != ENOENT) {
            fprintf(
                stderr,
                "catpkg: lstat \"%s\": ",
                request_val
            );

            perror(NULL);

            CATPKG_BUILD_LOG_PATH_ERROR(
                "REVERT_MKFILE",
                request_val
            );

            return 1;
        }

        CATPKG_BUILD_LOG(
            "%s",
            "NOT CREATED",
            0,
            "REVERT_MKFILE: Nothing to remove."
        );

        break;
    }


    /*
     * Do not accidentally remove another object type.
     */

    if (!S_ISREG(st.st_mode)) {
        fprintf(
            stderr,
            "catpkg: cannot revert MKFILE \"%s\": "
            "path exists but is not a regular file\n",
            request_val
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_MKFILE",
            request_val
        );

        return 1;
    }

    if (unlink(request_val) != 0) {

        fprintf(
            stderr,
            "catpkg: unlink \"%s\": ",
            request_val
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_MKFILE",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_MKFILE",
        request_val
    );

    break;
}


/*
 * SYMLINK
 *
 * APPLY may fail before link_path is created.
 */

case SYMLINK: {

    if (request_val == NULL)
        return 1;

    char *link_path =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);

    struct stat st;

    /*
     * lstat(), not stat().
     *
     * A dangling symlink still exists and must be
     * removed during REVERT.
     */

    if (lstat(link_path, &st) != 0) {

        if (errno != ENOENT) {
            fprintf(
                stderr,
                "catpkg: lstat \"%s\": ",
                link_path
            );

            perror(NULL);

            CATPKG_BUILD_LOG_PATH_ERROR(
                "REVERT_SYMLINK",
                link_path
            );

            return 1;
        }

        CATPKG_BUILD_LOG(
            "%s",
            "NOT CREATED",
            0,
            "REVERT_SYMLINK: Nothing to remove."
        );

        break;
    }


    /*
     * Do not remove an unrelated object if something
     * unexpected appeared at link_path.
     */

    if (!S_ISLNK(st.st_mode)) {
        fprintf(
            stderr,
            "catpkg: cannot revert SYMLINK \"%s\": "
            "path exists but is not a symbolic link\n",
            link_path
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_SYMLINK",
            link_path
        );

        return 1;
    }

    if (unlink(link_path) != 0) {

        fprintf(
            stderr,
            "catpkg: unlink \"%s\": ",
            link_path
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_SYMLINK",
            link_path
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_SYMLINK",
        link_path
    );

    break;
}


case SU:

    CATPKG_BUILD_LOG_TREE(1);

    if (seteuid(
            request->previous_euid
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: seteuid(%u): ",
            (unsigned int)
            request->previous_euid
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_SU",
            "ERROR"
        );

        return 1;
    }

    char uid_buffer[32];

    snprintf(
        uid_buffer,
        sizeof(uid_buffer),
        "%u",
        (unsigned int)
        request->previous_euid
    );

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_SU",
        uid_buffer
    );

    break;


case BUILD_LOG_START:

    CATPKG_BUILD_LOG_TREE(1);
    CATPKG_BUILD_LOG(
        "BUILD_LOG_END",
        "",
        1,
        ""
    );

    build_log_active = 0;

    break;


case BUILD_LOG_END:

    build_log_active = 1;

    CATPKG_BUILD_LOG_TREE(1);
    CATPKG_BUILD_LOG(
        "BUILD_LOG_START",
        "",
        1,
        ""
    );

    break;

case NONE:

    break;