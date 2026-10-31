#ifdef RESOLVE

    #ifdef PRE

    #elif defined(POST)

    #elif defined(PROTOCOL)

    #endif

#else

    #ifdef PRE

    #elif defined(POST)

    #elif defined(PROTOCOL)

        case REMOVE_FILE:
        case REMOVE_DIR:
        {
            if (pageRD != NULL && *pageRD != 0) {
                opsec_break = 1;

                OPSEC_ACTION(PACKAGE_STILL_REQUIRED);

                break;
            }
            if (pageD != NULL && pageR != NULL) {
                if ((*pageD & *pageR) != 0) {
                    opsec_break = 1;

                    OPSEC_ACTION(SECTION_STILL_REQUIRED);

                    break;
                }
            }
            if (
                opsec_context->P_pages != NULL &&
                eid_page != SIZE_MAX &&
                eid_page < opsec_context->P_pages_size &&
                (opsec_context->P_pages[eid_page] & eid_mask)
            ) {
                opsec_break = 1;

                OPSEC_ACTION(POLICY_PROTECTED);

                break;
            }
        }

    #endif

#endif