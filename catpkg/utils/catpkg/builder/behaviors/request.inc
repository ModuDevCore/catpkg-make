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


case PAYLOAD_EXCLUDE:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "PAYLOAD_EXCLUDE: invalid context"
        );

        return 1;
    }

    const struct CatpkgRequest *last_payload_exclude = NULL;

    for (size_t ai = builder->requests_size - 1; ai-- > 0;) {
        if (
            builder->requests[ai]->protocol ==
            PAYLOAD_EXCLUDE
        ) {
            last_payload_exclude =
                builder->requests[ai];

            break;
        }
    }

    struct PAYLOAD_EXCLUDE_Context *payload_exclude_context = NULL;
    
    if (last_payload_exclude == NULL) {
        payload_exclude_context = malloc(sizeof(*payload_exclude_context));
        payload_exclude_context -> E_pages = NULL;
        payload_exclude_context -> E_pages_size = 0;

    }
    else {
        payload_exclude_context = last_payload_exclude->context;
    }
    
    request->context = payload_exclude_context;

    struct header_posix_ustar hpu;
    size_t current_tar_header_id = 0;

    FILE *tarfile = fopen(ctpg_context->path, "r");

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {
        char size_string[13];

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
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR data"
            );

            fclose(tarfile);

            return 1;
        }

        if (
            fseek(
                tarfile,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            fclose(tarfile);

            return 1;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {

            fclose(tarfile);

            return 1;
        }

        if (
            strcmp(correct_path, request_val) == 0
        ) {
            size_t page =
                current_tar_header_id / 64;

            uint64_t mask =
                UINT64_C(1) << (63 - (current_tar_header_id % 64));

            if (page >= payload_exclude_context->E_pages_size) {
                size_t old_size =
                    payload_exclude_context->E_pages_size;

                size_t new_size =
                    page + 1;

                uint64_t *tmp = realloc(
                    payload_exclude_context->E_pages,
                    new_size * sizeof(*payload_exclude_context->E_pages)
                );

                if (tmp == NULL)
                    return 1;

                payload_exclude_context->E_pages =
                    tmp;

                memset(
                    payload_exclude_context->E_pages + old_size,
                    0,
                    (new_size - old_size) *
                    sizeof(*payload_exclude_context->E_pages)
                );

                payload_exclude_context->E_pages_size =
                    new_size;
            }

            payload_exclude_context->E_pages[page] |=
                mask;
        }

        free(correct_path);   
        current_tar_header_id++;
    }
    break;
}

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

