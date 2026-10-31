/*
 * ============================================================
 * BUILD_LOG_START
 * ============================================================
 */

case BUILD_LOG_START:

    build_log_active = 1;

    CATPKG_BUILD_LOG(
        "BUILD_LOG_START",
        "",
        0,
        ""
    );

    break;


/*
 * ============================================================
 * BUILD_LOG_END
 * ============================================================
 */

case BUILD_LOG_END:

    CATPKG_BUILD_LOG(
        "BUILD_LOG_END",
        "",
        0,
        ""
    );

    build_log_active = 0;

    break;

case REMOVE_BATCH_OPEN:
        size_t close_batch_index = 0;
        bool found = false;

        for (size_t ai = i + 1; ai < builder->requests_size; ai++) {
            if (builder->requests[ai] != NULL
                && builder->requests[ai]->protocol == REMOVE_BATCH_CLOSE) {

                close_batch_index = ai;
                found = true;
                break;
            }
        }

        if (!found) {
            return 1;
        }

        size_t count_batch_protocols = close_batch_index - i - 1;
        struct CatpkgRequest **remove_batch_protocols = &builder->requests[i + 1];

        // Files - first
        size_t count_batch_files_protocols = 0;

        for (size_t ai = 0; ai < count_batch_protocols; ai++) {
            if (remove_batch_protocols[ai]->protocol != REMOVE_FILE || remove_batch_protocols[ai]->protocol != CTPG_REMOVE_FILE_OBSOLETE)
                continue;

            if (ai != count_batch_files_protocols) {
                struct CatpkgRequest *tmp =
                    remove_batch_protocols[count_batch_files_protocols];

                remove_batch_protocols[count_batch_files_protocols] =
                    remove_batch_protocols[ai];

                remove_batch_protocols[ai] = tmp;
            }

            count_batch_files_protocols++;
        }

        size_t count_batch_dirs_protocols =
            count_batch_protocols - count_batch_files_protocols;

        struct CatpkgRequest **remove_batch_dirs_protocols =
            &remove_batch_protocols[count_batch_files_protocols];

        for (size_t ai = 0; ai < count_batch_dirs_protocols; ai++) {
            size_t longest_index = ai;

            for (size_t aj = ai + 1;
                 aj < count_batch_dirs_protocols;
                 aj++) {

                const char *current_path =
                    (const char *)remove_batch_dirs_protocols[aj]->value;

                const char *longest_path =
                    (const char *)remove_batch_dirs_protocols[longest_index]->value;

                if (strlen(current_path) > strlen(longest_path)) {
                    longest_index = aj;
                }
            }

            if (longest_index != ai) {
                struct CatpkgRequest *tmp =
                    remove_batch_dirs_protocols[ai];

                remove_batch_dirs_protocols[ai] =
                    remove_batch_dirs_protocols[longest_index];

                remove_batch_dirs_protocols[longest_index] =
                    tmp;
            }
        }
        CATPKG_BUILD_LOG(
            "%s",
            "",
            0,
            "REMOVE_BATCH_OPEN"
        ); 
    break;

case REMOVE_BATCH_CLOSE:
    CATPKG_BUILD_LOG(
        "%s",
        "",
        0,
        "REMOVE_BATCH_CLOSE"
    );
    break;

case DATABASE_REMOVE_REQUIRED:
{
    CATPKG_BUILD_LOG_TREE(0);

    catpkg_build_log(
        "DATABASE_REMOVE_REQUIRED %s\n",
        builder,
        true,
        request_val
    );

    if (
        request_val == NULL ||
        request_val[0] == '\0'
    ) {
        return 1;
    }

    const char *dependencies_path =
        CATPKG_DATABASE_DIR_PATH
        "/"
        CATPKG_DEPENDENCIES;

    FILE *file =
        fopen(
            dependencies_path,
            "rb"
        );

    if (file == NULL) {
        perror(
            "catpkg: failed to open DEPENDENCIES"
        );

        return 1;
    }

    int required_length =
        snprintf(
            NULL,
            0,
            "#required %s",
            request_val
        );

    if (required_length < 0) {
        fclose(file);
        return 1;
    }

    char *required =
        malloc(
            (size_t)required_length + 1
        );

    if (required == NULL) {
        fclose(file);
        return 1;
    }

    snprintf(
        required,
        (size_t)required_length + 1,
        "#required %s",
        request_val
    );

    int temporary_length =
        snprintf(
            NULL,
            0,
            "%s.tmp",
            dependencies_path
        );

    if (temporary_length < 0) {
        free(required);
        fclose(file);
        return 1;
    }

    char *temporary_path =
        malloc(
            (size_t)temporary_length + 1
        );

    if (temporary_path == NULL) {
        free(required);
        fclose(file);
        return 1;
    }

    snprintf(
        temporary_path,
        (size_t)temporary_length + 1,
        "%s.tmp",
        dependencies_path
    );

    FILE *temporary =
        fopen(
            temporary_path,
            "wb"
        );

    if (temporary == NULL) {
        perror(
            "catpkg: failed to create temporary DEPENDENCIES"
        );

        free(temporary_path);
        free(required);
        fclose(file);

        return 1;
    }

    char *line = NULL;
    size_t line_capacity = 0;

    ssize_t line_length;

    while (
        (
            line_length =
                getline(
                    &line,
                    &line_capacity,
                    file
                )
        ) != -1
    ) {
        size_t content_length =
            (size_t)line_length;

        if (
            content_length > 0 &&
            line[content_length - 1] == '\n'
        ) {
            content_length--;
        }

        if (
            content_length > 0 &&
            line[content_length - 1] == '\r'
        ) {
            content_length--;
        }

        bool remove_line =
            content_length ==
                (size_t)required_length &&

            memcmp(
                line,
                required,
                (size_t)required_length
            ) == 0;

        if (remove_line)
            continue;

        if (
            fwrite(
                line,
                1,
                (size_t)line_length,
                temporary
            ) != (size_t)line_length
        ) {
            free(line);

            fclose(temporary);
            fclose(file);

            remove(temporary_path);

            free(temporary_path);
            free(required);

            return 1;
        }
    }

    if (ferror(file)) {
        free(line);

        fclose(temporary);
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(required);

        return 1;
    }

    free(line);

    if (fflush(temporary) != 0) {
        fclose(temporary);
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(required);

        return 1;
    }

    if (fclose(temporary) != 0) {
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(required);

        return 1;
    }

    if (fclose(file) != 0) {
        remove(temporary_path);

        free(temporary_path);
        free(required);

        return 1;
    }

    if (
        rename(
            temporary_path,
            dependencies_path
        ) != 0
    ) {
        perror(
            "catpkg: failed to replace DEPENDENCIES"
        );

        remove(temporary_path);

        free(temporary_path);
        free(required);

        return 1;
    }

    free(temporary_path);
    free(required);

    break;
}

