#include "install.h"
#include "remove.h"
#include "build.h"
#include "database.h"
#include "utils/fs.h"
#include "catpkg/pkginfo.h"

#include "../catstrap/cast/include/cast-format.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>

#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdint.h>

#include <fcntl.h>
#include <ftw.h>

#include <poll.h>

// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude -g3 -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -c bootstrap/catstrap.c -o extra/x86_64/catstrap-debug.o
// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude -g3 -O0 -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined catpkg-build/build.c src/make-cli-debuger.c extra/x86_64/catstrap-debug.o -o catpkg-make-debugger-x86_64

#define __CATPKG_BUILD_BUILDER_SUFIX__ "1.0.0b"
#define __CATPKG_BUILDER_SUFIX__ "2.0.0b"
#define __CATSTRAP_CATPKG_BUILDER_SUFIX__ __CATPKG_BUILDER_SUFIX__

#define GREETING \
    "Welcome to the CATPKG-DEBUG utility builder! Choose which build you need:\n" \
    "1) Building catstrap-catpkg-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64\n" \
    "2) Building catstrap-catpkg-lite-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64\n" \
    "3) Building catstrap-catpkg-net-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64\n" \
    "4) Building catstrap-catpkg-custom-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64\n" \
    "5) Building catstrap-debug ( Empty )\n\n" \
    "6) Building the catpkg-debug-x86_64@" __CATPKG_BUILDER_SUFIX__ ".catpackage\n" \
    "8) Building the catpkg-build-debug-x86_64@" __CATPKG_BUILD_BUILDER_SUFIX__ ".catpackage\n" \
    "9) catpkg-make-debugger-x86_64 ( Update )\n"

#define GCC_X86_64 "gcc"

static int remove_entry(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)st;
    (void)typeflag;
    (void)ftwbuf;

    return remove(path);
}

int remove_tmp_directory(void)
{
    printf(
        "[INFO] Removing the TMP directory: %s\n",
        ".tmp"
    );

    if (
        nftw(
            ".tmp",
            remove_entry,
            16,
            FTW_DEPTH | FTW_PHYS
        ) != 0
    ) {
        if (errno != ENOENT)
            return 1;
    }

    return 0;
}

static int copy_file(
    const char *src,
    const char *dst
)
{
    int input = open(src, O_RDONLY);

    if (input < 0) {
        perror(src);
        return 1;
    }

    struct stat st;

    if (fstat(input, &st) < 0) {
        perror("fstat");
        close(input);
        return 1;
    }

    int output = open(
        dst,
        O_WRONLY | O_CREAT | O_TRUNC,
        st.st_mode & 0777
    );

    if (output < 0) {
        perror(dst);
        close(input);
        return 1;
    }

    if (fchmod(output, st.st_mode & 0777) < 0) {
        perror("fchmod");
        close(input);
        close(output);
        return 1;
    }

    char buffer[65536];
    ssize_t bytes_read;

    while (
        (bytes_read = read(
            input,
            buffer,
            sizeof(buffer)
        )) > 0
    ) {
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(
                output,
                buffer + total_written,
                (size_t)(bytes_read - total_written)
            );

            if (bytes_written < 0) {
                if (errno == EINTR)
                    continue;

                perror("write");
                close(input);
                close(output);
                return 1;
            }

            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(input);
        close(output);
        return 1;
    }

    if (close(input) < 0) {
        perror("close");
        close(output);
        return 1;
    }

    if (close(output) < 0) {
        perror("close");
        return 1;
    }

    return 0;
}

