#ifdef RESOLVE

    #ifdef PRE

    // for (size_t page = 0; page < opsec_context->P_pages_size; page++) {
    //     uint64_t flags = opsec_context->P_pages[page];

    //     for (size_t bit = 0; bit < 64; bit++) {
    //         uint64_t mask = UINT64_C(1) << (63 - bit);

    //         if (flags & mask) {
    //             size_t entry_id = page * 64 + bit;

    //             if (entry_id >= opsec_context->G_entries_ids_size)
    //                 break;

    //             struct FILEINFO_Entry *entry =
    //                 &opsec_context->G_entries_ids[entry_id];

    //             printf(
    //                 "ENTRY ID: %zu, records: %zu\n",
    //                 entry_id,
    //                 entry->records_size
    //             );

    //             for (size_t i = 0; i < entry->records_size; i++) {
    //                 printf(
    //                     "  [%zu] %s = %s\n",
    //                     i,
    //                     entry->names[i],
    //                     entry->values[i]
    //                 );
    //             }
    //         }
    //     }
    // }

    if (pageD != NULL && pageG != NULL)
        if ((*pageD & ~*pageG) != 0) {
            OPSEC_ACTION(MISSING_DEPENDENCY);
            opsec_break = 1;
        }
    
    #elif defined(POST)

    #elif defined(PROTOCOL)

        default:
            opsec_break = 1;

    #endif

#else
    #ifdef PROTOCOL
        default:
            opsec_break = 1;
    #endif

#endif