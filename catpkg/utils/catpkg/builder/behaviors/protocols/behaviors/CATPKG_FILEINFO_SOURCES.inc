case FILEINFO_DECLAR_EXIST_NOT_PACKAGE:
    struct stat st;

    if (lstat(request_val, &st) != 0) {
        perror("lstat");
        return 1;
    }
    

    if (S_ISLNK(st.st_mode)) {
        FIELD_SEP(INFO);
        X(INFO, "type", "symlink");
        X(INFO, "path", request_val);
    }
    else if (S_ISREG(st.st_mode)) {
        FIELD_SEP(INFO);
        X(INFO, "type", "file")
        X(INFO, "path", request_val)
    }
    else if (S_ISDIR(st.st_mode)) {
        FIELD_SEP(INFO);
        X(INFO, "type", "directory")
        X(INFO, "path", request_val)
    }
    else {
        FIELD_SEP(INFO);
        X(INFO, "type", "file")
        X(INFO, "path", request_val)
    }

    X(POLICY, "protected", sap_section_index + 1);

    sap_section_index++;
    break;

case FILEINFO_EXCLUDE:
    ai++; // Skip next
    break;

case MOVE_FILE:
{
    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "file");
    X(INFO, "path", destination);
    X(INFO, "source", source);

    sap_section_index++;

    break;
}

case MOVE_DIR:
{
    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "directory");
    X(INFO, "path", destination);
    X(INFO, "source", source);

    sap_section_index++;

    break;
}

case MKDIR:
{
    if (request_val == NULL)
        return 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "directory");
    X(INFO, "path", request_val);

    sap_section_index++;

    break;
}

case MKDIR_SOFT:
{
    const char *path = request->value;

    if (path == NULL)
        return 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "directory");
    X(INFO, "path", path);

    sap_section_index++;

    break;
}

case SYMLINK:
{
    if (request_val == NULL)
        return 1;

    char *target =
        request_val;

    char *link_path =
        target +
        strlen(target) + 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "symlink");
    X(INFO, "path", link_path);
    X(INFO, "target", target);

    sap_section_index++;

    break;
}

case MKFILE:

    break;