/*
 * ============================================================
 * DATABASE_OBSOLETE_SECTION
 * ============================================================
 */

case DATABASE_OBSOLETE_SECTION:
{
    CATPKG_BUILD_LOG_TREE(0);

    catpkg_build_log(
        "DATABASE_OBSOLETE_SECTION %s\n",
        builder,
        true,
        request_val
    );

    if (
        request_val == NULL ||
        request_val[0] == '\0'
    ) {
        return 1;
    }

    FILE *file =
        fopen(
            CATPKG_DATABASE_DIR_PATH
            "/"
            CATPKG_DEPENDENCIES,
            "ab"
        );

    if (file == NULL) {
        perror(
            "catpkg: failed to open DEPENDENCIES"
        );

        return 1;
    }

    if (
        fprintf(
            file,
            "#obsolete %s\n",
            request_val
        ) < 0
    ) {
        fclose(file);
        return 1;
    }

    if (fflush(file) != 0) {
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0)
        return 1;

    break;
}

case CTPG_MKCMD:
{
    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "CTPG_MKCMD: \"%s\"\n",
        "",
        0,
        request_val
    );

    if (request->context == NULL)
        return 1;

    char *script =
        catpkg_builder_ctpg_read_data(
            ctpg_context,
            request_val
        );

    if (script == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "CTPG_MKCMD",
            request_val
        );

        return 1;
    }

    char *commands_data_path =
        make_catpkg_path(
            "/var/lib/%s/.catpkg/commands",
            ctpg_context->name_field->value
        );

    if (commands_data_path == NULL) {
        free(script);
        return 1;
    }

    char *command_basename =
        basename(
            request_val
        );

    char *command_data_path =
        make_catpkg_path(
            "/var/lib/%s/.catpkg/commands/%s",
            ctpg_context->name_field->value,
            command_basename
        );

    if (command_data_path == NULL) {
        free(commands_data_path);
        free(script);

        return 1;
    }

    if (
        mkdir(
            commands_data_path,
            0755
        ) != 0 &&
        errno != EEXIST
    ) {
        free(commands_data_path);
        free(command_data_path);
        free(script);

        return 1;
    }

    FILE *command_file =
        fopen(
            command_data_path,
            "wb"
        );

    if (command_file == NULL) {
        free(commands_data_path);
        free(command_data_path);
        free(script);

        return 1;
    }

    size_t script_size =
        strlen(script);

    if (
        script_size > 0 &&
        fwrite(
            script,
            1,
            script_size,
            command_file
        ) != script_size
    ) {
        fclose(command_file);

        free(commands_data_path);
        free(command_data_path);
        free(script);

        return 1;
    }

    if (
        fclose(
            command_file
        ) != 0
    ) {
        free(commands_data_path);
        free(command_data_path);
        free(script);

        return 1;
    }

    free(script);

    if (
        symlink(
            command_data_path,
            request->context
        ) != 0
    ) {
        catpkg_build_log(
            "symlink: File exists\n",
            builder,
            true
        );
    }

    catpkg_build_log(
        "The symbolic link \"%s\" "
        "has been successfully created.\n",
        builder,
        true,
        request->context
    );

    free(commands_data_path);
    free(command_data_path);

    break;
}


/*
 * ============================================================
 * MOVE_FILE
 * ============================================================
 *
 * value:
 *
 * source\0destination\0
 */

