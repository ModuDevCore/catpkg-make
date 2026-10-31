case 0: {
    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;
        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package name cannot be empty\n"
        );

        return -1;
    }

    ADD_FIELD(
        "name",
        buffer,
        field_bytepos
    );


    break;
}


/*
 * Section 1 — package version.
 */

case 1: {
    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;
        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package version cannot be empty\n"
        );

        return -1;
    }

    ADD_FIELD(
        "version",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 2 — architecture.
 *
 * Empty section means that architecture
 * is not specified.
 */

case 2: {

    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;
        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: invalid empty package architecture\n"
        );

        return -1;
    }

    ADD_FIELD(
        "architecture",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 3 — description.
 *
 * Empty section means empty description.
 */

case 3: {

    if (strcmp(buffer, "-") == 0) {

        /*
         * Preserve the old behaviour:
         * "-" directly in place of description
         * means an empty description.
         */

        ADD_FIELD(
            "description",
            "",
            field_bytepos
        );

        (*current_section)++;

        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: invalid empty package description\n"
        );

        return -1;
    }

    ADD_FIELD(
        "description",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 4 — SHA.
 */

case 4: {

    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;
        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package SHA cannot be empty\n"
        );

        return -1;
    }

    ADD_FIELD(
        "SHA",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 5 — package size.
 */

case 5: {

    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;
        break;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package size cannot be empty\n"
        );

        return -1;
    }

    ADD_FIELD(
        "size",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 6 — dependencies.
 */

case 6: {
    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;

        long section_bytepos =
            reader->bytepos;

        char *content =
            package_reader_read_section(
                reader
            );

        if (content == NULL){
            return -1;
        }

        ADD_FIELD(
            "operations",
            content,
            section_bytepos
        );

        free(content);

        break;
    }


    if (buffer[0] == '\0')
        break;

    ADD_FIELD_DEPENDENCY(        
        "dependency",
        buffer,
        field_bytepos
    );

    break;
}


/*
 * Section 7 — filesystem operations.
 */

case 7: {

    if (buffer[0] == '\0')
        break;

    if(
        strncmp(
            buffer,
            "#WHERE ",
            7
        ) == 0
    ) {
        char *is_operator = strstr(buffer, ") is (");
        
        char *left_operand = strstr(buffer, "(") + 1;
        ptrdiff_t left_operand_size = is_operator - left_operand;
        
        char *right_operand = is_operator + 6;
        ptrdiff_t right_operand_size = strstr(buffer, ")") - right_operand;

        if(strncmp(left_operand, right_operand, (ptrdiff_t) (left_operand > right_operand ? left_operand : right_operand) != 0)) {
            while (1) {

                field_bytepos = reader->bytepos;

                if (
                    package_reader_getline(
                        reader,
                        buffer,
                        sizeof(buffer)
                    ) == NULL
                ) {
                    break;
                }

                buffer[strcspn(buffer, "\r\n")] = '\0';
            
                if(
                    strncmp(
                        buffer,
                        "#WHERE_END",
                        7
                    ) == 0
                )
                    break;
            }
        }
        break;
    }

    if(
        strncmp(
            buffer,
            "#WHERE_END",
            7
        ) == 0
    ) {
        break;
    }

    if (
        strncmp(
            buffer,
            "#prot ",
            6
        ) == 0
    ) {
        char *protocol = buffer + 6;

        /*
         * Find separator between protocol number and value.
         */
        char *value = strchr(protocol, ' ');

        if (
            *protocol == '\0' ||
            value == NULL ||
            value == protocol ||
            value[1] == '\0'
        ) {
            fprintf(
                stderr,
                "catpkg: invalid #prot syntax: %s\n",
                buffer
            );

            free(*section);
            return -1;
        }

        /*
         * Protocol must contain only decimal digits.
         */
        for (char *ptr = protocol; ptr < value; ptr++) {
            if (*ptr < '0' || *ptr > '9') {
                fprintf(
                    stderr,
                    "catpkg: invalid protocol number: \"%.*s\"\n",
                    (int)(value - protocol),
                    protocol
                );

                free(*section);
                return -1;
            }
        }

        ADD_FIELD(
            "#prot",
            buffer + 6,
            field_bytepos
        );

        break;
    }

    /*
     * Unknown operation.
     */

    fprintf(
        stderr,
        "catpkg: unknown filesystem operation: \"%s\"\n",
        buffer
    );

    free(*section);

    return -1;
}