case OPSEC_CREATE:
    PackageInfo_Free(((struct OPSeC_Context *)context) -> package_info);

    PackageInfo_Free(((struct OPSeC_Context *)context) -> file_info);

    if(((struct OPSeC_Context *)context) -> last_opsec_context == NULL)
        free(((struct OPSeC_Context *)context) -> G_package_ids);


    for (size_t i = 0; i < ((struct OPSeC_Context *)context) -> G_entries_ids_size; ++i) {
        for(size_t j = 0; j < ((struct OPSeC_Context *)context) -> G_entries_ids[i].records_size; j++) {
            free(((struct OPSeC_Context *)context) -> G_entries_ids[i].names[j]);
            free(((struct OPSeC_Context *)context) -> G_entries_ids[i].values[j]);
        }    
    }
    free(((struct OPSeC_Context *)context) -> G_entries_ids);

    free(((struct OPSeC_Context *)context) -> R_pages);
    
    free(((struct OPSeC_Context *)context) -> RD_pages);

    free(((struct OPSeC_Context *)context) -> D_pages);

    free(((struct OPSeC_Context *)context) -> N_pages);

    free(((struct OPSeC_Context *)context) -> C_pages); 

    free(context);
    break;
case PAYLOAD_EXCLUDE:

    break;
default:
	free(
        builder->requests[i]->context
    );