case OPSEC_CREATE:
{
    const struct CatpkgRequest *last_opsec_create = NULL;

    for (size_t ai = builder->requests_size; ai-- > 0;) {
        const struct CatpkgRequest *check_request =
            builder->requests[ai];

        if (check_request == request)
            continue;

        if (check_request->protocol == OPSEC_CREATE) {
            last_opsec_create = check_request;
            break;
        }
    }

    struct OPSeC_Context *last_opsec_context = NULL;

    if (last_opsec_create != NULL)
        last_opsec_context =
            (struct OPSeC_Context *)last_opsec_create->context;


    struct PackageInfo *database_fileinfo =
        CATPKG_Parse(
            CATPKG_DATABASE_DIR_PATH
            "/DATABASE",
            NULL
        );

    if (database_fileinfo == NULL)
        return 1;

    CATPKG_ReadAll(
        database_fileinfo
    );

    struct PackageInfo *package_info =
        CATPKG_Parse(
            request_val,
            NULL
        );

    if (package_info == NULL) {
        PackageInfo_Free(
            database_fileinfo
        );

        return 1;
    }

    CATPKG_ReadSections(
        package_info,
        6
    );

    struct OPSeC_Context *new_context =
        malloc(
            sizeof(*new_context)
        );

    if (new_context == NULL) {
        PackageInfo_Free(
            package_info
        );

        PackageInfo_Free(
            database_fileinfo
        );

        return 1;
    }

    new_context->package_info =
        package_info;

    new_context->file_info =
        database_fileinfo;

    new_context->G_package_ids =
        NULL;

    new_context->G_entries_ids =
        NULL;

    new_context->R_pages =
        NULL;
    
    new_context->RD_pages =
        NULL;

    new_context->D_pages =
        NULL;

    new_context->N_pages =
        NULL;

    new_context->C_pages =
        NULL;

    new_context->G_pages =
        NULL;

    new_context->P_pages =
        NULL;

    new_context->p = 0;
    new_context->p_page = 0;
    new_context->p_mask = SIZE_MAX;

    new_context->G_package_ids_size = 0;
    new_context->G_entries_ids_size = 0;

    new_context->RD_pages_size = 0;
    new_context->D_pages_size = 0;
    new_context->N_pages_size = 0;
    new_context->C_pages_size = 0;
    new_context->R_pages_size = 0;
    new_context->G_pages_size = 0;
    new_context->P_pages_size = 0;

    new_context->last_opsec_context = last_opsec_context;
        // An attempt to gather information about the protocols in advance.
        
        const struct PackageField *package_name_field =
            PackageInfo_Find(
                new_context->package_info,
                "name"
            );

        if(last_opsec_context == NULL) {        
            new_context->G_package_ids = malloc(
                database_package_ids_size *
                sizeof(*new_context->G_package_ids)
            );

            if (new_context->G_package_ids == NULL)
                return ENOMEM;

            memcpy(
                new_context->G_package_ids,
                database_package_ids,
                database_package_ids_size *
                sizeof(*new_context->G_package_ids)
            );

            new_context->G_package_ids_size =
                database_package_ids_size;

            new_context->G_pages_size =
                (database_package_ids_size + 63) / 64;

            if (new_context->G_pages_size > 0) {
                new_context->G_pages = calloc(
                    new_context->G_pages_size,
                    sizeof(*new_context->G_pages)
                );

                if (new_context->G_pages == NULL)
                    return ENOMEM;

                for (
                    size_t package_id = 0;
                    package_id < database_package_ids_size;
                    package_id++
                ) {
                    if (database_package_ids[package_id] == NULL)
                        continue;

                    size_t page = package_id / 64;

                    uint64_t mask =
                        UINT64_C(1) << (63 - (package_id % 64));

                    new_context->G_pages[page] |= mask;
                }
            }
        } else {
            new_context->G_package_ids = last_opsec_context->G_package_ids;
            new_context->G_package_ids_size = last_opsec_context->G_package_ids_size;
            
            new_context->G_pages = last_opsec_context->G_pages;
            new_context->G_pages_size = last_opsec_context->G_pages_size;
        }

        size_t package_id = SIZE_MAX;

        for (
            size_t i = 0;
            i < new_context->G_package_ids_size;
            i++
        ) {
            if (
                new_context->G_package_ids[i] != NULL &&
                strcmp(
                    new_context->G_package_ids[i],
                    package_name_field->value
                ) == 0
            ) {
                package_id = i;
                break;
            }
        }

        if (package_id == SIZE_MAX) {
            size_t new_id =
                new_context->G_package_ids_size;

            char **tmp = realloc(
                new_context->G_package_ids,
                (new_id + 1) *
                    sizeof(*new_context->G_package_ids)
            );

            if (tmp == NULL)
                return ENOMEM;

            new_context->G_package_ids = tmp;

            new_context->G_package_ids[new_id] =
                strdup(package_name_field->value);

            if (new_context->G_package_ids[new_id] == NULL)
                return ENOMEM;

            new_context->G_package_ids_size++;

            package_id = new_id;
        }

        new_context->p = package_id;
        new_context->p_page = package_id / 64;
        new_context->p_mask = UINT64_C(1) << (63 - (new_context->p % 64));

        if (new_context->p_page >= new_context->G_pages_size) {
            size_t old_size = new_context->G_pages_size;
            size_t new_size = new_context->p_page + 1;

            uint64_t *tmp = realloc(
                new_context->G_pages,
                new_size * sizeof(*new_context->G_pages)
            );

            if (tmp == NULL)
                return ENOMEM;

            new_context->G_pages = tmp;

            memset(
                new_context->G_pages + old_size,
                0,
                (new_size - old_size) *
                    sizeof(*new_context->G_pages)
            );

            new_context->G_pages_size = new_size;
        }

        new_context->G_pages[new_context->p_page] |= new_context->p_mask;

        /*
         * D — dependencies of current package.
         *
         */

        const struct PackageField *first_dependency_field =
            PackageInfo_Find(
                new_context->package_info,
                "dependency"
            );

        const struct PackageField *dep_field =
            first_dependency_field;

        while (dep_field != NULL) {
            if (dep_field->section != 6)
                break;

            if (dep_field->value != NULL) {
                size_t dep_id = SIZE_MAX;

                // Find dependency in G
                for (
                    size_t i = 0;
                    i < new_context->G_package_ids_size;
                    i++
                ) {
                    if (new_context->G_package_ids[i] == NULL)
                        continue;
                    

                    if (
                        strcmp(
                            new_context->G_package_ids[i],
                            dep_field->value
                        ) == 0
                    ) {
                        dep_id = i;
                        break;
                    }
                }

                // Dependency isn't in G yet
                if (dep_id == SIZE_MAX) {
                    char **tmp = realloc(
                        new_context->G_package_ids,
                        (new_context->G_package_ids_size + 1) *
                            sizeof(*new_context->G_package_ids)
                    );

                    if (tmp == NULL)
                        return ENOMEM;

                    new_context->G_package_ids = tmp;

                    new_context->G_package_ids[new_context->G_package_ids_size] =
                        strdup(dep_field->value);

                    if (
                        new_context->G_package_ids[new_context->G_package_ids_size] == NULL
                    )
                        return ENOMEM;

                    dep_id = new_context->G_package_ids_size;

                    new_context->G_package_ids_size++;
                }

                // Add dependency to D
                size_t page = dep_id / 64;

                uint64_t mask =
                    UINT64_C(1) << (63 - (dep_id % 64));

                if (page >= new_context->D_pages_size) {
                    size_t old_size = new_context->D_pages_size;
                    size_t new_size = page + 1;

                    uint64_t *tmp = realloc(
                        new_context->D_pages,
                        new_size * sizeof(*new_context->D_pages)
                    );

                    if (tmp == NULL)
                        return ENOMEM;

                    new_context->D_pages = tmp;

                    memset(
                        new_context->D_pages + old_size,
                        0,
                        (new_size - old_size) *
                            sizeof(*new_context->D_pages)
                    );

                    new_context->D_pages_size = new_size;
                }

                new_context->D_pages[page] |= mask;
            }

            dep_field = dep_field->next_field;
        }


        /*
         * RD — installed packages that depend on current package.
         */
        for (
            size_t package_id = 0;
            package_id < new_context->G_package_ids_size;
            package_id++
        ) {
            char *package_name =
                new_context->G_package_ids[package_id];

            if (package_name == NULL)
                continue;

            struct PackageMatches versions =
                catpkg_find_version(
                    package_name
                );

            if (
                versions.items == NULL ||
                versions.count == 0 ||
                versions.items[0] == NULL
            ) {
                continue;
            }

            char package_fullname[256];

            snprintf(
                package_fullname,
                sizeof(package_fullname),
                "%s@%s",
                package_name,
                versions.items[0]
            );

            struct PackageInfo *dep_info =
                catpkg_find_package_info(
                    package_fullname
                );

            if (dep_info == NULL)
                continue;

            if (
                CATPKG_ReadSection(
                    dep_info,
                    6
                ) == (ssize_t) SIZE_MAX
            ) {
                PackageInfo_Free(dep_info);
                continue;
            }

            const struct PackageField *dep_field =
                dep_info->fields;

            while (dep_field != NULL) {
                if (
                    dep_field->value != NULL &&
                    strcmp(
                        dep_field->value,
                        package_name_field->value
                    ) == 0
                ) {
                    size_t page =
                        package_id / 64;

                    uint64_t mask =
                        UINT64_C(1) << (63 - (package_id % 64));

                    if (page >= new_context->RD_pages_size) {
                        size_t old_size =
                            new_context->RD_pages_size;

                        size_t new_size =
                            page + 1;

                        uint64_t *tmp =
                            realloc(
                                new_context->RD_pages,
                                new_size * sizeof(*new_context->RD_pages)
                            );

                        if (tmp == NULL) {
                            PackageInfo_Free(dep_info);
                            return ENOMEM;
                        }

                        new_context->RD_pages = tmp;

                        memset(
                            new_context->RD_pages + old_size,
                            0,
                            (new_size - old_size) * sizeof(*new_context->RD_pages)
                        );

                        new_context->RD_pages_size =
                            new_size;
                    }

                    new_context->RD_pages[page] |= mask;

                    break;
                }

                dep_field =
                    dep_field->next_field;
            }

            PackageInfo_Free(dep_info);
        }

        const struct PackageField *info_field =
            PackageInfo_Find(new_context->file_info, "info");

        while (info_field != NULL) {
            if (strcmp("#type", info_field->name) == 0) {
                struct FILEINFO_Entry entry = {
                    .names = malloc(sizeof(char *)),
                    .values = malloc(sizeof(char *)),
                    .records_size = 1
                };

                if (entry.names == NULL || entry.values == NULL)
                    return 1;

                entry.names[0] = strdup("type");
                entry.values[0] = strdup(info_field->value);

                info_field = info_field->next_field;

                while (info_field != NULL) {
                    if (
                        info_field->next_field != NULL &&
                        strcmp("info", info_field->next_field->name) == 0
                    )
                        break;

                    char **tmp = realloc(
                        entry.names,
                        ++entry.records_size * sizeof(char *)
                    );

                    if (tmp == NULL)
                        return 1;

                    entry.names = tmp;

                    tmp = realloc(
                        entry.values,
                        entry.records_size * sizeof(char *)
                    );

                    if (tmp == NULL)
                        return 1;

                    entry.values = tmp;

                    entry.names[entry.records_size - 1] =
                        strdup(info_field->name);

                    entry.values[entry.records_size - 1] =
                        strdup(info_field->value);

                    info_field = info_field->next_field;
                }

                size_t entry_id = 0;
                bool is_exist = false;

                for (size_t i = 0; i < new_context->G_entries_ids_size; i++) {
                    bool is_equal =
                        entry.records_size ==
                        new_context->G_entries_ids[i].records_size;

                    for (
                        size_t ai = 0;
                        is_equal && ai < entry.records_size;
                        ai++
                    ) {
                        is_equal =
                            strcmp(
                                entry.names[ai],
                                new_context->G_entries_ids[i].names[ai]
                            ) == 0 &&
                            strcmp(
                                entry.values[ai],
                                new_context->G_entries_ids[i].values[ai]
                            ) == 0;
                    }

                    if (is_equal) {
                        entry_id = i;
                        is_exist = true;
                        break;
                    }
                }

                if (is_exist) {
                    for (size_t i = 0; i < entry.records_size; i++) {
                        free(entry.names[i]);
                        free(entry.values[i]);
                    }

                    free(entry.names);
                    free(entry.values);
                }
                else {
                    struct FILEINFO_Entry *new_items = realloc(
                        new_context->G_entries_ids,
                        (new_context->G_entries_ids_size + 1) *
                        sizeof(*new_context->G_entries_ids)
                    );

                    if (new_items == NULL)
                        return 1;

                    new_context->G_entries_ids = new_items;

                    entry_id = new_context->G_entries_ids_size;

                    new_context->G_entries_ids[entry_id] = entry;

                    new_context->G_entries_ids_size++;
                }

                size_t page = entry_id / 64;

                uint64_t mask =
                    UINT64_C(1) << (63 - (entry_id % 64));

                if (page >= new_context->N_pages_size) {
                    size_t old_size =
                        new_context->N_pages_size;

                    size_t new_size =
                        page + 1;

                    uint64_t *tmp = realloc(
                        new_context->N_pages,
                        new_size * sizeof(*new_context->N_pages)
                    );

                    if (tmp == NULL)
                        return 1;

                    new_context->N_pages = tmp;

                    memset(
                        new_context->N_pages + old_size,
                        0,
                        (new_size - old_size) *
                        sizeof(*new_context->N_pages)
                    );

                    new_context->N_pages_size = new_size;
                }

                new_context->N_pages[page] |= mask;
            }

            if (info_field != NULL)
                info_field = info_field->next_field;
        }

    request->context =
        new_context;

    catpkg_build_log(
        "OPSEC_CREATE: %s\n",
        builder,
        true,
        request_val
    );

    break;
}