int catpkg_embed_binary(
    const char *input_path,
    FILE *output
)
{
    if (
        input_path == NULL ||
        output == NULL
    ) {
        errno = EINVAL;
        return 1;
    }

    FILE *input =
        fopen(
            input_path,
            "rb"
        );

    if (input == NULL)
        return 1;

    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        return 1;
    }

    long file_size =
        ftell(input);

    if (file_size < 0) {
        fclose(input);
        return 1;
    }

    rewind(input);

    size_t package_size =
        (size_t)file_size;

    if (
        fprintf(
            output,
            "#include <stddef.h>\n"
            "#include <stdint.h>\n\n"
            "const uint8_t catpkg_embedded_package[] = {\n"
        ) < 0
    ) {
        fclose(input);
        return 1;
    }

    uint8_t buffer[4096];
    size_t offset = 0;
    size_t read_size;

    while (
        (
            read_size = fread(
                buffer,
                1,
                sizeof(buffer),
                input
            )
        ) > 0
    ) {
        for (
            size_t i = 0;
            i < read_size;
            i++
        ) {
            if (
                fprintf(
                    output,
                    "    0x%02X%s",
                    buffer[i],
                    ((offset + 1) % 12 == 0)
                        ? "\n"
                        : ""
                ) < 0
            ) {
                fclose(input);
                return 1;
            }

            if (offset + 1 < package_size) {
                if (
                    fputc(
                        ',',
                        output
                    ) == EOF
                ) {
                    fclose(input);
                    return 1;
                }
            }

            offset++;
        }
    }

    if (ferror(input)) {
        fclose(input);
        return 1;
    }

    fclose(input);

    if (
        fprintf(
            output,
            "\n};\n\n"
            "const size_t catpkg_embedded_package_size = %zu;\n",
            package_size
        ) < 0
    ) {
        return 1;
    }

    return 0;
}

int bootstrap_update(void)
{
    printf("Building bootstrap debug...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl(
            "/usr/bin/" GCC_X86_64,
            GCC_X86_64,
            "-D_DEFAULT_SOURCE",
            "-D_XOPEN_SOURCE=700",
            "-std=c23",
            "-Wextra",
            "-Wpedantic",
            "-Iinclude",
            "-fPIC",
            "-g3",
            "-O0",
            "-fno-omit-frame-pointer",
            "-fsanitize=address",
            "-c",
            "bootstrap/catstrap.c",
            "-o",
            "extra/x86_64/catstrap-debug.o",
            (char *)NULL
        );
        perror("gcc");
        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0
    ) {
        fprintf(
            stderr,
            "gcc: bootstrap debug compilation failed\n"
        );

        return 1;
    }

    return 0;
}

static int build_catstrap(
    int code
)
{
    if (bootstrap_update() != 0)
        return 1;

    const char *package_folder =
        code == 1
            ? "catstrap-catpkg"
        : code == 2
            ? "catstrap-catpkg-lite"
        : code == 3
            ? "catstrap-catpkg-net"
            : "catstrap-catpkg-custom";

    const char *output_name =
        code == 1
            ? "catstrap-catpkg-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
        : code == 2
            ? "catstrap-catpkg-lite-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
        : code == 3
            ? "catstrap-catpkg-net-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
            : "catstrap-catpkg-custom-debug-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64";

    execl(
        "build-cast-debug.sh",
        "build-cast-debug.sh",
        GCC_X86_64,
        "./extra/x86_64",
        package_folder,
        CAST_HEADER_FREE_STR,
        output_name,
        (char *)NULL
    );

    perror("build-cast-debug.sh");
    return 1;
}

