#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h> 

char *make_catpkg_path(
    const char *format,
    ...
)
{
    va_list args;
    va_list args_copy;

    /*
     * First pass: calculate required string length.
     */
    va_start(args, format);
    va_copy(args_copy, args);

    int len = vsnprintf(
        NULL,
        0,
        format,
        args
    );

    va_end(args);

    if (len < 0) {
        va_end(args_copy);
        return NULL;
    }

    char *path = malloc(
        (size_t)len + 1
    );

    if (path == NULL) {
        va_end(args_copy);
        return NULL;
    }

    /*
     * Second pass: actually write the formatted string.
     */
    vsnprintf(
        path,
        (size_t)len + 1,
        format,
        args_copy
    );

    va_end(args_copy);

    return path;
}

int catpkg_path_contains(
    const char *directory,
    const char *path
)
{
    if (
        directory == NULL ||
        path == NULL
    ) {
        return 0;
    }

    size_t directory_len =
        strlen(directory);

    if (
        strncmp(
            directory,
            path,
            directory_len
        ) != 0
    ) {
        return 0;
    }

    /*
     * Exact match:
     *
     * /usr/lib/foo
     * /usr/lib/foo
     */

    if (path[directory_len] == '\0')
        return 1;

    /*
     * Child:
     *
     * /usr/lib/foo
     * /usr/lib/foo/bar
     */

    if (
        directory_len > 0 &&
        directory[directory_len - 1] == '/'
    ) {
        return 1;
    }

    return
        path[directory_len] == '/';
}