/*
 * Section 0
 *
 * Global FileInfo options.
 *
 * Currently:
 *
 * #catpkg-version <version>
 * #use-options <path>
 */

case 0: {

    /*
     * End of global section.
     *
     * The next section is an info section.
     */

    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;


        long section_bytepos =
            reader->bytepos;

        char *content =
            package_reader_read_section(
                reader
            );

        if (content == NULL)
            return 1;

        ADD_FIELD(
            "info",
            content,
            section_bytepos
        );

        free(content);

        break;
    }


    /*
     * Ignore empty lines.
     */

    if (buffer[0] == '\0')
        break;


    /*
     * #catpkg-version
     */

    if (
        strncmp(
            buffer,
            "#catpkg-version ",
            16
        ) == 0
    ) {
        if (buffer[16] == '\0') {
            fprintf(
                stderr,
                "catpkg: catpkg version cannot be empty\n"
            );

            return 1;
        }

        ADD_FIELD(
            "#catpkg-version",
            buffer + 16,
            field_bytepos
        );

        break;
    }


    /*
     * #use-options
     */

    if (
        strncmp(
            buffer,
            "#use-options ",
            13
        ) == 0
    ) {
        if (buffer[13] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid options path: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#use-options",
            buffer + 13,
            field_bytepos
        );


        /*
         * Resolve options path relative
         * to the current FileInfo.
         */

        char directory[PATH_MAX];

        strncpy(
            directory,
            reader->base_path,
            sizeof(directory) - 1
        );

        directory[
            sizeof(directory) - 1
        ] = '\0';

        char *directory_path =
            dirname(directory);

        char *options_path =
            make_catpkg_path(
                "%s/%s",
                directory_path,
                buffer + 13
            );

        if (options_path == NULL)
            return 1;


        /*
         * Open options file.
         */


        int fd = open(options_path, O_RDONLY);

        if (fd == -1) {
            fprintf(
                stderr,
                "#use-options: cannot open options file \"%s\"\n",
                options_path
            );

            free(options_path);

            return 1;
        }



        /*
         * Initialize options reader.
         */

        struct PackageReader *options_reader = NULL;


        options_reader = package_reader_init(
            fd,
            options_path,
            options_reader
        );
        free(options_path);

        char option_buffer[PATH_MAX];


        /*
         * #use-options scope.
         */

        while (
            package_reader_getline(
                options_reader,
                option_buffer,
                sizeof(option_buffer)
            ) != NULL
        ) {
            option_buffer[
                strcspn(
                    option_buffer,
                    "\r\n"
                )
            ] = '\0';


            /*
             * Ignore empty lines.
             */

            if (option_buffer[0] == '\0')
                continue;


            /*
             * #required <section>=<path>
             */

             if (
                strncmp(
                    option_buffer,
                    "#required ",
                    10
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 10;

                char *separator =
                    strchr(
                        declaration,
                        '='
                    );

                if (
                    separator == NULL ||
                    separator == declaration ||
                    separator[1] == '\0'
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo required: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    package_reader_close(options_reader);
                    return 1;
                }

                /*
                 * Validate section number without modifying
                 * the original declaration.
                 */

                errno = 0;

                char *end = NULL;

                unsigned long long section_number =
                    strtoull(
                        declaration,
                        &end,
                        10
                    );

                if (
                    errno != 0 ||
                    end == declaration ||
                    end != separator ||
                    section_number > SIZE_MAX
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo section in: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    package_reader_close(options_reader);
                    return 1;
                }

                /*
                 * Keep the complete declaration:
                 *
                 * #required 0=catpkg
                 *
                 * PackageField:
                 *   name    = "#required"
                 *   value   = "0=catpkg"
                 *   section = section of #use-options
                 */

                ADD_FIELD(
                    "#required",
                    declaration,
                    options_reader -> bytepos
                );

                continue;
            }



            /*
             * #obsolete <section>=<path>
             */

             if (
                strncmp(
                    option_buffer,
                    "#obsolete ",
                    10
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 10;
                /*
                 * Keep the complete declaration:
                 *
                 * #obsolete 0
                 *
                 * PackageField:
                 *   name    = "#obsolete"
                 *   value   = "0"
                 *   section = section of #use-options
                 */

                ADD_FIELD(
                    "#obsolete",
                    declaration,
                    options_reader -> bytepos
                );

                continue;
            }            

            if (
                strncmp(
                    option_buffer,
                    "#protected ",
                    11
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 11;

                ADD_FIELD(
                    "#protected",
                    declaration,
                    options_reader -> bytepos
                );
                continue;
            }

            /*
             * Unknown external option.
             */

            fprintf(
                stderr,
                "#use-options: unknown option: \"%s\"\n",
                option_buffer
            );

            package_reader_close(options_reader);
            return 1;
        }

        package_reader_close(options_reader);
        break;
    }


    /*
     * Unknown global FileInfo option.
     */

    fprintf(
        stderr,
        "catpkg: unknown FileInfo global option: \"%s\"\n",
        buffer
    );

    return 1;
}


/*
 * Sections 1..N
 *
 * FileInfo entry.
 */

default: {

    /*
     * End of current info section.
     *
     * Read the next section in advance,
     * store its raw content as "info",
     * then return the reader to its start.
     */

    if (strcmp(buffer, "-") == 0) {
        (*current_section)++;

        long section_bytepos =
            reader->bytepos;

        char *content =
            package_reader_read_section(
                reader
            );

        if (content == NULL)
            return 1;

        ADD_FIELD(
            "info",
            content,
            section_bytepos
        );

        free(content);

        break;
    }


    /*
     * Ignore empty lines.
     */

    if (buffer[0] == '\0')
        break;


    /*
     * #type
     */

    if (
        strncmp(
            buffer,
            "#type ",
            6
        ) == 0
    ) {
        if (buffer[6] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo type: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#type",
            buffer + 6,
            field_bytepos
        );

        break;
    }


    /*
     * #path
     */

    if (
        strncmp(
            buffer,
            "#path ",
            6
        ) == 0
    ) {
        if (buffer[6] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo path: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#path",
            buffer + 6,
            field_bytepos
        );

        break;
    }

    /*
     * #path
     */

    if (
        strncmp(
            buffer,
            "#target ",
            8
        ) == 0
    ) {
        if (buffer[8] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo target: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#target",
            buffer + 8,
            field_bytepos
        );

        break;
    }

    /*
     * Unknown FileInfo option.
     */

    fprintf(
        stderr,
        "catpkg: unknown FileInfo option: \"%s\"\n",
        buffer
    );

    return 1;
}