case OPSEC_CHECK:
{
    #define OPSEC_ACTION(exception) exception

    #define X(name) #name,

    const char *protocols_names[] = {
        CATPKG_PROTOCOLS(X)
    };

    #undef X

    #define POLICY_PROTECTED \
        do { \
            catpkg_build_log( \
                "OPSeC: operation \"%s\" skipped - object \"%s\" is protected by policy\n", \
                builder, \
                true, \
                protocols_names[checked_request->protocol], \
                request_val \
            ); \
            checked_request->skip = 1; \
        } while (0)

    #define STILL_IN_USE \
        do { \
            catpkg_build_log( \
                "OPSeC: operation \"%s\" skipped - object \"%s\" is still in use by package \"%s\"\n", \
                builder, \
                true, \
                protocols_names[checked_request->protocol], \
                request_val, \
                name_field -> value \
            ); \
            checked_request->skip = 1; \
        } while (0)

    #define SECTION_STILL_REQUIRED \
        do { \
            catpkg_build_log( \
                "OPSeC: operation \"%s\" skipped - object \"%s\" is still required by an installed package\n", \
                builder, \
                true, \
                protocols_names[checked_request->protocol], \
                request_val \
            ); \
            checked_request->skip = 1; \
        } while (0)

    #define PACKAGE_STILL_REQUIRED \
        do { \
            uint64_t value = *pageRD; \
            size_t packages_count = 0; \
\
            for (size_t bit = 0; bit < 64; bit++) { \
                uint64_t mask = \
                    UINT64_C(1) << (63 - bit); \
\
                if ((value & mask) == 0) \
                    continue; \
\
                packages_count++; \
            } \
\
            fprintf( \
                stderr, \
                "OPSeC operation error: package \"%s\" is still required by an installed packages. \nPackages (%u): \"", \
                name_field -> value, \
                packages_count \
            ); \
\
            for (size_t bit = 0; bit < 64; bit++) { \
                uint64_t mask = \
                    UINT64_C(1) << (63 - bit); \
\
                if ((value & mask) == 0) \
                    continue; \
\
                size_t package_id = \
                    page * 64 + bit; \
\
                fprintf( \
                    stderr, \
                    database_package_ids[package_id] \
                ); \
                if(--packages_count > 0) \
                    fprintf(stderr, ", "); \
            } \
            fprintf(stderr, "\"\n"); \
            return 1; \
        } while (0)

         #define MISSING_DEPENDENCY \
            do { \
                size_t missing_count = 0; \
                \
                for (size_t page = 0; page < opsec_context->D_pages_size; page++) { \
                    uint64_t pageD = opsec_context->D_pages[page]; \
                    uint64_t pageG = \
                        page < opsec_context->G_pages_size \
                            ? opsec_context->G_pages[page] \
                            : 0; \
                    \
                    uint64_t missing = pageD & ~pageG; \
                    \
                    for (size_t bit = 0; bit < 64; bit++) { \
                        uint64_t mask = \
                            UINT64_C(1) << (63 - bit); \
                        \
                        if ((missing & mask) == 0) \
                            continue; \
                        \
                        size_t package_id = \
                            page * 64 + bit; \
                        \
                        if (package_id >= opsec_context->G_package_ids_size) \
                            continue; \
                        \
                        missing_count++; \
                    } \
                } \
                \
                fprintf( \
                    stderr, \
                    "OPSeC operation error: package \"%s\" has missing dependencies.\n" \
                    "Missing dependencies (%zu): \"", \
                    name_field->value, \
                    missing_count \
                ); \
                \
                size_t remaining = missing_count; \
                \
                for (size_t page = 0; page < opsec_context->D_pages_size; page++) { \
                    uint64_t pageD = opsec_context->D_pages[page]; \
                    uint64_t pageG = \
                        page < opsec_context->G_pages_size \
                            ? opsec_context->G_pages[page] \
                            : 0; \
                    \
                    uint64_t missing = pageD & ~pageG; \
                    \
                    for (size_t bit = 0; bit < 64; bit++) { \
                        uint64_t mask = \
                            UINT64_C(1) << (63 - bit); \
                        \
                        if ((missing & mask) == 0) \
                            continue; \
                        \
                        size_t package_id = \
                            page * 64 + bit; \
                        \
                        if (package_id >= opsec_context->G_package_ids_size) \
                            continue; \
                        \
                        fprintf( \
                            stderr, \
                            "%s", \
                            opsec_context->G_package_ids[package_id] \
                        ); \
                        \
                        if (--remaining > 0) \
                            fprintf(stderr, ", "); \
                    } \
                } \
                \
                fprintf(stderr, "\"\n"); \
                return 1; \
            } while (0)

    #define UNCORRECT_DATABASE_RECORD \
        do { \
            fprintf( \
                stderr, \
                "OPSeC: database integrity error: incorrect database record\n" \
            ); \
            return 1; \
        } while (0)

    #define WAS_NOT_FOUND_IN_DATABASE \
        do { \
            fprintf( \
                stderr, \
                "OPSeC: database integrity error: object was not found in DATABASE\n" \
            ); \
            return 1; \
        } while (0)

    #include "protocols/OPSeC_CHECK_DEFAULT.inc"

    #undef OPSEC_ACTION
    #undef POLICY_PROTECTED
    #undef STILL_IN_USE
    #undef SECTION_STILL_REQUIRED
    #undef PACKAGE_STILL_REQUIRED
    #undef WAS_NOT_FOUND_IN_DATABASE
    #undef UNCORRECT_DATABASE_RECORD
    break;
}

