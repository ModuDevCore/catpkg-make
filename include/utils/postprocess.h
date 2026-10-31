#ifndef CATPKG_UTILS_POSTPROCESS_H
#define CATPKG_UTILS_POSTPROCESS_H

#include <stddef.h>

struct MacroDeclaration {
    char *name;
    intptr_t value;

    bool is_string;
};

ssize_t postproccess_format_file_macros(
    int input_fd,
    int output_fd,
    struct MacroDeclaration macros[],
    size_t macros_size,
    size_t min_output
);

#endif