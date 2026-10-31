const struct CatpkgRequest *current_opsec_create = NULL;

for (size_t ai = builder->requests_size; ai-- > 0;) {
    if (
        builder->requests[ai]->protocol ==
        OPSEC_CREATE
    ) {
        current_opsec_create =
            builder->requests[ai];

        break;
    }
}

if (current_opsec_create == NULL)
    return 1;


struct OPSeC_Context *opsec_context = current_opsec_create->context;
const struct PackageField *name_field =
    PackageInfo_Find(
        opsec_context->package_info,
        "name"
    );

if (
    name_field == NULL ||
    name_field->value == NULL
) {
    return 1;
}


const struct PackageField *first_required_field =
    PackageInfo_Find(
        opsec_context->file_info,
        "#required"
    );

const struct PackageField *first_protected_field =
    PackageInfo_Find(
        opsec_context->file_info,
        "#protected"
    );

const struct PackageField *first_dependency_field =
    PackageInfo_Find(
        opsec_context->package_info,
        "dependency"
    );

unsigned long flags =
    strtoul(
        request_val,
        NULL,
        10
    );

catpkg_build_log(
    "OPSeC",
    builder,
    true,
    0
);

if (flags & OPSEC_FLAG_REMOVE) {
    catpkg_build_log(
        " / REMOVE",
        builder,
        true,
        0
    );
}

if (flags & OPSEC_FLAG_UPDATE) {
    catpkg_build_log(
        " / UPDATE",
        builder,
        true,
        0
    );
}

if (flags & OPSEC_FLAG_INSTALL) {
    catpkg_build_log(
        " / INSTALL",
        builder,
        true,
        0
    );
}

catpkg_build_log(
    "\n",
    builder,
    true,
    0
);

enum {
    OPSEC_VALIDATION,
    OPSEC_RESOLVE
};

const struct PackageField *info_field =
    PackageInfo_Find(opsec_context -> file_info, "info");