case CTPG_REGISTER_METADATA_PACKAGEINFO:
{
    if (request_val == NULL)
        return EINVAL;

    const char *source =
        CATPKG_PACKAGEINFO;

    const char *destination =
        request_val;

    catpkg_builder_request(
        builder,
        MKDIR_SOFT,
        request_val
    );

    size_t source_size =
        strlen(source) + 1;

    size_t destination_size =
        strlen(destination) + 1;

    char *extract_request_val =
        malloc(
            source_size +
            destination_size
        );

    if (extract_request_val == NULL)
        return ENOMEM;

    /*
     * CTPG_EXTRACT format:
     *
     * source\0destination\0
     */

    memcpy(
        extract_request_val,
        source,
        source_size
    );

    memcpy(
        extract_request_val + source_size,
        destination,
        destination_size
    );

    if (
        catpkg_builder_request_after_free(
            builder,
            CTPG_EXTRACT,
            extract_request_val
        ) != 0
    ) {
        free(extract_request_val);
        return 1;
    }

    break;
}

case CTPG_UPDATE_PACKAGEINFO:
{
    if (request_val == NULL)
        return EINVAL;

    char *remove_val =
        request_val;

    char *install_val =
        remove_val +
        strlen(remove_val) + 1;

    struct PackageInfo *package_info =
        CATPKG_Parse(
            install_val,
            NULL
        );

    if (package_info == NULL)
        return 1;

    const struct PackageField *version_field =
        PackageInfo_Find(
            package_info,
            "version"
        );

    if (
        version_field == NULL ||
        version_field->value == NULL
    ) {
        PackageInfo_Free(
            package_info
        );

        return 1;
    }

    char *current_version =
        strdup(
            version_field->value
        );

    PackageInfo_Free(
        package_info
    );

    if (current_version == NULL)
        return ENOMEM;

    char *metadata_packageinfo =
        strdup(
            install_val
        );

    dirname(metadata_packageinfo);

    if (metadata_packageinfo == NULL) {
        free(
            current_version
        );

        return ENOMEM;
    }

    if (
        catpkg_builder_request_after_free(
            builder,
            CTPG_REGISTER_METADATA_PACKAGEINFO,
            metadata_packageinfo
        ) != 0
    ) {
        free(
            metadata_packageinfo
        );

        free(
            current_version
        );

        return 1;
    }

    struct PackageMatches versions =
        catpkg_find_version(
            remove_val
        );

    if (
        versions.items == NULL ||
        versions.count == 0
    ) {
        free(
            current_version
        );

        break;
    }

    size_t package_name_size =
        strlen(
            remove_val
        );
    for (
        size_t i = 0;
        i < versions.count;
        i++
    ) {
        if (versions.items[i] == NULL)
            continue;

        if (
            strcmp(
                versions.items[i],
                current_version
            ) == 0
        ) {
            continue;
        }

        size_t version_size =
            strlen(
                versions.items[i]
            );

        char *package_fullname =
            malloc(
                package_name_size +
                1 +
                version_size +
                1
            );

        if (package_fullname == NULL) {
            free(
                current_version
            );

            return ENOMEM;
        }

        memcpy(
            package_fullname,
            remove_val,
            package_name_size
        );

        package_fullname[
            package_name_size
        ] = '@';

        memcpy(
            package_fullname +
                package_name_size + 1,
            versions.items[i],
            version_size + 1
        );

        char *remove_package =
            strdup(
                package_fullname
            );

        free(
            package_fullname
        );

        if (remove_package == NULL) {
            free(
                current_version
            );

            return ENOMEM;
        }

        if (
            catpkg_builder_request_after_free(
                builder,
                CATPKG_REMOVE_PACKAGE_RECORDS,
                remove_package
            ) != 0
        ) {
            free(
                remove_package
            );

            free(
                current_version
            );

            return 1;
        }
    }

    free(
        current_version
    );

    break;
}

