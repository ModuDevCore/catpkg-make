#include "utils/postprocess.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>


static int write_all(
    int fd,
    const void *buffer,
    size_t size
) {
    const char *ptr = buffer;

    while (size > 0) {
        ssize_t written = write(
            fd,
            ptr,
            size
        );

        if (written < 0) {
            if (errno == EINTR)
                continue;

            return 1;
        }

        ptr += written;
        size -= (size_t)written;
    }

    return 0;
}


static int write_char(
    int fd,
    char c
) {
    return write_all(
        fd,
        &c,
        1
    );
}


/*
 * Returns:
 *
 *  1 = byte read
 *  0 = EOF
* -1 = error
 */
static int read_char(
    int fd,
    int *c
) {
    unsigned char value;

    ssize_t result;

    do {
        result = read(
            fd,
            &value,
            1
        );
    } while (
        result < 0 &&
        errno == EINTR
    );

    if (result < 0)
        return -1;

    if (result == 0)
        return 0;

    *c = (int)value;

    return 1;
}

ssize_t postproccess_format_file_macros(
    int input_fd,
    int output_fd,
    struct MacroDeclaration macros[],
    size_t macros_size,
    size_t min_output
) {
    size_t merged_macros_size = macros_size;

    struct MacroDeclaration *merged_macros =
        malloc(merged_macros_size * sizeof(*merged_macros));

    if (merged_macros == NULL && merged_macros_size != 0)
        return -1;

    if (macros_size != 0)
        memcpy(merged_macros, macros, macros_size * sizeof(*merged_macros));

    const char *env = getenv("POSTPROC_CUSTOM_MACRO");
    char *buffer_env = NULL;

    if (env != NULL && *env != '\0') {
        buffer_env = strdup(env);

        if (buffer_env == NULL) {
            free(merged_macros);
            return -1;
        }

        char *cursor = buffer_env;

        while (*cursor != '\0') {
            char *end = strchr(cursor, ':');

            if (end != NULL)
                *end = '\0';

            char *separator = strchr(cursor, '=');

            if (separator != NULL && separator != cursor) {
                *separator = '\0';

                char *name = strdup(cursor);
                char *value = strdup(separator + 1);

                if (name == NULL || value == NULL) {
                    free(name);
                    free(value);
                    goto parse_error;
                }

                struct MacroDeclaration *tmp = realloc(
                    merged_macros,
                    (merged_macros_size + 1) * sizeof(*merged_macros)
                );

                if (tmp == NULL) {
                    free(name);
                    free(value);
                    goto parse_error;
                }

                merged_macros = tmp;

                merged_macros[merged_macros_size++] = (struct MacroDeclaration) {
                    .name = name,
                    .value = (intptr_t)value,
                    .is_string = true
                };
            }

            if (end == NULL)
                break;

            cursor = end + 1;
        }

        free(buffer_env);
    }

    goto parse_done;

    parse_error:
    for (size_t i = macros_size; i < merged_macros_size; i++) {
        free(merged_macros[i].name);
        free((char *)merged_macros[i].value);
    }

    free(merged_macros);
    free(buffer_env);
    return -1;

    parse_done:

    int current;
    int read_result;

    size_t output_size = 0;
    size_t relative_min_output = min_output;

    #define ACCOUNT_WRITE(size) \
        do { \
            output_size += (size); \
        } while (0)

    #define CHECK_OUTPUT() \
        do { \
            if ( \
                min_output != 0 && \
                output_size >= relative_min_output \
            ) \
                goto done; \
        } while (0)

    while (
        (read_result = read_char(
            input_fd,
            &current
        )) > 0
    ) {
        /*
         * Escaped macro:
         *
         * \__MACRO__ -> __MACRO__
         */
        if (current == '\\') {
            int next;

            read_result = read_char(
                input_fd,
                &next
            );

            if (read_result < 0)
                goto error;

            if (read_result == 0) {
                if (
                    write_char(
                        output_fd,
                        '\\'
                    ) != 0
                )
                    goto error;

                ACCOUNT_WRITE(1);
                break;
            }

            if (next != '_') {
                if (
                    write_char(
                        output_fd,
                        '\\'
                    ) != 0 ||
                    write_char(
                        output_fd,
                        (char)next
                    ) != 0
                )
                    goto error;

                ACCOUNT_WRITE(2);
                CHECK_OUTPUT();
                continue;
            }

            int next2;

            read_result = read_char(
                input_fd,
                &next2
            );

            if (read_result < 0)
                goto error;

            if (read_result == 0) {
                if (
                    write_all(
                        output_fd,
                        "\\_",
                        2
                    ) != 0
                )
                    goto error;

                ACCOUNT_WRITE(2);
                break;
            }

            if (next2 != '_') {
                if (
                    write_all(
                        output_fd,
                        "\\_",
                        2
                    ) != 0 ||
                    write_char(
                        output_fd,
                        (char)next2
                    ) != 0
                )
                    goto error;

                ACCOUNT_WRITE(3);
                CHECK_OUTPUT();
                continue;
            }

            if (
                write_all(
                    output_fd,
                    "__",
                    2
                ) != 0
            )
                goto error;

            ACCOUNT_WRITE(2);

            int prev = -1;

            while (
                (read_result = read_char(
                    input_fd,
                    &current
                )) > 0
            ) {
                if (
                    prev == '_' &&
                    current == '_'
                ) {
                    if (
                        write_all(
                            output_fd,
                            "__",
                            2
                        ) != 0
                    )
                        goto error;

                    ACCOUNT_WRITE(2);

                    prev = -1;
                    break;
                }

                if (prev != -1) {
                    if (
                        write_char(
                            output_fd,
                            (char)prev
                        ) != 0
                    )
                        goto error;

                    ACCOUNT_WRITE(1);
                }

                prev = current;
            }

            if (read_result < 0)
                goto error;

            if (
                read_result == 0 &&
                prev != -1
            ) {
                if (
                    write_char(
                        output_fd,
                        (char)prev
                    ) != 0
                )
                    goto error;

                ACCOUNT_WRITE(1);
            }

            CHECK_OUTPUT();
            continue;
        }

        /*
         * Normal character.
         */
        if (current != '_') {
            if (
                write_char(
                    output_fd,
                    (char)current
                ) != 0
            )
                goto error;

            ACCOUNT_WRITE(1);
            CHECK_OUTPUT();
            continue;
        }

        /*
         * Possible beginning of __MACRO__.
         */
        int next;

        read_result = read_char(
            input_fd,
            &next
        );

        if (read_result < 0)
            goto error;

        if (read_result == 0) {
            if (
                write_char(
                    output_fd,
                    '_'
                ) != 0
            )
                goto error;

            ACCOUNT_WRITE(1);
            break;
        }

        if (next != '_') {
            if (
                write_char(
                    output_fd,
                    '_'
                ) != 0 ||
                write_char(
                    output_fd,
                    (char)next
                ) != 0
            )
                goto error;

            ACCOUNT_WRITE(2);
            CHECK_OUTPUT();
            continue;
        }

        /*
         * Read macro name.
         */
        size_t capacity = 32;
        size_t length = 0;

        char *name = malloc(capacity);

        if (name == NULL)
            goto error;

        int closed = 0;
        int prev = -1;

        while (
            (read_result = read_char(
                input_fd,
                &current
            )) > 0
        ) {
            if (
                prev == '_' &&
                current == '_'
            ) {
                closed = 1;
                break;
            }

            if (prev != -1) {
                if (
                    length + 1 >=
                    capacity
                ) {
                    size_t new_capacity =
                        capacity * 2;

                    if (
                        new_capacity <
                        capacity
                    ) {
                        free(name);
                        goto error;
                    }

                    char *tmp = realloc(
                        name,
                        new_capacity
                    );

                    if (tmp == NULL) {
                        free(name);
                        goto error;
                    }

                    name = tmp;
                    capacity =
                        new_capacity;
                }

                name[length++] =
                    (char)prev;
            }

            prev = current;
        }

        if (read_result < 0) {
            free(name);
            goto error;
        }

        /*
         * __MACRO without closing __.
         */
        if (!closed) {
            if (prev != -1) {
                if (
                    length + 1 >=
                    capacity
                ) {
                    size_t new_capacity =
                        capacity * 2;

                    if (
                        new_capacity <
                        length + 2
                    )
                        new_capacity =
                            length + 2;

                    if (
                        new_capacity <
                        capacity
                    ) {
                        free(name);
                        goto error;
                    }

                    char *tmp = realloc(
                        name,
                        new_capacity
                    );

                    if (tmp == NULL) {
                        free(name);
                        goto error;
                    }

                    name = tmp;
                    capacity =
                        new_capacity;
                }

                name[length++] =
                    (char)prev;
            }

            if (
                write_all(
                    output_fd,
                    "__",
                    2
                ) != 0 ||
                write_all(
                    output_fd,
                    name,
                    length
                ) != 0
            ) {
                free(name);
                goto error;
            }

            ACCOUNT_WRITE(
                2 + length
            );

            free(name);
            break;
        }

        name[length] = '\0';

        /*
         * Find macro declaration.
         */
        struct MacroDeclaration *candidate =
            NULL;

        for (
            size_t i = 0;
            i < merged_macros_size;
            i++
        ) {
            if (
                strcmp(
                    merged_macros[i].name,
                    name
                ) == 0
            ) {
                candidate =
                    &merged_macros[i];

                break;
            }
        }

        if (candidate != NULL) {
            /*
             * String macro.
             */
            if (candidate->is_string) {
                const char *value =
                    (char *) candidate->value;

                size_t value_length =
                    strlen(value);

                if (
                    write_all(
                        output_fd,
                        value,
                        value_length
                    ) != 0
                ) {
                    free(name);
                    goto error;
                }

                ACCOUNT_WRITE(
                    value_length
                );
            }

            /*
             * Numeric macro.
             */
            else {
                char value_buffer[32];

                int value_length =
                    snprintf(
                        value_buffer,
                        sizeof(value_buffer),
                        "%ld",
                        candidate->value
                    );

                if (
                    value_length < 0 ||
                    (size_t)value_length >=
                        sizeof(value_buffer)
                ) {
                    free(name);
                    goto error;
                }

                if (
                    write_all(
                        output_fd,
                        value_buffer,
                        (size_t)value_length
                    ) != 0
                ) {
                    free(name);
                    goto error;
                }

                ACCOUNT_WRITE(
                    (size_t)value_length
                );
            }
        }
        else {
            /*
             * Unknown macro:
             * preserve it unchanged.
             */
            if (
                write_all(
                    output_fd,
                    "__",
                    2
                ) != 0 ||
                write_all(
                    output_fd,
                    name,
                    length
                ) != 0 ||
                write_all(
                    output_fd,
                    "__",
                    2
                ) != 0
            ) {
                free(name);
                goto error;
            }

            ACCOUNT_WRITE(
                length + 4
            );
        }

        free(name);

        CHECK_OUTPUT();
    }

    if (read_result < 0)
        goto error;

done:
    #undef ACCOUNT_WRITE
    #undef CHECK_OUTPUT

    return (ssize_t)output_size;

error:
    #undef ACCOUNT_WRITE
    #undef CHECK_OUTPUT

    return -1;
}
#undef ACCOUNT_WRITE
#undef CHECK_OUTPUT