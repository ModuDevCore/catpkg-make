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

// Manual build of catpkg-make-x86_64
// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude -c bootstrap/catstrap.c -o extra/x86_64/catstrap.o
// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude catpkg-build/build.c src/make-cli.c "extra/x86_64/catstrap.o" -static -o "catpkg-make-x86_64"

#define __CATPKG_BUILD_BUILDER_SUFIX__ "1.0.0b"
#define __CATPKG_BUILDER_SUFIX__ "2.0.0b"

#define __CATSTRAP_CATPKG_BUILDER_SUFIX__ __CATPKG_BUILDER_SUFIX__
#define __CATSTRAP_FILESYSTEM_SUFIX__ "1.0"

#define __BUILDING_CATSTRAP_CATPKG__ \
    "Building catstrap-catpkg-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-(aarch64/x86_64)"

#define __BUILDING_CATSTRAP_CATPKG_LITE__ \
    "Building catstrap-catpkg-lite-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-(aarch64/x86_64)"

#define __BUILDING_CATSTRAP_CATPKG_NET__ \
    "Building catstrap-catpkg-net-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-(aarch64/x86_64)"

#define __BUILDING_CATSTRAP_CATPKG_CUSTOM__ \
    "Building catstrap-catpkg-custom-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-(aarch64/x86_64)"

#define __BUILDING_CATSTRAP_EMPTY__ \
    "Building catstrap ( Empty )"

#define __BUILDING_CATPKG_CATPACKAGE__ \
    "Building the catpkg-(aarch64/x86_64)@" __CATPKG_BUILDER_SUFIX__ ".catpackage"

#define __BUILDING_CATSTRAPLIB_CATPACKAGE__ \
    "Building the catstraplib-(aarch64/x86_64)@" __CATPKG_BUILDER_SUFIX__ ".catpackage"

#define __BUILDING_CATSTRAP_BIN_CATPACKAGE__ \
    "Building the catstrap-bin-(aarch64/x86_64)@" __CATPKG_BUILDER_SUFIX__ ".catpackage"

#define __BUILDING_CATPKG_BUILD_CATPACKAGE__ \
    "Building the catpkg-build-(aarch64/x86_64)@" __CATPKG_BUILD_BUILDER_SUFIX__ ".catpackage"

#define __BUILDING_FILESYSTEM_CATPACKAGE__ \
    "Building the filesystem@" __CATSTRAP_FILESYSTEM_SUFIX__ ".catpackage"

#define __BUILDING_FILESYSTEM_ANDROID_CATPACKAGE__ \
    "Building the filesystem-android@" __CATSTRAP_FILESYSTEM_SUFIX__ ".catpackage"

#define __BUILDING_CATPKG_MAKE__ \
    "catpkg-make-(aarch64/x86_64) ( Update )"


#define GREETING \
    "Welcome to the CATPKG utility builder! Choose which build you need:\n" \
    "1) " __BUILDING_CATSTRAP_CATPKG__ "\n" \
    "2) " __BUILDING_CATSTRAP_CATPKG_LITE__ "\n" \
    "3) " __BUILDING_CATSTRAP_CATPKG_NET__ "\n" \
    "4) " __BUILDING_CATSTRAP_CATPKG_CUSTOM__ "\n" \
    "5) " __BUILDING_CATSTRAP_EMPTY__ "\n\n" \
    "6) " __BUILDING_CATPKG_CATPACKAGE__ "\n" \
    "7) " __BUILDING_CATSTRAPLIB_CATPACKAGE__ "\n" \
    "8) " __BUILDING_CATSTRAP_BIN_CATPACKAGE__ "\n" \
    "9) " __BUILDING_CATPKG_BUILD_CATPACKAGE__ "\n\n" \
    "10) " __BUILDING_FILESYSTEM_CATPACKAGE__ "\n" \
    "11) " __BUILDING_FILESYSTEM_ANDROID_CATPACKAGE__ "\n\n" \
    "12) " __BUILDING_CATPKG_MAKE__ "\n"

