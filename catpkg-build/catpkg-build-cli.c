#include <stdio.h>
#include <string.h>
#include "build.h"

static int catpkg_parse_arch(
    const char *option
)
{
    if (strcmp(option, "--aarch64") == 0)
        return 1;

    if (strcmp(option, "--x86_64") == 0)
        return 2;

    return 0;
}

int main(int argc, char *argv[])
{
    int arch = 0;
    
    if(argc > 2) {
        arch = catpkg_parse_arch(argv[2]);
    }
    if(arch == 1)
        return catpkg_build("aarch64");
    else if(arch ==2)
        return catpkg_build("x86_64");
    else
        return catpkg_build(NULL);
}