#include <stdio.h>
#include "utils/allow.h"

int catpkg_confirm()
{
    char input[16];

    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL)
        return 0;

    if (input[0] == '\n' ||
        input[0] == 'y' ||
        input[0] == 'Y') {

        return input[0];
    }

    return EOF;
}