case MOVE_FILE: {

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_TREE(0);
        CATPKG_BUILD_LOG_PATH_ERROR("MOVE_FILE", "???");
        return 1;
    }

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(0);

    if (catpkg_move_file(
            source,
            destination
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "MOVE_FILE",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "MOVE_FILE",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * MOVE_DIR
 * ============================================================
 */

case MOVE_DIR: {

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_TREE(0);
        CATPKG_BUILD_LOG_PATH_ERROR("MOVE_DIR", "???");
        return 1;
    }

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(0);

    if (catpkg_move_directory(
            source,
            destination
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "MOVE_DIR",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "MOVE_DIR",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * REMOVE_DIR / CTPG_REMOVE_DIR_OBSOLETE
 * ============================================================
 */

case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    CATPKG_BUILD_LOG_TREE(0);

    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "CTPG_REMOVE_DIR_OBSOLETE"
            : "REMOVE_DIR";

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            "???"
        );

        return 1;
    }

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

        if (is_obsolete < 0) {
            CATPKG_BUILD_LOG_PATH_ERROR(
                protocol_name,
                request_val
            );

            return 1;
        }

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "CTPG_REMOVE_DIR_OBSOLETE: \"%s\" (It exists in the package.)",
                "NOT OBSOLETE",
                0,
                request_val
            );

            break;
        }
    }

    if (
        catpkg_builder_cache_change(
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    if (rmdir(request_val) != 0) {
        fprintf(
            stderr,
            "catpkg: rmdir \"%s\": ",
            request_val
        );

        perror(NULL);

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

/*
 * ============================================================
 * REMOVE_FILE / CTPG_REMOVE_FILE_OBSOLETE
 * ============================================================
 */

case REMOVE_FILE:
case CTPG_REMOVE_FILE_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_FILE_OBSOLETE
            ? "CTPG_REMOVE_FILE_OBSOLETE"
            : "REMOVE_FILE";

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            "???"
        );

        return 1;
    }


    /*
     * CTPG_REMOVE_FILE_OBSOLETE:
     *
     * Remove the file only if it is obsolete.
     *
     * A file is NOT obsolete when:
     *
     * - it exists in the currently opened package payload;
     * - it is declared by #persistent-file.
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

        if (is_obsolete < 0) {
            CATPKG_BUILD_LOG_PATH_ERROR(
                protocol_name,
                request_val
            );

            return 1;
        }

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "%s",
                "NOT OBSOLETE",
                0,
                "CTPG_REMOVE_FILE_OBSOLETE: It exists in the package or is persistent."
            );

            break;
        }
    }

    if (
        catpkg_builder_cache_change(
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
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

/*
 * ============================================================
 * REMOVE
 * ============================================================
 */

case REMOVE:

    CATPKG_BUILD_LOG_TREE(0);

    /*
     * TODO
     */

    CATPKG_BUILD_LOG(
        "%s",
        "TODO",
        0,
        "REMOVE"
    );

    break;


/*
 * ============================================================
 * BUILD_LOG
 * ============================================================
 */

/*
 * ============================================================
 * SET_PROGRESS_INFO
 * ============================================================
 */

case SET_PROGRESS_INFO:
{
    progress_info_height = 0;

    if (request_val == NULL)
        return 1;

    for (
        const char *check = request_val;
        *check != '\0';
        check++
    ) {
        if (*check == '\n')
            progress_info_height++;
    }

    progress_info = request_val;

    builder_progress_bar(
        builder->applied_count + 1,
        builder->requests_size,
        stdout_fd
    );

    break;
}

case BUILD_LOG:
    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "BUILD_LOG",
            "???"
        );

        return 1;
    }

    catpkg_build_log("%s\n", builder, true, request_val);
    break;

/*
 * ============================================================
 * BUILD_LOG_SEP
 * ============================================================
 */

case BUILD_LOG_SEP:
    size_t width = console_width(stdout_fd);

    size_t title_length = strlen(request_val);

    /*
     * "\n" + '-' + "**" + request_val + "**" + '-' + "\n"
     */
    size_t prefix_size = 4 + title_length;

    if (width <= prefix_size)
        return 1;

    size_t dash_count = width - prefix_size;

    size_t left_dashes = dash_count / 2;
    size_t right_dashes = dash_count - left_dashes;

    size_t buffer_size =
        1 +
        left_dashes +
        2 +
        title_length +
        2 +
        right_dashes +
        1 +
        1;

    char *buffer = malloc(buffer_size);

    if (buffer == NULL)
        return 1;

    char *ptr = buffer;

    *ptr++ = '|';
    *ptr++ = '\n';

    memset(ptr, '-', left_dashes);
    ptr += left_dashes;

    *ptr++ = '*';
    *ptr++ = '*';

    memcpy(ptr, request_val, title_length);
    ptr += title_length;

    *ptr++ = '*';
    *ptr++ = '*';

    memset(ptr, '-', right_dashes);
    ptr += right_dashes;

    *ptr++ = '\n';
    *ptr = '\0';

    catpkg_build_log(buffer, builder, true);

    free(buffer);

    break;

/*
 * ============================================================
 * MKDIR
 * ============================================================
 */

case MKDIR:

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR",
            "???"
        );

        return 1;
    }

    if (
        mkdir(
            request_val,
            0755
        ) != 0
    ) {
        int mkdir_errno = errno;

        fprintf(
            stderr,
            "catpkg: mkdir \"%s\": ",
            request_val
        );

        errno = mkdir_errno;
        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR",
            request_val
        );

        return mkdir_errno;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "MKDIR",
        request_val
    );

    break;


/*
 * ============================================================
 * MKDIR_SOFT
 * ============================================================
 */

