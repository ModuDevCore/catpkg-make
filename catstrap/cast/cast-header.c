#define _GNU_SOURCE

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/sendfile.h>

#include "cast-format.h"

int main(int argc, char *argv[])
{
    // CAST CLI
	int fd = open(argv[0], O_RDONLY);
    lseek(fd, CAST_HEADER_FREE, SEEK_SET);

    struct header_posix_ustar hpu;

	int memfd = memfd_create("cast-cli", 0);

    while(read(fd, &hpu, sizeof(hpu)) > 0) {
        if(memcmp(hpu.magic, "ustar", 5) != 0)
            break;
        unsigned long long file_size =
            strtoull(hpu.size, NULL, 8);
        if(strcmp(hpu.name, "cast-cli.elf") == 0) {
            off_t pos = lseek(fd, 0, SEEK_CUR);

            sendfile(
                memfd,
                fd,
                &pos,
                file_size
            );
            break;
        }


       off_t skip =
            (off_t)((file_size + 511) / 512) * 512;

        if (lseek(fd, skip, SEEK_CUR) == (off_t)-1) {
            perror("lseek");
            close(fd);
            return 1;
        }
    }

	close(fd);

	char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", memfd);
    char **cast_catstrap_argv =
        malloc(
            (
                1 +
                argc +
                1
            ) * sizeof(*cast_catstrap_argv)
        );

    if (cast_catstrap_argv == NULL) {
        _exit(1);
    }

    size_t i = 0;

    cast_catstrap_argv[i++] =
        argv[0];

    for (int j = 1; j < argc; j++)
        cast_catstrap_argv[i++] = argv[j];

    cast_catstrap_argv[i++] = NULL;

    execv(
        path,
        cast_catstrap_argv
    );

	return 0;
}