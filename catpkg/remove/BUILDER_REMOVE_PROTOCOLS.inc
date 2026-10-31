#ifndef BUILDER_PROGRESS_INFO_TITLE
#error "BUILDER_PROGRESS_INFO_TITLE "
#endif
#ifndef BUILDER_PROGRESS_PACKAGES_COUNT
#error "BUILDER_PROGRESS_PACKAGES_COUNT "
#endif
#ifndef BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT
#error "BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT "
#endif

char package_proggress_info_buffer[2048];

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Remove package..."
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);

catpkg_builder_request(
    &builder,
    REMOVE_BATCH_OPEN,
    NULL
);

const struct PackageField *current_field =
    fileinfo->fields;

int type = -1;

while (current_field != NULL) {

    if (
        current_field->name != NULL &&
        strcmp(
            current_field->name,
            "#type"
        ) == 0
    ) {
        if (
            current_field->value != NULL &&
            strcmp(
                current_field->value,
                "directory"
            ) == 0
        ) {
            type = 0;
        }

        else if (
            current_field->value != NULL &&
            (
                strcmp(
                    current_field->value,
                    "file"
                ) == 0 ||
                strcmp(
                    current_field->value,
                    "symlink"
                ) == 0
            )
        ) {
            type = 1;
        }

        else {
            type = -1;
        }
    }

    else if (
        current_field->name != NULL &&
        strcmp(
            current_field->name,
            "#path"
        ) == 0 &&
        current_field->value != NULL
    ) {

#ifdef CATPKG_TRANSACTION_UPDATE
        sprintf(
            package_proggress_info_buffer,
            PACKAGE_PROGRESS_INFO_SHEME,
            BUILDER_PROGRESS_INFO_TITLE,
            package_fullname,
            BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT,
            BUILDER_PROGRESS_PACKAGES_COUNT,
            "\nCleaning up obsolete files..."
        );

        catpkg_builder_request_after_free(
            &builder,
            SET_PROGRESS_INFO,
            strdup(
                package_proggress_info_buffer
            )
        );

        size_t packages_path_len =
            strlen(
                CATPKG_PACKAGES_PATH
            );

        bool package_metadata =
            strncmp(
                current_field->value,
                CATPKG_PACKAGES_PATH,
                packages_path_len
            ) == 0 &&
            (
                current_field->value[
                    packages_path_len
                ] == '/' ||
                current_field->value[
                    packages_path_len
                ] == '\0'
            );

        if (!package_metadata) {
            switch (type) {
                case 0:
                    if (
                        catpkg_builder_request_after_free(
                            &builder,
                            REMOVE_DIR,
                            strdup(
                                current_field->value
                            )
                        ) != 0
                    ) {
                        return 1;
                    }

                    break;

                case 1:
                    if (
                        catpkg_builder_request_after_free(
                            &builder,
                            REMOVE_FILE,
                            strdup(
                                current_field->value
                            )
                        ) != 0
                    ) {
                        return 1;
                    }

                    break;
            }
        }

#else

        sprintf(
            package_proggress_info_buffer,
            PACKAGE_PROGRESS_INFO_SHEME,
            BUILDER_PROGRESS_INFO_TITLE,
            package_fullname,
            BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT,
            BUILDER_PROGRESS_PACKAGES_COUNT,
            "\nRemove packages files..."
        );

        catpkg_builder_request_after_free(
            &builder,
            SET_PROGRESS_INFO,
            strdup(
                package_proggress_info_buffer
            )
        );

        switch (type) {

            case 0:
                if (
                    catpkg_builder_request_after_free(
                        &builder,
                        REMOVE_DIR,
                        strdup(
                            current_field->value
                        )
                    ) != 0
                ) {
                    return 1;
                }

                break;

            case 1:
                if (
                    catpkg_builder_request_after_free(
                        &builder,
                        REMOVE_FILE,
                        strdup(
                            current_field->value
                        )
                    ) != 0
                ) {
                    return 1;
                }

                break;
        }

#endif
    }

    current_field =
        current_field->next_field;
}

catpkg_builder_request(
    &builder,
    REMOVE_BATCH_CLOSE,
    NULL
);