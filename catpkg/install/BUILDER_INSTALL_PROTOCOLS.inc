#ifndef BUILDER_PROGRESS_INFO_TITLE
#error "BUILDER_PROGRESS_INFO_TITLE "
#endif
#ifndef BUILDER_PROGRESS_PACKAGES_COUNT
#error "BUILDER_PROGRESS_PACKAGES_COUNT "
#endif
#ifndef BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT
#error "BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT "
#endif


/*
 * --------------------------------------------------------
 * Prepare transaction
 * --------------------------------------------------------
 */

char package_proggress_info_buffer[2048];

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Preparing to install the package..."
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "PREPARATION OF THE PACKAGE"
);

/*
 * Cache the currently installed package if necessary.
 *
 * The protocol itself determines the size contribution.
 */

catpkg_builder_request_after_free(
    &builder,
    CTPG_TO_CACHE,
    strdup(package_fullname)
);
/*
 * --------------------------------------------------------
 * Prepare package directories
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG,
    "Prepare package directories"
);

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Preparing to install the package...\n( Prepare package directories )"
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);

catpkg_builder_request_after_free(
    &builder,
    MKDIR_SOFT,
    strdup(metadata_path)
);

catpkg_builder_request_after_free(
    &builder,
    MKDIR_SOFT,
    strdup(files_path)
);

catpkg_builder_request_after_free(
    &builder,
    MKDIR_SOFT,
    strdup(package_path)
);

catpkg_builder_request(&builder, FILEINFO_EXCLUDE, NULL);
catpkg_builder_request(
    &builder,
    MKDIR_SOFT,
    CATPKG_DATABASE_DIR_PATH
);

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "#prot operations"
);

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Preparing to install the package...\n( #prot operations... )"
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);

const struct PackageField *operation_field = PackageInfo_Find(info, "operations");

while (operation_field != NULL) {
    if (strcmp("#prot", operation_field->name) == 0) {
        char *copy = strdup(operation_field->value);

        char *arg1 = strtok(copy, " ");
        char *arg2 = strtok(NULL, " ");

        size_t size = strlen(arg2) + 1;

        char *request_val = malloc(size);
        if (request_val == NULL) {
            free(copy);
            return 1;
        }

        memcpy(request_val, arg2, size);

        for (size_t i = 0; i < size - 1; i++) {
            if (request_val[i] == ':')
                request_val[i] = '\0';
        }

        catpkg_builder_request_after_free(
            &builder,
            strtol(arg1, NULL, 10),
            request_val
        );

        free(copy);
    }

    operation_field = operation_field->next_field;
}

/*
 * --------------------------------------------------------
 * Create #symlink entries
 *
 * The protocol receives:
 *
 *     target\0link
 *
 * The allocated value belongs to us and is freed only
 * after apply/revert.
 * --------------------------------------------------------
 */

const struct PackageFieldMatches symlink_fields =
    PackageInfo_FindAll(
        info,
        "#symlink"
    );

for (size_t i = 0; i < symlink_fields.count; ++i) {

    const char *separator =
        strstr(symlink_fields.items[i] -> value, " -> ");

    if (separator == NULL)
        goto prepare_error;

    size_t target_size =
        (size_t)(separator - symlink_fields.items[i] -> value);

    const char *link =
        separator + 4;

    size_t value_size =
        target_size +
        1 +
        strlen(link) +
        1;

    char *value = malloc(value_size);

    if (value == NULL)
        goto prepare_error;

    memcpy(
        value,
        symlink_fields.items[i] -> value,
        target_size
    );

    value[target_size] = '\0';

    memcpy(
        value + target_size + 1,
        link,
        strlen(link) + 1
    );

    if (
        catpkg_builder_request_after_free(
            &builder,
            SYMLINK,
            value
        ) != 0
    ) {
        free(value);
        goto prepare_error;
    }
}



/*
 * --------------------------------------------------------
 * Extract package payload
 * --------------------------------------------------------
 */

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Extarct package payload..."
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Extarct package payload..."
);

catpkg_builder_request(
    &builder,
    CTPG_EXTRACT_PAYLOAD,
    NULL
);


/*
 * --------------------------------------------------------
 * Extract PACKAGEINFO
 * --------------------------------------------------------
 */

#ifdef CATPKG_TRANSACTION_UPDATE

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Update PACKAGEINFO"
);

const char *remove_val =
    INSTALL_VAL(
        name_field
    ) -> value;

const char *install_val =
    package_info_path;

size_t remove_size =
    strlen(remove_val) + 1;

size_t install_size =
    strlen(install_val) + 1;

char *update_packageinfo_val =
    malloc(
        remove_size +
        install_size
    );

if (update_packageinfo_val == NULL)
    goto prepare_error;

memcpy(
    update_packageinfo_val,
    remove_val,
    remove_size
);

memcpy(
    update_packageinfo_val + remove_size,
    install_val,
    install_size
);

catpkg_builder_request_after_free(
    &builder,
    CTPG_UPDATE_PACKAGEINFO,
    update_packageinfo_val
);

#else

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Register PACKAGEINFO"
);

catpkg_builder_request_after_free(
    &builder,
    CTPG_REGISTER_METADATA_PACKAGEINFO,
    strdup(metadata_path)
);

#endif



/*
 * --------------------------------------------------------
 * Generate FileInfo
 * --------------------------------------------------------
 */

sprintf(package_proggress_info_buffer, PACKAGE_PROGRESS_INFO_SHEME, BUILDER_PROGRESS_INFO_TITLE,
    package_fullname, BUILDER_PROGRESS_PACKAGES_CURRENT_COUNT, BUILDER_PROGRESS_PACKAGES_COUNT,
    "Generate FileInfo..."
);

catpkg_builder_request_after_free(
    &builder,
    SET_PROGRESS_INFO,
    strdup(package_proggress_info_buffer) 
);