case MKDIR_SOFT:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR_SOFT",
            "???"
        );

        return 1;
    }

    CATPKG_BUILD_LOG(
        "MKDIR_SOFT_START: \"%s\"",
        "",
        0,
        request_val
    );

    struct MkdirSoftContext *context =
        calloc(
            1,
            sizeof(*context)
        );

    if (context == NULL) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "MKDIR_SOFT: calloc"
        );

        return 1;
    }

    request->context =
        context;


    char path[PATH_MAX];

    int path_length =
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        );

    if (
        path_length < 0 ||
        path_length >= (int)sizeof(path)
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "MKDIR_SOFT: path too long"
        );

        goto mkdir_soft_error;
    }


    char *p = path;

    if (*p == '/')
        p++;


    for (;;) {

        char *slash =
            strchr(p, '/');

        if (slash != NULL)
            *slash = '\0';


        struct stat st;

        if (stat(path, &st) != 0) {

            if (errno != ENOENT) {

                fprintf(
                    stderr,
                    "catpkg: stat \"%s\": ",
                    path
                );

                perror(NULL);

                CATPKG_BUILD_LOG_PATH_ERROR(
                    "MKDIR",
                    path
                );

                goto mkdir_soft_error;
            }


            /*
             * Directory does not exist.
             * Create it.
             */

            if (mkdir(path, 0755) != 0) {

                fprintf(
                    stderr,
                    "catpkg: mkdir \"%s\": ",
                    path
                );

                perror(NULL);

                CATPKG_BUILD_LOG_PATH_ERROR(
                    "MKDIR",
                    path
                );

                goto mkdir_soft_error;
            }


            /*
             * The directory now belongs to this
             * MKDIR_SOFT operation.
             *
             * Save its path directly inside the
             * context allocation.
             */

            size_t path_size =
                strlen(path) + 1;

            /*
             * Overflow protection.
             */

            if (
                context->data_size >
                SIZE_MAX -
                    sizeof(*context) -
                    path_size
            ) {
                CATPKG_BUILD_LOG(
                    "%s",
                    "ERROR",
                    0,
                    "MKDIR_SOFT: context size overflow"
                );

                goto mkdir_soft_error;
            }

            size_t new_context_size =
                sizeof(*context) +
                context->data_size +
                path_size;


            struct MkdirSoftContext *tmp =
                realloc(
                    context,
                    new_context_size
                );

            if (tmp == NULL) {

                CATPKG_BUILD_LOG(
                    "%s",
                    "ERROR",
                    0,
                    "MKDIR_SOFT: realloc"
                );

                goto mkdir_soft_error;
            }


            /*
             * realloc() may have moved the block.
             */

            context = tmp;

            request->context =
                context;


            /*
             * Append:
             *
             * path\0
             */

            memcpy(
                context->data +
                    context->data_size,
                path,
                path_size
            );

            context->data_size +=
                path_size;

            context->created_paths_size++;


            CATPKG_BUILD_LOG_PATH_OK(
                "MKDIR",
                path
            );
        }
        else if (!S_ISDIR(st.st_mode)) {

            fprintf(
                stderr,
                "catpkg: \"%s\" is not a directory\n",
                path
            );

            CATPKG_BUILD_LOG_PATH_ERROR(
                "MKDIR",
                path
            );

            goto mkdir_soft_error;
        }


        if (slash == NULL)
            break;


        /*
         * Restore slash removed for the current
         * path component.
         */

        *slash = '/';

        p = slash + 1;

        while (*p == '/')
            p++;
    }


    if (context->created_paths_size == 0) {
        CATPKG_BUILD_LOG(
            "%s",
            "EXIST",
            0,
            "MKDIR_SOFT_END"
        );
    }
    else {
        CATPKG_BUILD_LOG(
            "%s",
            "OK",
            0,
            "MKDIR_SOFT_END"
        );
    }

    break;


mkdir_soft_error:

    request->context =
        context;

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "ERROR",
        0,
        "MKDIR_SOFT_END"
    );

    return 1;
}


/*
 * ============================================================
 * MKFILE
 * ============================================================
 */

case MKFILE:

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "TODO",
        0,
        "MKFILE"
    );

    break;


/*
 * ============================================================
 * SYMLINK
 * ============================================================
 *
 * value:
 *
 * target\0link_path\0
 */

case SYMLINK: {

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "SYMLINK",
            "???",
            "???"
        );

        return 1;
    }

    char *target =
        request_val;

    char *link_path =
        request_val +
        strlen(request_val) + 1;

    if (
        symlink(
            target,
            link_path
        ) != 0
    ) {
        int symlink_errno = errno;

        fprintf(
            stderr,
            "catpkg: symlink \"%s\": ",
            link_path
        );

        errno = symlink_errno;
        perror(NULL);

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "SYMLINK",
            target,
            link_path
        );

        return symlink_errno;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "SYMLINK",
        target,
        link_path
    );

    break;
}


/*
 * ============================================================
 * SU
 * ============================================================
 */

case SU: {

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            "???"
        );

        return 1;
    }

    uid_t uid;

    if (catpkg_parse_uid(
            request_val,
            &uid
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: invalid UID \"%s\"\n",
            request_val
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            request_val
        );

        return 1;
    }

    request->previous_euid =
        geteuid();

    if (seteuid(uid) != 0) {

        fprintf(
            stderr,
            "catpkg: seteuid(%u): ",
            (unsigned int)uid
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "SU",
        request_val
    );

    break;
}

/*
 * ============================================================
 * EXCEPTION
 * ============================================================
 */

case EXCEPTION:
{
    long error_code =
        strtol(
            request->context,
            NULL,
            10
        );

    if (
        request_val == NULL ||
        strcmp(
            request_val,
            "EEXIST"
        ) != 0
    ) {
        fprintf(
            stderr,
            "EXCEPTION incorrect type: %s\n",
            request_val != NULL
                ? request_val
                : "(null)"
        );

        return 1;
    }

    if ((int)error_code != EEXIST)
        return 1;

    size_t next_index =
        i + 1;

    if (next_index >= builder->requests_size)
        return 1;

    struct CatpkgRequest *next_request =
        builder->requests[next_index];

    if (next_request == NULL)
        return 1;

    next_request->skip = false;

    if (
        ftruncate(
            builder->stderr_buf,
            0
        ) == -1
    ) {
        perror("ftruncate");
    }

    break;
}

/*
 * ============================================================
 * CTPG_EXTRACT_MAPPED
 * ============================================================
 *
 * value:
 *
 * source\0destination\0
 */

case CTPG_EXTRACT_MAPPED:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (
        request_val == NULL ||
        ctpg_context == NULL
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_MAPPED"
        );

        return 1;
    }

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    if (
        catpkg_builder_catpackage_extract_mapped(
            ctpg_context,
            source,
            destination,
            NULL
        ) != 0
    ) {
        CATPKG_BUILD_LOG_MOVE_ERROR(
            "CTPG_EXTRACT_MAPPED",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "CTPG_EXTRACT_MAPPED",
        source,
        destination
    );

    break;
}