case EXCEPTION:
{
    char *exception_protocol = request_val + strlen(request_val) + 1;
    int exception_protocol_n = (int)strtol(exception_protocol, NULL, 10);
    char *exception_protocol_value =
        exception_protocol + strlen(exception_protocol) + 1;

    request->skip = 1;

    catpkg_builder_request(
        builder,
        exception_protocol_n,
        exception_protocol_value
    );
    break;
}

case CTPG_OPEN:
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


case CTPG_CLOSE:

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

case CTPG_MKCMD:
{
    int fd4[2];
    int fd5[2];
    int fd6[2];

    if (
        pipe(fd4) == -1 ||
        pipe(fd5) == -1 ||
        pipe(fd6) == -1
    )
        return 1;

    char *script =
        catpkg_builder_ctpg_read_data(
            ctpg_context,
            request_val
        );

    if (script == NULL)
        return 1;

    size_t script_size =
        strlen(script);

    pid_t pid =
        fork();

    if (pid == -1) {
        free(script);
        return 1;
    }

    if (pid == 0) {
        dup2(fd4[1], 4);
        dup2(fd5[1], 5);
        dup2(fd6[1], 6);

        close(fd4[0]);
        close(fd5[0]);
        close(fd6[0]);

        int command_memfd =
            memfd_create(
                "catpkg-command",
                0
            );

        if (command_memfd == -1)
            _exit(1);

        if (
            write(
                command_memfd,
                script,
                script_size
            ) != (ssize_t)script_size
        )
            _exit(1);

        if (
            lseek(
                command_memfd,
                0,
                SEEK_SET
            ) == -1
        )
            _exit(1);

        char path[64];

        snprintf(
            path,
            sizeof(path),
            "/proc/self/fd/%d",
            command_memfd
        );

        execl(
            "/usr/bin/catpkg-command",
            "catpkg-command",
            path,

            "--commands_data_dir",
            "",

            "--name",
            "4",

            "--description",
            "5",

            "--declaration",
            "6",

            (char *)NULL
        );

        _exit(127);
    }

    free(script);

    close(fd4[1]);
    close(fd5[1]);
    close(fd6[1]);

    char command_name[256] = {0};
    char command_description[4096] = {0};
    char command_declaration[4096] = {0};

    ssize_t name_size =
        read(
            fd4[0],
            command_name,
            sizeof(command_name) - 1
        );

    ssize_t description_size =
        read(
            fd5[0],
            command_description,
            sizeof(command_description) - 1
        );

    ssize_t declaration_size =
        read(
            fd6[0],
            command_declaration,
            sizeof(command_declaration) - 1
        );

    close(fd4[0]);
    close(fd5[0]);
    close(fd6[0]);

    int status;

    if (
        waitpid(
            pid,
            &status,
            0
        ) == -1
    )
        return 1;

    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0
    )
        return 1;

    if (
        name_size <= 0 ||
        declaration_size <= 0 ||
        description_size < 0
    )
        return 1;

    command_name[name_size] = '\0';
    command_description[description_size] = '\0';
    command_declaration[declaration_size] = '\0';

    if (
        command_name[0] == '\0' ||
        command_declaration[0] == '\0'
    )
        return 1;

    char *command_symlink_path =
        make_catpkg_path(
            "/var/lib/catpkg/commands/%s",
            command_name
        );

    if (command_symlink_path == NULL)
        return 1;

    request->context =
        strdup(
            command_symlink_path
        );

    free(command_symlink_path);

    if (request->context == NULL)
        return 1;

    break;
}