while (info_field != NULL) {
    if (strcmp("#type", info_field->name) == 0) {
        size_t entry_section = info_field->section;

        struct FILEINFO_Entry entry = {
            .names = malloc(sizeof(char *)),
            .values = malloc(sizeof(char *)),
            .records_size = 1
        };

        if (entry.names == NULL || entry.values == NULL)
            return 1;

        entry.names[0] = strdup("type");
        entry.values[0] = strdup(info_field->value);

        if (entry.names[0] == NULL || entry.values[0] == NULL)
            return 1;

        info_field = info_field->next_field;

        while (info_field != NULL) {
            size_t new_records_size =
                entry.records_size + 1;

            char **tmp_names = realloc(
                entry.names,
                new_records_size * sizeof(*entry.names)
            );

            if (tmp_names == NULL)
                return 1;

            entry.names = tmp_names;

            char **tmp_values = realloc(
                entry.values,
                new_records_size * sizeof(*entry.values)
            );

            if (tmp_values == NULL)
                return 1;

            entry.values = tmp_values;

            entry.names[entry.records_size] =
                strdup(info_field->name + 1); // Skip prefix #

            entry.values[entry.records_size] =
                strdup(info_field->value);

            if (
                entry.names[entry.records_size] == NULL ||
                entry.values[entry.records_size] == NULL
            )
                return 1;

            entry.records_size = new_records_size;

            if (
                info_field->next_field != NULL &&
                strcmp(
                    "info",
                    info_field->next_field->name
                ) == 0
            )
                break;

            info_field = info_field->next_field;
        }

        size_t entry_id = SIZE_MAX;

        for (
            size_t i = 0;
            i < opsec_context->G_entries_ids_size;
            i++
        ) {
            bool is_equal =
                entry.records_size ==
                opsec_context->G_entries_ids[i].records_size;

            for (
                size_t j = 0;
                is_equal && j < entry.records_size;
                j++
            ) {
                is_equal =
                    strcmp(
                        entry.names[j],
                        opsec_context->G_entries_ids[i].names[j]
                    ) == 0 &&
                    strcmp(
                        entry.values[j],
                        opsec_context->G_entries_ids[i].values[j]
                    ) == 0;
            }

            if (is_equal) {
                entry_id = i;
                break;
            }
        }

        if (entry_id != SIZE_MAX) {
            for (size_t i = 0; i < entry.records_size; i++) {
                free(entry.names[i]);
                free(entry.values[i]);
            }

            free(entry.names);
            free(entry.values);
        }
        else {
            struct FILEINFO_Entry *new_items = realloc(
                opsec_context->G_entries_ids,
                (opsec_context->G_entries_ids_size + 1) *
                sizeof(*opsec_context->G_entries_ids)
            );

            if (new_items == NULL) {
                for (size_t i = 0; i < entry.records_size; i++) {
                    free(entry.names[i]);
                    free(entry.values[i]);
                }

                free(entry.names);
                free(entry.values);

                return 1;
            }

            opsec_context->G_entries_ids =
                new_items;

            entry_id =
                opsec_context->G_entries_ids_size;

            opsec_context->G_entries_ids[entry_id] =
                entry;

            opsec_context->G_entries_ids_size++;
        }

        size_t page =
            entry_id / 64;

        uint64_t mask =
            UINT64_C(1) << (63 - (entry_id % 64));

        if (page >= opsec_context->N_pages_size) {
            size_t old_size =
                opsec_context->N_pages_size;

            size_t new_size =
                page + 1;

            uint64_t *tmp = realloc(
                opsec_context->N_pages,
                new_size * sizeof(*opsec_context->N_pages)
            );

            if (tmp == NULL)
                return 1;

            opsec_context->N_pages =
                tmp;

            memset(
                opsec_context->N_pages + old_size,
                0,
                (new_size - old_size) *
                sizeof(*opsec_context->N_pages)
            );

            opsec_context->N_pages_size =
                new_size;
        }

        opsec_context->N_pages[page] |=
            mask;

        for (
            const struct PackageField *field_check =
                first_protected_field;

            field_check != NULL &&
            field_check->section == 0;

            field_check = field_check->next_field
        ) {
            if (
                strcmp(
                    "#protected",
                    field_check->name
                ) != 0
            )
                continue;

            size_t protected_section =
                (size_t)strtoull(
                    field_check->value,
                    NULL,
                    10
                );

            if (
                entry_section == 0 ||
                entry_section - 1 != protected_section
            )
                continue;

            if (page >= opsec_context->P_pages_size) {
                size_t old_size =
                    opsec_context->P_pages_size;

                size_t new_size =
                    page + 1;

                uint64_t *tmp = realloc(
                    opsec_context->P_pages,
                    new_size * sizeof(*opsec_context->P_pages)
                );

                if (tmp == NULL)
                    return 1;

                opsec_context->P_pages =
                    tmp;

                memset(
                    opsec_context->P_pages + old_size,
                    0,
                    (new_size - old_size) *
                    sizeof(*opsec_context->P_pages)
                );

                opsec_context->P_pages_size =
                    new_size;
            }
            opsec_context->P_pages[page] |=
                mask;

            break;
        }
    }

    if (info_field != NULL)
        info_field = info_field->next_field;
}
for (int phase = OPSEC_VALIDATION; phase <= OPSEC_RESOLVE; phase++) {
    size_t index;

    for (index = 0; index < builder->requests_size; index++)
        if (builder->requests[index] == current_opsec_create)
            break;

    if(phase == OPSEC_RESOLVE) {
        for (
            size_t ai = index;
            ai < builder->requests_size;
            ai++
        ) {

            struct CatpkgRequest *checked_request =
                builder->requests[ai];

            if (checked_request == NULL)
                continue;
  
            if(checked_request == request)
                break;

            char *checked_request_val =
                checked_request->value;

            if (checked_request->skip)
                continue;

            size_t sap_section_index =
                1;

            struct FILEINFO_Entry entry = {
                .names = NULL,
                .values = NULL,

                .records_size = 0
            };

            struct FILEINFO_Entry remove_entry = {
                .names = NULL,
                .values = NULL,

                .records_size = 0
            };

            bool is_protected = 0;

            #define X(record, name, value) X_##record(name, value)
            
            #define ADD_ENTRY                                                              \
                if (entry.names != NULL && entry.values != NULL) {                          \
                    size_t entry_id = 0;                                                    \
                    bool is_exist = false;                                                  \
                                                                                            \
                    for (                                                                   \
                        size_t _entry_id = 0;                                               \
                        _entry_id < opsec_context->G_entries_ids_size;                      \
                        _entry_id++                                                         \
                    ) {                                                                     \
                        bool is_equal =                                                     \
                            entry.records_size ==                                           \
                            opsec_context->G_entries_ids[_entry_id].records_size;           \
                                                                                            \
                        for (                                                               \
                            size_t ai = 0;                                                  \
                            is_equal && ai < entry.records_size;                            \
                            ai++                                                            \
                        )                                                                   \
                            is_equal =                                                      \
                                strcmp(                                                     \
                                    entry.names[ai],                                         \
                                    opsec_context->G_entries_ids[_entry_id].names[ai]        \
                                ) == 0 &&                                                   \
                                strcmp(                                                     \
                                    entry.values[ai],                                        \
                                    opsec_context->G_entries_ids[_entry_id].values[ai]       \
                                ) == 0;                                                     \
                                                                                            \
                        if (is_equal) {                                                     \
                            entry_id = _entry_id;                                           \
                            is_exist = true;                                                \
                            break;                                                          \
                        }                                                                   \
                    }                                                                       \
                                                                                            \
                    if (!is_exist) {                                                        \
                        struct FILEINFO_Entry *new_items = realloc(                         \
                            opsec_context->G_entries_ids,                                   \
                            (opsec_context->G_entries_ids_size + 1) *                       \
                            sizeof(*opsec_context->G_entries_ids)                           \
                        );                                                                  \
                                                                                            \
                        if (new_items == NULL)                                              \
                            return 1;                                                       \
                                                                                            \
                        opsec_context->G_entries_ids = new_items;                           \
                        entry_id = opsec_context->G_entries_ids_size;                       \
                        opsec_context->G_entries_ids[entry_id] = entry;                     \
                        opsec_context->G_entries_ids_size++;                                \
                    }                                                                       \
                                                                                            \
                    size_t page = entry_id / 64;                                            \
                    uint64_t mask =                                                         \
                        UINT64_C(1) << (63 - (entry_id % 64));                              \
                                                                                            \
                    if (page >= opsec_context->C_pages_size) {                              \
                        size_t old_size = opsec_context->C_pages_size;                      \
                        size_t new_size = page + 1;                                         \
                                                                                            \
                        uint64_t *tmp = realloc(                                            \
                            opsec_context->C_pages,                                         \
                            new_size * sizeof(*opsec_context->C_pages)                      \
                        );                                                                  \
                                                                                            \
                        if (tmp == NULL)                                                    \
                            return 1;                                                       \
                                                                                            \
                        opsec_context->C_pages = tmp;                                       \
                                                                                            \
                        memset(                                                             \
                            opsec_context->C_pages + old_size,                              \
                            0,                                                              \
                            (new_size - old_size) *                                         \
                            sizeof(*opsec_context->C_pages)                                 \
                        );                                                                  \
                                                                                            \
                        opsec_context->C_pages_size = new_size;                             \
                    }                                                                       \
                                                                                            \
                    if (is_protected && page >= opsec_context->P_pages_size) {              \
                        size_t old_size = opsec_context->P_pages_size;                      \
                        size_t new_size = page + 1;                                         \
                                                                                            \
                        uint64_t *tmp = realloc(                                            \
                            opsec_context->P_pages,                                         \
                            new_size * sizeof(*opsec_context->P_pages)                      \
                        );                                                                  \
                                                                                            \
                        if (tmp == NULL)                                                    \
                            return 1;                                                       \
                                                                                            \
                        opsec_context->P_pages = tmp;                                       \
                                                                                            \
                        memset(                                                             \
                            opsec_context->P_pages + old_size,                              \
                            0,                                                              \
                            (new_size - old_size) *                                         \
                            sizeof(*opsec_context->P_pages)                                 \
                        );                                                                  \
                                                                                            \
                        opsec_context->P_pages_size = new_size;                             \
                    }                                                                       \
                                                                                            \
                    if (is_protected) {                                                     \
                        opsec_context->P_pages[page] |= mask;                               \
                        is_protected = false;                                               \
                    }                                                                       \
                                                                                            \
                    opsec_context->C_pages[page] |= mask;                                   \
                                                                                            \
                    if (is_exist) {                                                         \
                        for (size_t i = 0; i < entry.records_size; i++) {                   \
                            free(entry.names[i]);                                           \
                            free(entry.values[i]);                                          \
                        }                                                                   \
                                                                                            \
                        free(entry.names);                                                  \
                        free(entry.values);                                                 \
                    }                                                                       \
                                                                                            \
                    entry.names = NULL;                                                     \
                    entry.values = NULL;                                                    \
                    entry.records_size = 0;                                                 \
                }

            #define REMOVE_ENTRY                                                                \
                if (remove_entry.names != NULL && remove_entry.values != NULL) {                   \
                    for (size_t _entry_id = 0; _entry_id < opsec_context->G_entries_ids_size; _entry_id++) {    \
                        bool is_equal = \
                            entry.records_size == opsec_context->G_entries_ids[_entry_id].records_size; \
\
                        for (size_t ai = 0; is_equal && ai < entry.records_size; ai++) \
                            is_equal = \
                                strcmp(entry.names[ai], opsec_context->G_entries_ids[_entry_id].names[ai]) == 0 && \
                                strcmp(entry.values[ai], opsec_context->G_entries_ids[_entry_id].values[ai]) == 0; \
\
                        if (is_equal) { \
                            size_t page = _entry_id / 64;                                       \
                                                                                                 \
                            uint64_t mask =                                                     \
                                UINT64_C(1) << (63 - (_entry_id % 64));                         \
                                                                                                 \
                                                                                                 \
                            if (page < opsec_context->C_pages_size)                                            \
                                opsec_context->C_pages[page] &= ~mask;                                         \
                                                                                                 \
                        } \
                    }                                                                           \
                                                                                                 \
                    for(size_t i = 0; i < entry.records_size; i++) { \
                        free(entry.names[i]); \
                        free(entry.values[i]); \
                    } \
                    remove_entry.names = NULL;                                                   \
                    remove_entry.values = NULL;                                                   \
                }

            #define FIELD_SEP(record) ADD_ENTRY;

            #define X_INFO(field_name, field_value)                                  \
                {                                                                   \
                char **tmp = realloc(                                                \
                    entry.names,                                                     \
                    ++entry.records_size * sizeof(char *)                            \
                );                                                                   \
                                                                                     \
                if (tmp == NULL)                                                     \
                    return 1;                                                        \
                                                                                     \
                entry.names = tmp;                                                   \
                                                                                     \
                tmp = realloc(                                                       \
                    entry.values,                                                    \
                    entry.records_size * sizeof(char *)                              \
                );                                                                   \
                                                                                     \
                if (tmp == NULL)                                                     \
                    return 1;                                                        \
                                                                                     \
                entry.values = tmp;                                                  \
                                                                                     \
                entry.names[entry.records_size - 1] = strdup(field_name);            \
                entry.values[entry.records_size - 1] = strdup(field_value);          \
            }

            #define X_POLICY(name, value) \
                is_protected = 1;

            #define SEP_REMOVE_RECORD(record) REMOVE_ENTRY;

            #define REMOVE_RECORD(field_name, field_value)               \
               {                                                                   \
                char **tmp = realloc(                                                \
                    remove_entry.names,                                                     \
                    ++remove_entry.records_size * sizeof(char *)                            \
                );                                                                   \
                                                                                     \
                if (tmp == NULL)                                                     \
                    return 1;                                                        \
                                                                                     \
                remove_entry.names = tmp;                                                   \
                                                                                     \
                tmp = realloc(                                                       \
                    remove_entry.values,                                                    \
                    remove_entry.records_size * sizeof(char *)                              \
                );                                                                   \
                                                                                     \
                if (tmp == NULL)                                                     \
                    return 1;                                                        \
                                                                                     \
                remove_entry.values = tmp;                                                  \
                                                                                     \
                remove_entry.names[remove_entry.records_size - 1] = strdup(field_name);            \
                remove_entry.values[remove_entry.records_size - 1] = strdup(field_value);          \
            }

            {
                struct CatpkgRequest *request =
                    checked_request;     

                char *request_val =
                    request->value;       
                switch (
                    checked_request->protocol
                ) {
                    #include "behaviors/CATPKG_FILEINFO_SOURCES.inc"
                }
            }

            ADD_ENTRY;
            REMOVE_ENTRY;

            #undef X_INFO
            #undef X_POLICY
            #undef X
            #undef FIELD_SEP
            #undef REMOVE_RECORD
            #undef SEP_REMOVE_RECORD
        }
    }


    size_t pages_size_max = opsec_context->G_pages_size;

    if(opsec_context->D_pages_size > pages_size_max)
        pages_size_max = opsec_context->D_pages_size;
    if(opsec_context->RD_pages_size > pages_size_max)
        pages_size_max = opsec_context->RD_pages_size;
    if(opsec_context->N_pages_size > pages_size_max)
        pages_size_max = opsec_context->N_pages_size;

    bool opsec_break = 0;

    for(size_t page = 0; page < pages_size_max && opsec_break == 0; page++) {
        uint64_t *pageD =
            page < opsec_context->D_pages_size ? &opsec_context->D_pages[page] : NULL;

        uint64_t *pageR =
            page < opsec_context->R_pages_size ? &opsec_context->R_pages[page] : NULL;

        uint64_t *pageRD =
            page < opsec_context->RD_pages_size ? &opsec_context->RD_pages[page] : NULL;

        uint64_t *pageG =
            page < opsec_context->G_pages_size ? &opsec_context->G_pages[page] : NULL;
        
        #define PRE
        if(phase == OPSEC_VALIDATION) {
            if (flags & OPSEC_FLAG_REMOVE) {
                #include "behaviors/OPSeC_Remove.inc"
            }

            if (flags & OPSEC_FLAG_UPDATE){
                #include "behaviors/OPSeC_Update.inc"
            }

            if (flags & OPSEC_FLAG_INSTALL){
                    #include "behaviors/OPSeC_Install.inc"
            }
        }
        else {
            #define RESOLVE
            if (flags & OPSEC_FLAG_REMOVE) {
                    #include "behaviors/OPSeC_Remove.inc"
            }

            if (flags & OPSEC_FLAG_UPDATE){
                    #include "behaviors/OPSeC_Update.inc"
            }

            if (flags & OPSEC_FLAG_INSTALL){
                    #include "behaviors/OPSeC_Install.inc"
            }
            #undef RESOLVE
        }
        #undef PRE

    }

    for (
        size_t ai = index;
        ai < builder->requests_size;
        ai++
    ) {
        struct CatpkgRequest *checked_request =
            builder->requests[ai];

        if (checked_request == NULL)
            continue;

        if (checked_request->skip)
            continue;

        if(checked_request == request)
            break;

        char *request_val =
            checked_request->value;

        char *request_result =
            checked_request->result;

        if (request_val == NULL)
            continue;

        size_t entry_id = SIZE_MAX;

        for (size_t _entry_id = 0; _entry_id < opsec_context -> G_entries_ids_size; ++_entry_id)
        {
            char *value = NULL;
            for(size_t i = 0; i < opsec_context -> G_entries_ids[_entry_id].records_size; i++) {
                if(strcmp(opsec_context -> G_entries_ids[_entry_id].names[i], "path") == 0) {
                    value = opsec_context -> G_entries_ids[_entry_id].values[i];
                    break;
                }
            }

            if(value != NULL && strcmp(value, request_val) == 0){
                entry_id = _entry_id;
            }
        }

        size_t eid_page = SIZE_MAX;
        uint64_t eid_mask = 0;

        if(entry_id != SIZE_MAX) {
            eid_page = entry_id / 64;
            eid_mask = UINT64_C(1) << (63 - (entry_id % 64));
        }

        const struct PackageField *check_field =
            opsec_context->file_info->fields;

        size_t pages_size_max = opsec_context->R_pages_size;

        if(opsec_context->G_pages_size > pages_size_max)
            pages_size_max = opsec_context->G_pages_size;
        if(opsec_context->D_pages_size > pages_size_max)
            pages_size_max = opsec_context->D_pages_size;
        if(opsec_context->RD_pages_size > pages_size_max)
            pages_size_max = opsec_context->RD_pages_size;
        if(opsec_context->N_pages_size > pages_size_max)
            pages_size_max = opsec_context->N_pages_size;
        if(opsec_context->C_pages_size > pages_size_max)
            pages_size_max = opsec_context->C_pages_size;
        if(opsec_context->P_pages_size > pages_size_max)
            pages_size_max = opsec_context->P_pages_size;

        bool opsec_break = 0;

        for(size_t page = 0; page < pages_size_max && opsec_break == 0; page++) {
            uint64_t *pageD =
                page < opsec_context->D_pages_size ? &opsec_context->D_pages[page] : NULL;

            uint64_t *pageR =
                page < opsec_context->R_pages_size ? &opsec_context->R_pages[page] : NULL;

            uint64_t *pageRD =
                page < opsec_context->RD_pages_size ? &opsec_context->RD_pages[page] : NULL;

            uint64_t *pageG =
                page < opsec_context->G_pages_size ? &opsec_context->G_pages[page] : NULL;

            uint64_t *pageP =
                page < opsec_context->P_pages_size ? &opsec_context->P_pages[page] : NULL;

            #define PROTOCOL

            if(phase == OPSEC_VALIDATION) {
                if (flags & OPSEC_FLAG_REMOVE)
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Remove.inc"
                    }

                if (flags & OPSEC_FLAG_UPDATE){
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Update.inc"
                    }
                }

                if (flags & OPSEC_FLAG_INSTALL){
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Install.inc"
                    }
                }
            }
            else {
                #define RESOLVE
                if (flags & OPSEC_FLAG_REMOVE)
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Remove.inc"
                    }

                if (flags & OPSEC_FLAG_UPDATE){
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Update.inc"
                    }
                }

                if (flags & OPSEC_FLAG_INSTALL){
                    switch (
                        checked_request->protocol
                    ) {
                        #include "behaviors/OPSeC_Install.inc"
                    }
                }
                #undef RESOLVE
            }

            #undef PROTOCOL
        }
    }
}