case CTPG_EXTRACT:
{
    char *path_prefix =
        catpkg_normalize_tar_path(
            request_val
        );

    const char *destination =
        request_val +
        strlen(request_val) + 1;

    FILE *tarfile =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (tarfile == NULL) {
        perror(
            "catpkg: failed to open package"
        );

        free(path_prefix);

        return 1;
    }

    size_t prefix_len =
        strlen(path_prefix);

    struct header_posix_ustar hpu;

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {
        char size_string[
            sizeof(hpu.size) + 1
        ];

        memcpy(
            size_string,
            hpu.size,
            sizeof(hpu.size)
        );

        size_string[
            sizeof(hpu.size)
        ] = '\0';

        off_t data_size =
            (off_t)strtoull(
                size_string,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        if (
            fseek(
                tarfile,
                (long)(data_size + padding),
                SEEK_CUR
            ) != 0
        ) {
            perror(
                "catpkg: failed to skip TAR entry"
            );

            return 1;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {
            return 1;
        }

        if (
            is_excluded_tar_path(
                correct_path,
                (char *[]) {
                    CATPKG_PACKAGEINFO,
                    NULL
                },
                1
            ) != 0
        ) {
            free(correct_path);
            continue;
        }

        if (
            prefix_len != 0 &&
            (
                strncmp(
                    correct_path,
                    path_prefix,
                    prefix_len
                ) != 0 ||
                (
                    correct_path[prefix_len] != '\0' &&
                    correct_path[prefix_len] != '/'
                )
            )
        ) {
            free(correct_path);
            continue;
        }

        const char *relative_path =
            correct_path;

        while (*relative_path == '/')
            relative_path++;

        size_t destination_len =
            strlen(destination);

        while (
            destination_len > 1 &&
            destination[destination_len - 1] == '/'
        ) {
            destination_len--;
        }

        char *exported_to;

        if (
            destination_len == 1 &&
            destination[0] == '/'
        ) {
            exported_to =
                make_catpkg_path(
                    "/%s",
                    relative_path
                );
        }
        else {
            exported_to =
                make_catpkg_path(
                    "%.*s/%s",
                    (int)destination_len,
                    destination,
                    relative_path
                );
        }

        if (exported_to == NULL) {
            free(correct_path);

            return 1;
        }

        switch (hpu.typeflag[0]) {
            case '\0':
            case '0':

                FIELD_SEP(INFO);
                X(INFO, "type", "file");
                X(INFO, "path", exported_to);

                sap_section_index++;

                break;

            case '2':

                FIELD_SEP(INFO);
                X(INFO, "type", "symlink");
                X(INFO, "path", exported_to);
                X(INFO, "target", hpu.linkname);

                sap_section_index++;

                break;

            default:
                // Skip
                break;
        }

        free(exported_to);
        free(correct_path);
    }

    fclose(tarfile);
    free(path_prefix);
    break;
}
case CTPG_EXTRACT_MAPPED:
{
    char *mapped_path =
        catpkg_normalize_tar_path(
            request_val
        );

    if (mapped_path == NULL)
        return 1;

    const char *destination =
        request_val +
        strlen(request_val) + 1;

    FILE *tarfile =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (tarfile == NULL) {
        perror(
            "catpkg: failed to open package"
        );

        free(mapped_path);

        return 1;
    }

    struct header_posix_ustar hpu;

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {
        char size_string[
            sizeof(hpu.size) + 1
        ];

        memcpy(
            size_string,
            hpu.size,
            sizeof(hpu.size)
        );

        size_string[
            sizeof(hpu.size)
        ] = '\0';

        off_t data_size =
            (off_t)strtoull(
                size_string,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        if (
            fseek(
                tarfile,
                (long)(data_size + padding),
                SEEK_CUR
            ) != 0
        ) {
            perror(
                "catpkg: failed to skip TAR entry"
            );

            fclose(tarfile);
            free(mapped_path);

            return 1;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {
            fclose(tarfile);
            free(mapped_path);

            return 1;
        }

        if (
            strcmp(
                correct_path,
                mapped_path
            ) != 0
        ) {
            free(correct_path);
            continue;
        }

        switch (hpu.typeflag[0]) {
            case '\0':
            case '0':

                FIELD_SEP(INFO);
                X(INFO, "type", "file");
                X(INFO, "path", destination);

                sap_section_index++;

                break;

            case '2':

                FIELD_SEP(INFO);
                X(INFO, "type", "symlink");
                X(INFO, "path", destination);
                X(INFO, "target", hpu.linkname);

                sap_section_index++;

                break;

            default:
                break;
        }

        free(correct_path);
        break;
    }

    fclose(tarfile);
    free(mapped_path);

    break;
}

case CTPG_MKCMD:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL
    ) {
        return 1;
    }

    char *command_data_path =
        make_catpkg_path(
            CATPKG_PACKAGE_DATA
            "/.catpkg/commands/%s",
            ctpg_context->name_field->value,
            basename(request_val)
        );

    if (
        command_data_path == NULL
    ) {
        return 1;
    }


    char *command_symlink_path = request -> context;

    FIELD_SEP(INFO);
    X(INFO, "type", "file");
    X(INFO, "path", command_data_path);

    FIELD_SEP(INFO);
    X(INFO, "type", "symlink");
    X(INFO, "path", command_symlink_path);
    X(INFO, "target", command_data_path);

    SEP_REMOVE_RECORD(INFO);
    REMOVE_RECORD("type", "directory");
    REMOVE_RECORD("path", request_val);

    break;
}

case CTPG_REGISTER_METADATA_PACKAGEINFO:
case CTPG_UPDATE_PACKAGEINFO:
{
    if (ctpg_context == NULL) {

        return 1;
    }

    char *catpkg_PACKAGEINFO_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            ctpg_context->full_name
        );

    if (catpkg_PACKAGEINFO_path == NULL)
        return 1;

    FIELD_SEP(INFO);
    X(INFO, "type", "file");
    X(INFO, "path", catpkg_PACKAGEINFO_path);

    free(catpkg_PACKAGEINFO_path);

    sap_section_index++;

    break;
}

case REMOVE_DIR:
    SEP_REMOVE_RECORD(INFO);
    REMOVE_RECORD("type", "directory");
    REMOVE_RECORD("path", request_val);
    break;
case REMOVE_FILE:
    SEP_REMOVE_RECORD(INFO);
    REMOVE_RECORD("type", "file");
    REMOVE_RECORD("path", request_val);
    break;

/*
 * ------------------------------------------------
 * Non-persistent protocols
 * ------------------------------------------------
 */

case SU:
case BUILD_LOG:
case BUILD_LOG_START:
case BUILD_LOG_END:
case CTPG_OPEN:
case CTPG_INFO:
case CTPG_CLOSE:

    break;


default:

    // Skip

    break;