int main(
    int argc,
    char *argv[]
)
{
    (void)argc;
    (void)argv;

    printf(
        "========================================\n"
    );

    printf(
        "        CATPKG MAKE DEBUGGER\n"
    );

    printf(
        "The CATPKG DEBUG utility builder.\n"
    );

    printf(
        "========================================\n"
    );

    printf(GREETING);

    printf("Enter a number: ");

    char buf[3] = {0};

    buf[0] = getchar();

    int c = getchar();

    if (c != '\n') {
        buf[1] = (char)c;

        while (getchar() != '\n')
            ;
    }

    int code =
        atoi(buf);

    if (
        mkdir(
            ".tmp",
            0755
        ) != 0 &&
        errno != EEXIST
    ) {
        perror("mkdir");
        return 1;
    }

    switch (code) {
        case 1:
        case 2:
        case 3:
        case 4:
        {
            return build_catstrap(code);
        }

        case 5:
        {
            if (bootstrap_update() != 0)
                return 1;

            execl(
                "build-cast-debug.sh",
                "build-cast-debug.sh",
                GCC_X86_64,
                "./extra/x86_64",
                "catstrap-empty",
                CAST_HEADER_FREE_STR,
                "catstrap-debug-x86_64",
                (char *)NULL
            );

            perror("build-cast-debug.sh");
            return 1;
        }

        case 6:
        {
            if (remove_tmp_directory() != 0) {
                fprintf(
                    stderr,
                    "[FATAL] Failed to remove old temporary directory.\n"
                );

                return 1;
            }

            if (
                mkdir(".tmp", 0755) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(".tmp/usr", 0755) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(".tmp/usr/bin", 0755) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(".tmp/usr/lib", 0755) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(
                    ".tmp/usr/lib/catpkg",
                    0755
                ) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (chdir(".tmp") != 0) {
                perror("chdir");
                return 1;
            }

            if (bootstrap_update() != 0)
                return 1;

            printf(
                "Building \"catpkg-debug-x86_64@X.X.X.catpackage\"...\n"
            );

            if (
                copy_file(
                    "../extra/x86_64/catstraplib.so.0",
                    "./usr/lib/catpkg/catstraplib.so."
                    __CATPKG_BUILDER_SUFIX__
                ) != 0
            ) {
                return 1;
            }

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                execl(
                    "/usr/bin/" GCC_X86_64,
                    GCC_X86_64,
                    "-std=c23",
                    "-D_XOPEN_SOURCE=700",
                    "-D_DEFAULT_SOURCE",
                    "-Wextra",
                    "-Wpedantic",
                    "-I../include",
                    "-g3",
                    "-O0",
                    "-fno-omit-frame-pointer",
                    "-fsanitize=address",
                    "-fsanitize=undefined",
                    "../catpkg/cli.c",
                    "../catpkg/main.c",
                    "./usr/lib/catpkg/catstraplib.so."
                    __CATPKG_BUILDER_SUFIX__,
                    "-Wl,-rpath,/usr/lib",
                    "-o",
                    "usr/bin/catpkg",
                    (char *)NULL
                );

                perror("gcc");
                _exit(127);
            }

            int status;

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) < 0
            ) {
                perror("waitpid");
                return 1;
            }

            if (
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0
            ) {
                fprintf(
                    stderr,
                    "gcc: compilation failed\n"
                );

                return 1;
            }

            pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                execl(
                    "/usr/bin/" GCC_X86_64,
                    GCC_X86_64,
                    "-std=c23",
                    "-D_XOPEN_SOURCE=700",
                    "-D_DEFAULT_SOURCE",
                    "-Wextra",
                    "-Wpedantic",
                    "-I../include",
                    "-g3",
                    "-O0",
                    "-fno-omit-frame-pointer",
                    "-fsanitize=address",
                    "-fsanitize=undefined",
                    "../catpkg-command/main.c",
                    "./usr/lib/catpkg/catstraplib.so."
                    __CATPKG_BUILDER_SUFIX__,
                    "-Wl,-rpath,/usr/lib",
                    "-o",
                    "usr/bin/catpkg-command",
                    (char *)NULL
                );

                perror("gcc");
                _exit(127);
            }

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) < 0
            ) {
                perror("waitpid");
                return 1;
            }

            if (
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0
            ) {
                fprintf(
                    stderr,
                    "gcc: compilation failed\n"
                );

                return 1;
            }

            if (
                copy_file(
                    "../build-data/PACKAGEINFO",
                    "PACKAGEINFO"
                ) != 0
            ) {
                return 1;
            }

            if (
                copy_file(
                    "../build-data/catstrap-bin",
                    "usr/bin/catstrap-bin"
                ) != 0
            ) {
                return 1;
            }

            setenv(
                "CATSTRAP-BIN",
                "../build-data/catstrap-bin",
                0
            );

            catpkg_build("x86_64");

            printf(
                "Compilation successful.\n"
            );

            break;
        }

        case 8:
        {
            if (remove_tmp_directory() != 0) {
                fprintf(
                    stderr,
                    "[FATAL] Failed to remove old temporary directory.\n"
                );

                return 1;
            }

            if (
                mkdir(".tmp", 0755) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(
                    ".tmp/extra",
                    0755
                ) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (
                mkdir(
                    ".tmp/extra/COMMAND_DATA",
                    0755
                ) != 0 &&
                errno != EEXIST
            ) {
                perror("mkdir");
                return 1;
            }

            if (chdir(".tmp") != 0) {
                perror("chdir");
                return 1;
            }

            printf(
                "Building \"catpkg-build-debug-x86_64@X.X.X.catpackage\"...\n"
            );

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                execl(
                    "/usr/bin/" GCC_X86_64,
                    GCC_X86_64,
                    "-std=c23",
                    "-D_XOPEN_SOURCE=700",
                    "-D_DEFAULT_SOURCE",
                    "-Wextra",
                    "-Wpedantic",
                    "-I../include",
                    "-g3",
                    "-O0",
                    "-fno-omit-frame-pointer",
                    "-fsanitize=address",
                    "-fsanitize=undefined",
                    "../catpkg-build/build.c",
                    "../catpkg-build/catpkg-build-cli.c",
                    "/usr/lib/catstraplib.so.0",
                    "-Wl,-rpath,/usr/lib",
                    "-o",
                    "extra/COMMAND_DATA/catpkg-build-cli",
                    (char *)NULL
                );

                perror("gcc");
                _exit(127);
            }

            int status;

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) < 0
            ) {
                perror("waitpid");
                return 1;
            }

            if (
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0
            ) {
                fprintf(
                    stderr,
                    "gcc: compilation failed\n"
                );

                return 1;
            }

            if (
                copy_file(
                    "../build-data/catpkg-build/PACKAGEINFO",
                    "PACKAGEINFO"
                ) != 0
            ) {
                return 1;
            }

            if (
                copy_file(
                    "../build-data/catpkg-build/CATPKG-BUILD-COMMAND",
                    "CATPKG-BUILD-COMMAND"
                ) != 0
            ) {
                return 1;
            }

            setenv(
                "CATSTRAP-BIN",
                "../build-data/catstrap-bin",
                0
            );

            catpkg_build("x86_64");

            printf(
                "Compilation successful.\n"
            );

            break;
        }

        case 9:
        {
            if (bootstrap_update() != 0)
                return 1;

            printf(
                "Building catpkg-make-debugger-x86_64...\n"
            );

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                execl(
                    "/usr/bin/" GCC_X86_64,
                    GCC_X86_64,
                    "-D_DEFAULT_SOURCE",
                    "-D_XOPEN_SOURCE=700",
                    "-std=c23",
                    "-Wextra",
                    "-Wpedantic",
                    "-Iinclude",
                    "-g3",
                    "-O0",
                    "-fno-omit-frame-pointer",
                    "-fsanitize=address",
                    "-fsanitize=undefined",
                    "catpkg-build/build.c",
                    "src/make-cli-debuger.c",
                    "extra/x86_64/catstrap-debug.o",
                    "-fsanitize=address",
                    "-fsanitize=undefined",
                    "-o",
                    "catpkg-make-debugger-x86_64",
                    (char *)NULL
                );

                perror("gcc");
                _exit(127);
            }

            int status;

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) < 0
            ) {
                perror("waitpid");
                return 1;
            }

            if (
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0
            ) {
                fprintf(
                    stderr,
                    "gcc: compilation failed\n"
                );

                return 1;
            }

            printf(
                "Compilation successful.\n"
            );

            break;
        }

        default:
            printf(
                "Unknown option.\n"
            );

            break;
    }

    return 0;
}