case CTPG_OPEN:

    CATPKG_BUILD_LOG_TREE(0);

    if (
        request_val == NULL ||
        ctpg_context_init(
            &ctpg_context,
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "CTPG_OPEN",
            request_val != NULL
                ? request_val
                : "???"
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "CTPG_OPEN",
        request_val
    );

    break;


/*
 * ============================================================
 * CTPG_EXTRACT_PAYLOAD
 * ============================================================
 */

#define CATPKG_REWRITE_FIELD(field, new_value)                          \
    do {                                                                \
        if ((field) == NULL ||                                          \
            (field)->value == NULL ||                                   \
            (new_value) == NULL)                                        \
            goto rewrite_error;                                         \
                                                                        \
        size_t _old_size = strlen((field)->value);                      \
        size_t _new_size = strlen((new_value));                         \
                                                                        \
        /*                                                              \
         * We cannot overwrite the following bytes of PACKAGEINFO.      \
         */                                                             \
        if (_new_size > _old_size)                                      \
            goto rewrite_error;                                         \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Clear the old value. */                                      \
                                                                        \
        for (size_t _i = 0; _i < _old_size; _i++) {                    \
            if (fputc(' ', file) == EOF)                                \
                goto rewrite_error;                                     \
        }                                                               \
                                                                        \
        /* Return to the beginning of the value. */                     \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Write the new value. */                                      \
                                                                        \
        if (fwrite(                                                      \
                (new_value),                                            \
                1,                                                      \
                _new_size,                                              \
                file                                                    \
            ) != _new_size) {                                           \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
    } while (0)


#define CATPKG_REWRITE_OPTION_FIELD(field, new_name, new_value)          \
    do {                                                                 \
        if ((field) == NULL ||                                          \
            (field)->name == NULL ||                                    \
            (field)->value == NULL ||                                   \
            (new_name) == NULL ||                                       \
            (new_value) == NULL)                                        \
            goto rewrite_error;                                         \
                                                                        \
        size_t _old_size =                                               \
            strlen((field)->name) +                                     \
            1 +                                                         \
            strlen((field)->value);                                     \
                                                                        \
        size_t _new_size =                                               \
            strlen((new_name)) +                                        \
            1 +                                                         \
            strlen((new_value));                                        \
                                                                        \
        /*                                                              \
         * The new option line must fit into the old option line.        \
         */                                                             \
        if (_new_size > _old_size)                                      \
            goto rewrite_error;                                         \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Clear the old option line. */                                \
                                                                        \
        for (size_t _i = 0; _i < _old_size; _i++) {                    \
            if (fputc(' ', file) == EOF)                                \
                goto rewrite_error;                                     \
        }                                                               \
                                                                        \
        /* Return to the beginning of the option line. */               \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Write: #option value */                                      \
                                                                        \
        if (fprintf(                                                     \
                file,                                                   \
                "%s %s",                                                \
                (new_name),                                             \
                (new_value)                                             \
            ) < 0) {                                                    \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
    } while (0)


#define CATPKG_CLEAR_OPTION_FIELD(field)                                 \
    do {                                                                 \
        if ((field) == NULL ||                                           \
            (field)->name == NULL ||                                     \
            (field)->value == NULL)                                      \
            goto rewrite_error;                                          \
                                                                         \
        size_t _old_size =                                                \
            strlen((field)->name) +                                      \
            1 +                                                          \
            strlen((field)->value);                                      \
                                                                         \
        if (fseek(                                                        \
                file,                                                    \
                (field)->bytepos,                                        \
                SEEK_SET                                                 \
            ) != 0) {                                                    \
            goto rewrite_error;                                          \
        }                                                                \
                                                                         \
        for (size_t _i = 0; _i < _old_size; _i++) {                     \
            if (fputc(' ', file) == EOF)                                 \
                goto rewrite_error;                                      \
        }                                                                \
                                                                         \
    } while (0)


#define FIELD_IS(field, field_name, field_value)                         \
    ((field) != NULL &&                                                 \
     (field)->name != NULL &&                                           \
     (field)->value != NULL &&                                          \
     strcmp((field)->name, (field_name)) == 0 &&                        \
     strcmp((field)->value, (field_value)) == 0)


#define FIELD_NAME(field, field_name)                                    \
    ((field) != NULL &&                                                 \
     (field)->name != NULL &&                                           \
     strcmp((field)->name, (field_name)) == 0)

#undef FIELD_IS
#undef FIELD_NAME

#undef CATPKG_CLEAR_OPTION_FIELD
#undef CATPKG_REWRITE_OPTION_FIELD
#undef CATPKG_REWRITE_FIELD

case CTPG_INFO:

    CATPKG_BUILD_LOG_TREE(0);

    if (
        ctpg_context == NULL ||
        request->result == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_INFO"
        );

        return 1;
    }

    *(struct PackageInfo **)request->result =
        ctpg_context->pkginfo;

    CATPKG_BUILD_LOG(
        "CTPG_INFO\n|   Package: %s@%s",
        "OK",
        0,
        CATPKG_BUILD_LOG_CTPG_ARGS(
            ctpg_context->name_field->value,
            ctpg_context->version_field->value
        )
    );

    break;


/*
 * ============================================================
 * CTPG_EXTRACT
 * ============================================================
 *
 * value:
 *
 * source\0destination\0
 */

case CTPG_EXTRACT: {

    CATPKG_BUILD_LOG_TREE(0);

    if (
        request_val == NULL ||
        ctpg_context == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT"
        );

        return 1;
    }

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    if (catpkg_builder_catpackage_extract(
            ctpg_context,
            source,
            destination,
            NULL
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "CTPG_EXTRACT",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "CTPG_EXTRACT",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * CTPG_CLOSE
 * ============================================================
 */

case CTPG_CLOSE:

    CATPKG_BUILD_LOG_TREE(0);

    if (ctpg_context == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_CLOSE: no context"
        );

        return 1;
    }

    ctpg_context_close(
        &ctpg_context
    );

    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "CTPG_CLOSE"
    );

    break;

case OPSEC_CHECK_FILEINFO_CONTEXT_UPDATE:
case OPSEC_CHECK_FILEINFO_CONTEXT_CREATE:
{
    struct CatpkgRequest *catpkg_create_fileinfo =
        builder->requests[i + 1];

    CATPKG_BUILD_LOG_TREE(0);

    if (catpkg_create_fileinfo == NULL) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "OPSEC_CHECK_FILEINFO_CONTEXT: The CATPKG_CREATE_FILEINFO protocol, which should follow OPSEC_CHECK_FILEINFO_CONTEXT, was not found."
        );

        return 1;
    }

    if (catpkg_create_fileinfo->protocol != CATPKG_CREATE_FILEINFO) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "OPSEC_CHECK_FILEINFO_CONTEXT: The CATPKG_CREATE_FILEINFO protocol must follow OPSEC_CHECK_FILEINFO_CONTEXT."
        );

        return 1;
    }

    const struct CatpkgRequest *current_opsec_create = NULL;

    for (size_t ai = builder->applied_count; ai-- > 0;) {
        if (builder->requests[ai]->protocol == OPSEC_CREATE) {
            current_opsec_create = builder->requests[ai];
            break;
        }
    }

    if (current_opsec_create == NULL)
        return 1;

    struct OPSeC_Context *opsec_context =
        current_opsec_create->context;

    catpkg_create_fileinfo->context =
        malloc(sizeof(struct FILEINFO_CREATE_Context));

    if (catpkg_create_fileinfo->context == NULL)
        return 1;

    struct FILEINFO_CREATE_Context *fileinfo_context =
        catpkg_create_fileinfo->context;

    fileinfo_context->G_records_ids =
        opsec_context->G_entries_ids;

    fileinfo_context->G_records_ids_size =
        opsec_context->G_entries_ids_size;

    fileinfo_context->N_pages =
        opsec_context->C_pages;

    fileinfo_context->N_pages_size =
        opsec_context->C_pages_size;

    fileinfo_context->P_pages =
        opsec_context->P_pages;

    fileinfo_context->P_pages_size =
        opsec_context->P_pages_size;

    break;
}


