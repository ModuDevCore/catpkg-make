#ifdef RESOLVE

    #ifdef PRE

    if (pageD != NULL && pageG != NULL) {
        if ((*pageD & ~*pageG) != 0) {

            OPSEC_ACTION(MISSING_DEPENDENCY);
            opsec_break = 1;
        }
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