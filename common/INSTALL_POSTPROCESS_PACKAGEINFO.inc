#ifndef __PACKAGEINFO__
#error "__PACKAGEINFO__ is not defined"
#endif

#ifndef __PACKAGE_COMMANDS_PATH__
#error "__PACKAGE_COMMANDS_PATH__ is not defined"
#endif


char *commands_data_dir_path = make_catpkg_path("%s/DATA", __PACKAGE_COMMANDS_PATH__);

#define __COMMANDS_DATA_DIR__ commands_data_dir_path

struct MacroDeclaration macros[] = {
    #define MACRO(name, value, is_string) { name, (intptr_t) value, is_string },

    #include "postprocess_macros/commands.inc"
    #include "postprocess_macros/protocols.inc"

    #undef MACRO
};

#undef __COMMANDS_DATA_DIR__

char command[PATH_MAX];

snprintf(
    command,
    sizeof(command),
    "tar -xOf '%s' '%s'",
    path,
    CATPKG_PACKAGEINFO
);

FILE *file = popen(
    command,
    "r"
);

if (file == NULL)
    return 1;

int input_fd = fileno(file);

if (input_fd == -1) {
    pclose(file);
    return 1;
}

int p[2];

if (pipe(p) < 0)
    return 1;

if (postproccess_format_file_macros(
        input_fd,
        p[1],
        macros,
        sizeof(macros) / sizeof(macros[0]),
        0
    ) == -1) {

    fprintf(
        stderr,
        "catpkg: failed to postprocess %s\n",
        CATPKG_PACKAGEINFO
    );

    return 1;
}

free(commands_data_dir_path);
close(p[1]);
pclose(file);

__PACKAGEINFO__ = CATPKG_FD_Parse(p[0], "???", __PACKAGEINFO__);
CATPKG_ReadAll(__PACKAGEINFO__);

if (__PACKAGEINFO__ == NULL)
    return 1;