/*
 * ============================================================
 * CATPKG_CREATE_FILEINFO
 * ============================================================
 */

case CATPKG_CREATE_FILEINFO:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CATPKG_CREATE_FILEINFO: invalid path"
        );

        return 1;
    }

    CATPKG_BUILD_LOG(
        "CATPKG_CREATE_FILEINFO_START: \"%s\"",
        "",
        0,
        request_val
    );

    struct FILEINFO_CREATE_Context *fileinfo_context =
        request->context;

    if (fileinfo_context == NULL)
        return 1;

    char *path_copy =
        strdup(request_val);

    if (path_copy == NULL) {
        perror("strdup");
        return 1;
    }

    char *policy_path =
        make_catpkg_path(
            "%s/%s",
            dirname(path_copy),
            "POLICY"
        );

    if (policy_path == NULL) {
        free(path_copy);
        return 1;
    }

    FILE *fileinfo =
        fopen(
            request_val,
            "w"
        );

    if (fileinfo == NULL) {
        perror("catpkg: fopen FileInfo");

        free(policy_path);
        free(path_copy);

        return 1;
    }

    FILE *filepolicy =
        fopen(
            policy_path,
            "w"
        );

    if (filepolicy == NULL) {
        perror("catpkg: fopen POLICY");

        fclose(fileinfo);

        free(policy_path);
        free(path_copy);

        return 1;
    }

    if (
        fprintf(
            fileinfo,
            "CATPKG 1.1\n"
            "#catpkg-version 2.0.0-beta\n"
            "#use-options POLICY\n"
        ) < 0
    )
        goto fileinfo_error;

    size_t record_id = 2;

    if (
        fprintf(
            fileinfo,
            "-\n"
            "#type file\n"
            "#path %s\n",
            request_val
        ) < 0 ||
        fprintf(
            fileinfo,
            "-\n"
            "#type file\n"
            "#path %s\n",
            policy_path
        ) < 0
    )
        goto fileinfo_error;

    for (
        size_t i = 0;
        i < fileinfo_context->G_records_ids_size;
        i++
    ) {
        size_t page = i / 64;

        uint64_t mask =
            UINT64_C(1) << (63 - (i % 64));

        if (
            page >= fileinfo_context->N_pages_size ||
            !(fileinfo_context->N_pages[page] & mask)
        )
            continue;

        struct FILEINFO_Entry *entry =
            &fileinfo_context->G_records_ids[i];

        if (fprintf(fileinfo, "-\n") < 0)
            goto fileinfo_error;

        for (
            size_t ai = 0;
            ai < entry->records_size;
            ai++
        ) {
            if (
                fprintf(
                    fileinfo,
                    "#%s %s\n",
                    entry->names[ai],
                    entry->values[ai]
                ) < 0
            )
                goto fileinfo_error;
        }

        if (
            page < fileinfo_context->P_pages_size &&
            (fileinfo_context->P_pages[page] & mask)
        ) {

            if (
                fprintf(
                    filepolicy,
                    "#protected %zu\n",
                    record_id
                ) < 0
            )
                goto fileinfo_error;
        }

        record_id++;
    }

    fclose(filepolicy);
    fclose(fileinfo);

    free(policy_path);
    free(path_copy);

    break;

fileinfo_error:

    fclose(filepolicy);
    fclose(fileinfo);

    free(policy_path);
    free(path_copy);

    CATPKG_BUILD_LOG(
        "%s",
        "ERROR",
        0,
        "CATPKG_CREATE_FILEINFO"
    );

    return 1;
}

