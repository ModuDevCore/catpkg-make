#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "catpkg/command.h"

int read_meta_command_field(
    int fd,
    char **value
)
{
    size_t value_size = 0;
    size_t value_used = 0;

    *value = NULL;

    for (;;) {
        char c;

        ssize_t result =
            read(fd, &c, 1);

        if (result == 0) {
            free(*value);
            *value = NULL;
            return 1;
        }

        if (result == -1) {
            perror("read");
            free(*value);
            *value = NULL;
            return 1;
        }

        if (c == '\0') {
            if (*value == NULL) {
                *value = malloc(1);

                if (*value == NULL)
                    return 1;

                (*value)[0] = '\0';
            }
            else {
                (*value)[value_used] = '\0';
            }

            return 0;
        }

        if (value_used + 1 >= value_size) {

            size_t new_size =
                value_size == 0
                    ? 64
                    : value_size * 2;

            char *new_value =
                realloc(
                    *value,
                    new_size
                );

            if (new_value == NULL) {
                perror("realloc");
                free(*value);
                *value = NULL;
                return 1;
            }

            *value = new_value;
            value_size = new_size;
        }

        (*value)[value_used++] = c;
    }
}