case CTPG_MKCMD_BUILD:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->name_field == NULL
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_MKCMD_BUILD: invalid context"
        );

        return 1;
    }
    char *package_data = make_catpkg_path(
        CATPKG_PACKAGE_DATA "/.catpkg/commands/DATA",
        ctpg_context->name_field->value
    );

    catpkg_builder_request_after_free(
        builder,
        MKDIR_SOFT,
        package_data
    );

    catpkg_builder_request(
        builder,
        CTPG_MKCMD,
        request_val
    );
    break;
}

case CTPG_EXTRACT_PAYLOAD:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_PAYLOAD: invalid context"
        );

        return 1;
    }

    const struct CatpkgRequest *last_payload_exclude = NULL;

    for (size_t ai = builder->requests_size; ai-- > 0;) {
        if (
            builder->requests[ai]->protocol ==
            PAYLOAD_EXCLUDE
        ) {
            last_payload_exclude =
                builder->requests[ai];

            break;
        }
    }

    struct PAYLOAD_EXCLUDE_Context *payload_exclude_context = NULL;
    
    if (last_payload_exclude != NULL)
        payload_exclude_context = last_payload_exclude->context;

    struct header_posix_ustar hpu;

    FILE *tarfile = fopen(ctpg_context->path, "r");
    uint64_t current_tar_header_mask = UINT64_C(1) << 63;

    size_t current_tar_header_mask_page = 0;

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {
        char size_string[13];

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
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR data"
            );

            fclose(tarfile);

            return 1;
        }

        if (
            fseek(
                tarfile,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            fclose(tarfile);

            return 1;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {

            fclose(tarfile);

            return 1;
        }

        if (
            strcmp(correct_path, "PACKAGEINFO") != 0 &&
            hpu.typeflag[0] != '5' &&
            (
                payload_exclude_context == NULL ||
                current_tar_header_mask_page >=
                    payload_exclude_context->E_pages_size ||
                !(
                    current_tar_header_mask &
                    payload_exclude_context
                        ->E_pages[current_tar_header_mask_page]
                )
            )
        ) {
            size_t name_len =
                strlen(hpu.name);

            char *hpu_name_dir =
                malloc(name_len + 2);

            if (hpu_name_dir == NULL)
                return 1;

            hpu_name_dir[0] = '/';

            memcpy(
                hpu_name_dir + 1,
                hpu.name,
                name_len + 1
            );

            dirname(hpu_name_dir);

            size_t destination_len = 1;

            char *request_val =
                malloc(
                    name_len + 1 +
                    destination_len + 1
                );

            if (request_val == NULL) {
                free(hpu_name_dir);
                return 1;
            }

            char *ptr =
                request_val;

            memcpy(
                ptr,
                hpu.name,
                name_len + 1
            );

            ptr +=
                name_len + 1;

            memcpy(
                ptr,
                "/",
                destination_len + 1
            );

            catpkg_builder_request_after_free(
                builder,
                MKDIR_SOFT,
                hpu_name_dir
            );

            catpkg_builder_request_after_free(
                builder,
                CTPG_EXTRACT,
                request_val
            );
        }

        free(correct_path);   

        current_tar_header_mask >>= 1;

        if (current_tar_header_mask == 0) {
            current_tar_header_mask = UINT64_C(1) << 63;
            current_tar_header_mask_page++;
        }
    }
    catpkg_builder_request(
        builder,
        CTPG_EXTRACT_PAYLOAD_END,
        NULL
    );
}