case CATPKG_REMOVE_PACKAGE_RECORDS:
{
    char *metadata_path = make_catpkg_path(CATPKG_METADATA_PATH, request_val);
    char *files_path = make_catpkg_path(CATPKG_FILES_PATH, request_val);
    char *package_path = make_catpkg_path(CATPKG_PACKAGE_PATH, request_val);

    char *info_path = make_catpkg_path(CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO, request_val);
    char *fileinfo_path = make_catpkg_path(CATPKG_FILES_PATH "/" CATPKG_FILE, request_val);
    
    if (unlink(info_path) != 0){
        return 1;
    }
    if (unlink(fileinfo_path) != 0)
        return 1;

    if (rmdir(metadata_path) != 0)
        return 1;
    if (rmdir(files_path) != 0)
        return 1;
    if (rmdir(package_path) != 0)
        return 1;

    CATPKG_BUILD_LOG_PATH_OK(
        "CATPKG_REMOVE_PACKAGE_RECORDS",
        request_val
    );
    break;
}

case CATPKG_MERGE_DATABASE:
{
    CATPKG_BUILD_LOG_TREE(0);

    catpkg_build_log(
        "CATPKG_MERGE_DATABASE \n",
        builder,
        true
    );

    char request_dir[PATH_MAX];

    int request_dir_len =
        snprintf(
            request_dir,
            sizeof(request_dir),
            "%s",
            request_val
        );

    if (
        request_dir_len < 0 ||
        (size_t)request_dir_len >= sizeof(request_dir)
    ) {
        return 1;
    }

    char *database_dir =
        dirname(request_dir);

    char *depend_path =
        make_catpkg_path(
            "%s/%s",
            database_dir,
            CATPKG_DEPENDENCIES
        );

    char *policy_path =
        make_catpkg_path(
            "%s/%s",
            database_dir,
            CATPKG_POLICY
        );

    if (
        depend_path == NULL ||
        policy_path == NULL
    ) {
        free(depend_path);
        free(policy_path);
        return 1;
    }

    if (
        catpkg_builder_cache_change(
            CATPKG_DATABASE_DIR_PATH
        ) != 0
    ) {
        free(depend_path);
        free(policy_path);
        return 1;
    };

    if (
        access(
            request_val,
            F_OK
        ) != 0
    ) {
        FILE *file =
            fopen(
                request_val,
                "w"
            );

        if (file == NULL) {
            perror("fopen");
            free(depend_path);
            free(policy_path);
            return 1;
        }

        if (
            fprintf(
                file,
                "CATPKG 1.1\n"
                "#catpkg-version 2.0.0-beta\n"
                "#use-options DEPENDENCIES\n"
                "#use-options POLICY\n"
            ) < 0
        ) {
            fclose(file);
            free(depend_path);
            free(policy_path);
            return 1;
        }

        if (fclose(file) != 0) {
            perror("fclose");
            free(depend_path);
            free(policy_path);
            return 1;
        }
    }

    FILE *dependencies_file =
        fopen(
            depend_path,
            "w"
        );

    if (dependencies_file == NULL) {
        perror("fopen");
        free(depend_path);
        free(policy_path);
        return 1;
    }

    FILE *policy_file =
        fopen(
            policy_path,
            "w"
        );

    if (policy_file == NULL) {
        perror("fopen");
        fclose(dependencies_file);
        free(depend_path);
        free(policy_path);
        return 1;
    }

    struct PackageInfo *database_fileinfo =
        CATPKG_Parse(
            request_val,
            NULL
        );
    CATPKG_ReadAll(database_fileinfo);

    if (database_fileinfo == NULL) {
        fclose(policy_file);
        fclose(dependencies_file);
        free(depend_path);
        free(policy_path);
        return 1;
    }

    FILE *database_file =
        fopen(
            request_val,
            "a"
        );

    if (database_file == NULL) {
        perror("fopen");

        PackageInfo_Free(
            database_fileinfo
        );

        fclose(policy_file);
        fclose(dependencies_file);

        free(depend_path);
        free(policy_path);

        return 1;
    }

    size_t last_database_section =
        0;

    const struct PackageField *database_field =
        PackageInfo_Find(
            database_fileinfo,
            "info"
        );

    while (database_field != NULL) {
        if (
            database_field->name != NULL &&
            strcmp(
                database_field->name,
                "info"
            ) == 0 &&
            database_field->section >
                last_database_section
        ) {
            last_database_section =
                database_field->section;
        }

        database_field =
            database_field->next_field;
    }
    DIR *dir =
        opendir(
            CATPKG_PACKAGES_PATH
        );

    if (dir == NULL) {
        perror("opendir");

        fclose(database_file);
        fclose(policy_file);
        fclose(dependencies_file);

        PackageInfo_Free(
            database_fileinfo
        );

        free(depend_path);
        free(policy_path);

        return 1;
    }

    struct dirent *entry;

    size_t packages_count =
        catpkg_directory_count(
            CATPKG_PACKAGES_PATH
        );

    size_t package_count =
        0;

    while (
        (entry = readdir(dir)) != NULL
    ) {
        if (
            strcmp(
                entry->d_name,
                "."
            ) == 0 ||
            strcmp(
                entry->d_name,
                ".."
            ) == 0
        ) {
            continue;
        }

        char *package_fileinfo_path =
            make_catpkg_path(
                CATPKG_FILES_PATH "/"
                CATPKG_FILE,
                entry->d_name
            );

        char *package_info_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH "/"
                CATPKG_PACKAGEINFO,
                entry->d_name
            );

        if (
            package_fileinfo_path == NULL ||
            package_info_path == NULL
        ) {
            free(package_fileinfo_path);
            free(package_info_path);
            goto merge_error;
        }

        struct PackageInfo *package_info =
            CATPKG_Parse(
                package_info_path,
                NULL
            );

        CATPKG_ReadSections(package_info, 6);

        struct PackageInfo *package_fileinfo =
            CATPKG_Parse(
                package_fileinfo_path,
                NULL
            );

        CATPKG_ReadAll(package_fileinfo);

        free(package_fileinfo_path);
        free(package_info_path);

        if (
            package_info == NULL ||
            package_fileinfo == NULL
        ) {
            PackageInfo_Free(package_info);
            PackageInfo_Free(package_fileinfo);
            goto merge_error;
        }

        const struct PackageField *name_field =
            PackageInfo_Find(
                package_info,
                "name"
            );

        if (
            name_field == NULL ||
            name_field->value == NULL ||
            name_field->value[0] == '\0'
        ) {
            PackageInfo_Free(package_fileinfo);
            PackageInfo_Free(package_info);
            goto merge_error;
        }

        const struct PackageField *info_field =
            PackageInfo_Find(
                package_fileinfo,
                "info"
            );


        while (info_field != NULL) {
            if (
                info_field->name == NULL ||
                strcmp(
                    info_field->name,
                    "info"
                ) != 0
            ) {
                info_field =
                    info_field->next_field;

                continue;
            }

            const char *package_type =
                NULL;

            const char *package_path =
                NULL;

            const struct PackageField *package_field =
                package_fileinfo->fields;

            while (package_field != NULL) {
                if (
                    package_field->section ==
                        info_field->section &&
                    package_field->name != NULL
                ) {
                    if (
                        strcmp(
                            package_field->name,
                            "#type"
                        ) == 0
                    ) {
                        package_type =
                            package_field->value;
                    }

                    else if (
                        strcmp(
                            package_field->name,
                            "#path"
                        ) == 0
                    ) {
                        package_path =
                            package_field->value;
                    }
                }

                package_field =
                    package_field->next_field;
            }

            if (
                package_type == NULL ||
                package_path == NULL
            ) {
                info_field =
                    info_field->next_field;

                continue;
            }

            size_t database_section =
                0;
            int code_resolve_entry = catpkg_merge_database_get_or_resolve_entry(
                    database_fileinfo,
                    &last_database_section,
                    package_type,
                    package_path,
                    &database_section
                );

            if (
                code_resolve_entry < 0
            ) {
                PackageInfo_Free(package_fileinfo);
                PackageInfo_Free(package_info);
                goto merge_error;
            }

            if(code_resolve_entry == 0)
                if (
                    fprintf(
                        database_file,
                        "-\n"
                        "#type %s\n"
                        "#path %s\n",
                        package_type,
                        package_path
                    ) < 0
                ) {
                    return 1;
                }

            if (database_section == 0) {
                PackageInfo_Free(package_fileinfo);
                PackageInfo_Free(package_info);
                goto merge_error;
            }

            if (
                fprintf(
                    dependencies_file,
                    "\n// PROTECTED SECTION INFO\n//  type: %s\n//  path: %s\n#required %zu=%s\n",
                    package_type,
                    package_path,
                    database_section - 1,
                    name_field->value
                ) < 0
            ) {
                PackageInfo_Free(package_fileinfo);
                PackageInfo_Free(package_info);
                goto merge_error;
            }

            const struct PackageField *protected_field =
                PackageInfo_Find(package_fileinfo, "#protected");

            while (protected_field != NULL) {
                if (
                    protected_field->section == 0 &&
                    strcmp(protected_field->name, "#protected") == 0 &&
                    (size_t)strtol(protected_field->value, NULL, 10) ==
                        info_field->section - 1
                ) {
                    break;
                }

                protected_field = protected_field->next_field;
            }

            if (
                protected_field != NULL &&
                fprintf(
                    policy_file,
                    "\n// PROTECTED SECTION INFO\n//  type: %s\n//  path: %s\n#protected %zu\n",
                    package_type,
                    package_path,
                    database_section - 1
                ) < 0
            ) {
                PackageInfo_Free(package_fileinfo);
                PackageInfo_Free(package_info);
                goto merge_error;
            }

            info_field =
                info_field->next_field;
        }

        PackageInfo_Free(
            package_fileinfo
        );

        PackageInfo_Free(
            package_info
        );

        package_count++;
    }

    closedir(
        dir
    );

    if (
        fclose(
            policy_file
        ) != 0
    ) {
        perror("fclose");
        fclose(dependencies_file);
        fclose(database_file);
        PackageInfo_Free(database_fileinfo);
        free(depend_path);
        free(policy_path);
        return 1;
    }

    if (
        fclose(
            dependencies_file
        ) != 0
    ) {
        perror("fclose");
        fclose(database_file);
        PackageInfo_Free(database_fileinfo);
        free(depend_path);
        free(policy_path);
        return 1;
    }

    if (
        fclose(
            database_file
        ) != 0
    ) {
        perror("fclose");
        PackageInfo_Free(database_fileinfo);
        free(depend_path);
        free(policy_path);
        return 1;
    }

    PackageInfo_Free(
        database_fileinfo
    );

    free(
        depend_path
    );

    free(
        policy_path
    );

    catpkg_build_log(
        "|       └── OK\n",
        builder,
        true
    );

    break;

merge_error:

    closedir(
        dir
    );

    fclose(
        policy_file
    );

    fclose(
        dependencies_file
    );

    fclose(
        database_file
    );

    PackageInfo_Free(
        database_fileinfo
    );

    free(
        depend_path
    );

    free(
        policy_path
    );

    return 1;
}


/*
 * ============================================================
 * NONE
 * ============================================================
 */

case NONE:

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "SKIP",
        0,
        "NONE"
    );

    break;