#define CHOOSE_A10E \
    "Select the processor architecture:\n" \
    "1) x86_64\n" \
    "2) aarch64\n"

#define GCC_AARCH64 "aarch64-linux-gnu-gcc"
#define GCC_X86_64  "gcc"

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

enum BuildOption {
    NONE,
    CATSTRAP_CATPKG,
    CATSTRAP_CATPKG_LITE,
    CATSTRAP_CATPKG_NET,
    CATSTRAP_CATPKG_CUSTOM,
    CATSTRAP_EMPTY,
    CATPKG_CATPACKAGE,
    CATSTRAPLIB_CATPACKAGE,
    CATSTRAP_BIN_CATPACKAGE,
    CATPKG_BUILD_CATPACKAGE,
    FILESYSTEM_CATPACKAGE,
    FILESYSTEM_ANDROID_CATPACKAGE,
    CATPKG_MAKE
};

int remove_tmp_directory(void)
{
    printf(
        "[INFO] Removing the TMP directory: %s\n",
        ".tmp"
    );
    /*
     * Remove old temporary installer directory.
     */
    if (nftw(
            ".tmp",
            remove_entry,
            16,
            FTW_DEPTH | FTW_PHYS
        ) != 0) {

        if (errno != ENOENT)
            return 1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst)
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
    if (input_path == NULL || output == NULL) {
        errno = EINVAL;
        return 1;
    }

    FILE *input = fopen(input_path, "rb");

    if (input == NULL)
        return 1;

    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        return 1;
    }

    long file_size = ftell(input);

    if (file_size < 0) {
        fclose(input);
        return 1;
    }

    rewind(input);

    size_t package_size = (size_t)file_size;

    if (fprintf(
        output,
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "const uint8_t catpkg_embedded_package[] = {\n"
    ) < 0) {
        fclose(input);
        return 1;
    }

    uint8_t buffer[4096];
    size_t offset = 0;
    size_t read;

    while ((read = fread(buffer, 1, sizeof(buffer), input)) > 0) {

        for (size_t i = 0; i < read; i++) {

            if (fprintf(
                output,
                "    0x%02X%s",
                buffer[i],
                ((offset + 1) % 12 == 0) ? "\n" : ""
            ) < 0) {
                fclose(input);
                return 1;
            }

            if (offset + 1 < package_size) {
                if (fputc(',', output) == EOF) {
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

    if (fprintf(
        output,
        "\n};\n\n"
        "const size_t catpkg_embedded_package_size = %zu;\n",
        package_size
    ) < 0) {
        return 1;
    }

    return 0;
}

int bootstrap_update()
{
    printf("Building bootstrap...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl(
            "update-bootstrap.sh",
            "update-bootstrap.sh",
            GCC_X86_64,
            GCC_AARCH64,
            "./extra/x86_64",
            "./extra/aarch64",
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
            "gcc: bootstrap compilation failed\n"
        );

        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    printf(
        "========================================\n"
    );

    printf(
        "        CATPKG MAKE\n"
    );
    printf(
        "The CATPKG utility builder.\n"
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

    int code = atoi(buf);

    /*
     * Remove leftovers from a previous installer run.
     */
    if (remove_tmp_directory() != 0) {

        fprintf(
            stderr,
            "[FATAL] Failed to remove old temporary directory.\n"
        );

        return 1;
    }

    // TMP struct
    switch (code) {
        case FILESYSTEM_CATPACKAGE:
        case FILESYSTEM_ANDROID_CATPACKAGE:
        case CATPKG_BUILD_CATPACKAGE:
        case CATSTRAP_CATPKG:
        case CATSTRAP_CATPKG_LITE:
        case CATSTRAP_CATPKG_NET:
        case CATPKG_CATPACKAGE:
        case CATSTRAPLIB_CATPACKAGE:
        case CATSTRAP_BIN_CATPACKAGE:
            if (mkdir(".tmp", 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return 1;
            }
            switch(code) {
                case CATPKG_BUILD_CATPACKAGE:
                    if (mkdir(".tmp/extra", 0755) != 0 && errno != EEXIST) {
                        perror("mkdir");
                        return 1;
                    }
                    if (mkdir(".tmp/extra/COMMAND_DATA", 0755) != 0 && errno != EEXIST) {
                        perror("mkdir");
                        return 1;
                    }
                    break;
                case CATPKG_CATPACKAGE:
                case CATSTRAP_CATPKG_LITE:
                case CATSTRAP_CATPKG_NET:
                case CATSTRAPLIB_CATPACKAGE:
                case CATSTRAP_BIN_CATPACKAGE:
                    if (mkdir(".tmp/usr", 0755) != 0 && errno != EEXIST) {
                        perror("mkdir");
                        return 1;
                    }
                    if (mkdir(".tmp/usr/bin", 0755) != 0 && errno != EEXIST) {
                        perror("mkdir");
                        return 1;
                    }
                    
                    switch(code) {
                        case CATSTRAPLIB_CATPACKAGE:
                            if (mkdir(".tmp/usr/lib", 0755) != 0 && errno != EEXIST) {
                                perror("mkdir");
                                return 1;
                            }
                            if (mkdir(".tmp/usr/lib/catpkg", 0755) != 0 && errno != EEXIST) {
                                perror("mkdir");
                                return 1;
                            }
                            break;
                    }

                    break;
            }
            break;
    }

    // build-data copy payload to tmp struct

    char cc = '\0'; // a10h - optional

    if(
        code == CATPKG_BUILD_CATPACKAGE ||
        code == CATSTRAP_CATPKG ||
        code == CATSTRAP_CATPKG_LITE ||
        code == CATSTRAP_CATPKG_NET ||
        code == CATPKG_CATPACKAGE ||
        code == CATPKG_MAKE ||
        code == CATSTRAP_BIN_CATPACKAGE ||
        code == CATSTRAPLIB_CATPACKAGE
    ) {
        printf(CHOOSE_A10E);
        
        printf("Enter a number: ");

        cc = getchar();

        while (getchar() != '\n')
            ;
        if(cc != '1' && cc != '2') {
            printf("Invalid architecture.\n");
            return 1;
        }  
    }

    switch(code) {
        case CATPKG_BUILD_CATPACKAGE:
            if (copy_file(
                "build-data/catpkg-build/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }

            if (copy_file(
                "build-data/catpkg-build/CATPKG-BUILD-COMMAND",
                ".tmp/CATPKG-BUILD-COMMAND"
            ) != 0) {
                return 1;
            }
            break;
       case CATPKG_CATPACKAGE:
            if (copy_file(
                "build-data/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }
            break;
        case CATSTRAP_BIN_CATPACKAGE:
            if (copy_file(
                "build-data/catstrap-bin/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }

            if (copy_file(
                "build-data/catstrap-bin/catstrap-bin",
                ".tmp/usr/bin/catstrap-bin"
            ) != 0) {
                return 1;
            }
            break;
        case CATSTRAPLIB_CATPACKAGE:
            bootstrap_update();

            if (copy_file(
                "build-data/catstraplib/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }

            switch (cc) {
                case '1':
                    if (copy_file(
                        "extra/x86_64/catstraplib.so.0",
                        ".tmp/usr/lib/catpkg/catstraplib.so."__CATPKG_BUILDER_SUFIX__
                    ) != 0) {
                        return 1;
                    }         
                    break;

                case '2':
                    if (copy_file(
                        "extra/aarch64/catstraplib.so.0",
                        ".tmp/usr/lib/catpkg/catstraplib.so."__CATPKG_BUILDER_SUFIX__
                    ) != 0) {
                        return 1;
                    }
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }
            break;
        case FILESYSTEM_CATPACKAGE:
            if (copy_file(
                "build-data/filesystem/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }
            break;
        case FILESYSTEM_ANDROID_CATPACKAGE:
            if (copy_file(
                "build-data/filesystem-android/PACKAGEINFO",
                ".tmp/PACKAGEINFO"
            ) != 0) {
                return 1;
            }
            break;
    } 

    // compile and build
    switch (code) {
        case FILESYSTEM_CATPACKAGE:
            chdir(".tmp");

            printf(__BUILDING_FILESYSTEM_CATPACKAGE__ "...\n");
            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);
            catpkg_build(NULL);
            break;
        case FILESYSTEM_ANDROID_CATPACKAGE:
            chdir(".tmp");

            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);
            printf(__BUILDING_FILESYSTEM_ANDROID_CATPACKAGE__ "...\n");
            catpkg_build(NULL);
            break;

        case CATSTRAP_CATPKG:
        case CATSTRAP_CATPKG_LITE:
        case CATSTRAP_CATPKG_NET:
        {
                bootstrap_update();

                switch (cc) {
                    case '1':
                        execl(
                            "build-cast.sh",
                            "???",
                            GCC_X86_64,
                            "./extra/x86_64",
                            code == 1 ?
                                "catstrap-catpkg"
                            : code == 2 ?
                                "catstrap-catpkg-lite"
                            : 
                                "catstrap-catpkg-net"
                            ,
                            CAST_HEADER_FREE_STR,
                            code == 1 ?
                                "catstrap-catpkg-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
                            : code == 2 ?
                                "catstrap-catpkg-lite-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
                            :
                                "catstrap-catpkg-net-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-x86_64"
                            ,
                            (char *)NULL
                        );
                        break;

                    case '2':
                        execl(
                            "build-cast.sh",
                            "???",
                            GCC_AARCH64,
                            "./extra/aarch64",
                            code == 1 ?
                                "catstrap-catpkg"
                            : code == 2 ?
                                "catstrap-catpkg-lite"
                            : 
                                "catstrap-catpkg-net"
                            ,
                            CAST_HEADER_FREE_STR,
                            code == 1 ?
                                "catstrap-catpkg-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-aarch64"
                            : code == 2 ?
                                "catstrap-catpkg-lite-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-aarch64"
                            :
                                "catstrap-catpkg-net-" __CATSTRAP_CATPKG_BUILDER_SUFIX__ "-aarch64"
                            ,
                            (char *)NULL
                        );
                        break;

                    default:
                        _exit(1);
                }
            break;
        }
        case CATSTRAP_BIN_CATPACKAGE:
            chdir(".tmp");

            bootstrap_update();

            printf(__BUILDING_CATSTRAP_BIN_CATPACKAGE__ "...\n");
            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);
            switch (cc) {
                case '1':
                    catpkg_build("x86_64");            
                    break;

                case '2':
                    catpkg_build("aarch64");             
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }            

            break;
        case CATSTRAPLIB_CATPACKAGE:
            chdir(".tmp");

            printf(__BUILDING_CATSTRAP_BIN_CATPACKAGE__ "...\n");
            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);
            switch (cc) {
                case '1':
                    catpkg_build("x86_64");            
                    break;

                case '2':
                    catpkg_build("aarch64");  
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }

            break;
        case CATPKG_CATPACKAGE:
        {      
            chdir(".tmp");

            bootstrap_update();

            printf(__BUILDING_CATPKG_CATPACKAGE__ "...\n");

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                switch (cc) {
                    case '1':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_X86_64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg/cli.c "
                            "../catpkg/main.c "
                            "../extra/x86_64/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o usr/bin/catpkg",
                            (char *)NULL
                        );        
                        break;

                    case '2':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_AARCH64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg/cli.c "
                            "../catpkg/main.c "
                            "../extra/aarch64/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o usr/bin/catpkg",
                            (char *)NULL
                        );       
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                perror("gcc");
                _exit(127);
            }

            int status;

            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid");
                return 1;
            }

            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {

                fprintf(stderr, "gcc: compilation failed\n");
                return 1;
            }

            pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                switch (cc) {
                    case '1':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_X86_64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg-command/main.c "
                            "../extra/x86_64/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o usr/bin/catpkg-command",
                            (char *)NULL
                        );        
                        break;

                    case '2':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_AARCH64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg-command/main.c "
                            "../extra/aarch64/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o usr/bin/catpkg-command",
                            (char *)NULL
                        );       
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                perror("gcc");
                _exit(127);
            }

            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid");
                return 1;
            }

            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {

                fprintf(stderr, "gcc: compilation failed\n");
                return 1;
            }

            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);

            switch (cc) {
                case '1':
                catpkg_build("x86_64");            
                    break;

                case '2':
                catpkg_build("aarch64");             
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }

            printf("Compilation successful.\n");

            break;
        }
        case CATPKG_BUILD_CATPACKAGE:
        { 
            chdir(".tmp");

            printf("Building \"catpkg-build-(aarch64/x86_64)@X.X.X.catpackage\"...\n");

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }   

            if (pid == 0) {
                switch (cc) {
                    case '1':   
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_X86_64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg-build/build.c "
                            "../catpkg-build/catpkg-build-cli.c "
                            "/usr/lib/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o extra/COMMAND_DATA/catpkg-build-cli",
                            (char *)NULL
                        );        
                        break;

                    case '2':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_AARCH64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../catpkg-build/build.c "
                            "../catpkg-build/catpkg-build-cli.c "
                            "/usr/lib/catstraplib.so.0 "
                            " -Wl,--gc-sections "
                            "-Wl,-rpath,/usr/lib "
                            "-o extra/COMMAND_DATA/catpkg-build-cli",
                            (char *)NULL
                        );       
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                perror("gcc");
                _exit(127);
            }

            int status;

            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid");
                return 1;
            }

            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {

                fprintf(stderr, "gcc: compilation failed\n");
                return 1;
            }

            setenv("CATSTRAP-BIN", "../build-data/catstrap-bin/catstrap-bin", 0);

            switch (cc) {
                case '1':
                catpkg_build("x86_64");            
                    break;

                case '2':
                catpkg_build("aarch64");             
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }

            printf("Compilation successful.\n");

            break;
        }
        case CATPKG_MAKE:
        {                
                bootstrap_update();

                printf("Building catpkg-make...\n");
                pid_t pid = fork();

                if (pid < 0) {
                    perror("fork");
                    return 1;
                }

                if (pid == 0) {
                    switch (cc) {
                        case '1':
                            execl(
                                "/usr/bin/" GCC_X86_64,
                                GCC_X86_64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "catpkg-build/build.c",
                                "src/make-cli.c",
                                "extra/x86_64/catstrap.o",
                                "-static",
                                "-o",
                                "catpkg-make-x86_64",
                                (char *)NULL
                            );                        
                            break;

                        case '2':
                            execl(
                                "/usr/bin/" GCC_AARCH64,
                                GCC_AARCH64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "catpkg-build/build.c",
                                "src/make-cli.c",
                                "extra/aarch64/catstrap.o",
                                "-static",
                                "-o",
                                "catpkg-make-aarch64",
                                (char *)NULL
                            );
                            break;

                        default:
                            printf("Invalid architecture.\n");
                            break;
                    }

                    perror("gcc");
                    _exit(127);
                }

                int status;

                if (waitpid(pid, &status, 0) < 0) {
                    perror("waitpid");
                    return 1;
                }

                if (!WIFEXITED(status) ||
                    WEXITSTATUS(status) != 0) {

                    fprintf(stderr, "gcc: compilation failed\n");
                    return 1;
                }

                printf("Compilation successful.\n");
            break;
        }
        default:
            printf("Unknown option.\n");
            break;
    }

    return 0;
}