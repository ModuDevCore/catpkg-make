#ifdef RESOLVE

    #ifdef PRE

    #elif defined(POST)

    #elif defined(PROTOCOL)

        case REMOVE_FILE:
        case REMOVE_DIR:
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
            if (eid_page != SIZE_MAX)
                if (opsec_context->C_pages[eid_page] & eid_mask) {
                    opsec_break = 1;

                    OPSEC_ACTION(STILL_IN_USE);
                }
            break;

    #endif

#else

    #ifdef PRE

    #elif defined(POST)

    #elif defined(PROTOCOL)

        case REMOVE_FILE:
        case REMOVE_DIR:
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
            break;

    #endif

#endif