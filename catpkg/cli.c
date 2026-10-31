#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <libgen.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"
#include "remove.h"
#include "database.h"
#include "cli.h"
#include "update.h"

#include "catpkg/pkginfo.h"
#include "catpkg/path.h"
#include "catpkg/command.h"

enum Command {
    COMMAND_UNKNOWN = 0,
    COMMAND_INSTALL,
    COMMAND_REMOVE,
    COMMAND_VERIFY,
    COMMAND_DATABASE,
    COMMAND_UPDATE,
    COMMAND_HELP,
    COMMAND_BY_PACKAGE
};

struct CatpkgBuiltinCommand {
    const char *declaration;
    const char *description;
};

static const struct CatpkgBuiltinCommand builtin_commands[] = {
    {
        "install <package>",
        "Install a package"
    },
    {
        "remove <package>",
        "Remove a package"
    },
    {
        "verify <package>",
        "Verify a package"
    },
    {
        "database <command>",
        "Manage package database"
    },
    {
        "update <package>",
        "Update a package"
    },
    {
        "help",
        "Show this help"
    }
};

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

static enum Command catpkg_parse_command(
    const char *command
)
{
    if (strcmp(command, "install") == 0)
        return COMMAND_INSTALL;

    if (strcmp(command, "remove") == 0)
        return COMMAND_REMOVE;

    if (strcmp(command, "verify") == 0)
        return COMMAND_VERIFY;

    if (strcmp(command, "database") == 0)
        return COMMAND_DATABASE;
    if (strcmp(command, "update") == 0)
        return COMMAND_UPDATE;
    if (strcmp(command, "help") == 0)
        return COMMAND_HELP;
    
    char *catpkg_command_path =
        make_catpkg_path(
            CATPKG_COMMANDS_DIR_PATH "/%s",
            command
        );

    if(access(catpkg_command_path, F_OK) == 0)
        return COMMAND_BY_PACKAGE;

    free(catpkg_command_path);

    return COMMAND_UNKNOWN;
}

void print_package_separator(
    const char *action,
    const char *package_fullname
)
{
    const int width = 60;

    char text[256];

    snprintf(
        text,
        sizeof(text),
        "  %s %s  ",
        action,
        package_fullname
    );

    int text_len = (int)strlen(text);

    if (text_len >= width) {
        printf("%s\n", text);
        return;
    }

    int remaining = width - text_len;

    int left = remaining / 2;
    int right = remaining - left;

    for (int i = 0; i < left; i++)
        putchar('-');

    fputs(text, stdout);

    for (int i = 0; i < right; i++)
        putchar('-');

    putchar('\n');
}

/*
 * Check whether the current process has
 * root privileges.
 */
static int catpkg_require_root(void)
{
    if (geteuid() == 0)
        return 0;

    fprintf(
        stderr,
        "catpkg: this command must be run as root.\n"
    );

    fprintf(
        stderr,
        "catpkg: please run it with sudo or as root.\n"
    );

    return 1;
}


int catpkg_cli(
    int argc,
    char **argv
)
{
    enum Command command;
    if (argc < 2)
        command = COMMAND_HELP;
    else
        command = catpkg_parse_command(argv[1]);

    switch (command) {

        case COMMAND_INSTALL:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg install <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_install(
                (const char**) argv + 2,
                argc - 2,
                false
            );       
 
        case COMMAND_UPDATE:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg update <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_update(
                (const char**) argv + 2,
                argc - 2
            );


        case COMMAND_REMOVE:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg remove <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_remove(
                (const char**) argv + 2,
                argc - 2,
                false
            );


        case COMMAND_VERIFY:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg verify <package>\n"
                );

                return 1;
            }


            return catpkg_verify(
                argv[2]
            );


        case COMMAND_DATABASE:

            /*
             * Database commands will be handled here.
             */

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg database <command>\n"
                );

                return 1;
            }


            if (strcmp(argv[2], "list") == 0) {
                struct PackageMatches packages =
                    catpkg_find_package("");


                for (
                    size_t p_i = 0;
                    p_i < packages.count;
                    p_i++
                ) {
                    struct PackageMatches versions =
                        catpkg_find_version(
                            packages.items[p_i]
                        );

                    char package_fullname[256];

                    snprintf(
                        package_fullname,
                        sizeof(package_fullname),
                        "%s@%s",
                        packages.items[p_i],
                        versions.items[0]
                    );

                    struct PackageInfo *info = catpkg_find_package_info(package_fullname);
                    const struct PackageField *description_field = PackageInfo_Find(info, "description");
                    struct PackageFieldMatches dependency_fields = PackageInfo_FindAll(info, "dependency");

                    print_package_separator("", package_fullname);

                    printf(
                        "Name: %s\n",
                        packages.items[p_i]
                    );
                    printf(
                        "Version:"
                    );


                    for (
                        size_t v_i = 0;
                        v_i < versions.count;
                        v_i++
                    ) {

                        printf(
                            " %s",
                            versions.items[v_i]
                        );
                    }


                    printf(
                        "\n"
                    );

                    printf("Description: ");
                    if(description_field != NULL)
                        printf("%s\n", description_field -> value);
                    else
                        printf("\n");

                    printf("Dependencies: ");
                    for (
                        size_t dp_i = 0;
                        dp_i < dependency_fields.count;
                        dp_i++
                    ) {
                        if(dependency_fields.items[dp_i] != NULL)
                            printf("%s ", dependency_fields.items[dp_i] -> value);
                    }
                    PackageFieldMatches_Free(&dependency_fields);

                    printf(
                        "\n"
                    );
                    printf(
                        "\n"
                    );

                    PackageMatches_Free(
                        &versions
                    );
                    PackageInfo_Free(
                        info
                    );
                }

                PackageMatches_Free(
                    &packages
                );
            }


            return 0;

        case COMMAND_HELP: 
        {
            printf("%s", CATPKG_HELP);

            DIR *dir =
                opendir(CATPKG_COMMANDS_DIR_PATH);

            if (dir != NULL) {
                struct dirent *entry;

                while ((entry = readdir(dir)) != NULL) {
                    if (
                        strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0
                    ) {
                        continue;
                    }

                    int fd4[2];
                    int fd5[2];

                    pipe(fd4);
                    pipe(fd5);

                    pid_t pid = fork();

                    if (pid == -1) {
                        perror("fork");

                        return 1;
                    }

                    if (pid == 0) {
                        dup2(fd4[1], 4);
                        dup2(fd5[1], 5);

                        char *command_path =
                            make_catpkg_path(
                                CATPKG_COMMANDS_DIR_PATH "/%s",
                                entry->d_name
                            );

                        char *command_realpath = realpath(command_path, NULL);

                        char *command_realpath_dir = strdup(command_realpath);

                        dirname(command_realpath_dir);

                        char *commands_data_dir =
                            make_catpkg_path(
                                "%s/%s",
                                command_realpath_dir,
                                "DATA"
                            );

                        execl(
                            "/usr/bin/catpkg-command",
                            "catpkg-command",
                            command_realpath,
                            "--commands_data_dir",
                            commands_data_dir,
                            "--declaration", "4",
                            "--description", "5",
                            (char *)NULL
                        );

                        free(command_path);
                        free(commands_data_dir);
                        free(command_realpath);

                        perror("execl");
                        _exit(127);
                    }
    
                    int status;

                    if (waitpid(pid, &status, 0) == -1) {
                        perror("waitpid");
                        
                        close(fd4[1]);
                        close(fd5[1]);

                        return 1;
                    }

                    close(fd4[1]);
                    close(fd5[1]);

                    char command_declaration[4096];
                    char command_description[4096];

                    if(read(fd4[0], command_declaration, sizeof(command_declaration)) < 0) {
                        fprintf(
                            stderr,
                            "Error: failed to read name.\n"
                        );

                        close(fd4[0]);
                        close(fd5[0]);
                        
                        return 1;
                    }

                    close(fd4[0]);

                    if(read(fd5[0], command_description, sizeof(command_description)) < 0) {
                        fprintf(
                            stderr,
                            "Error: failed to read name.\n"
                        );
                        close(fd5[0]);
                        
                        return 1;
                    }             

                    close(fd5[0]);

                    /*
                     * Metadata is ready.
                     */

                    printf(
                        "  %-22s %s\n",
                        command_declaration,
                        command_description
                    );
                }

                closedir(dir);
            }

            size_t count =
                sizeof(builtin_commands) /
                sizeof(builtin_commands[0]);

            for (size_t i = 0; i < count; i++) {
                printf(
                    "  %-22s %s\n",
                    builtin_commands[i].declaration,
                    builtin_commands[i].description
                );
            }



            return 0;
        }

        case COMMAND_BY_PACKAGE:
        {
            pid_t pid = fork();

            if (pid == -1) {
                perror("fork");
                return 1;
            }

            /*
             * --------------------------------------------------------
             * Child
             * --------------------------------------------------------
             */

            if (pid == 0) {
                char *command_path =
                    make_catpkg_path(
                        CATPKG_COMMANDS_DIR_PATH "/%s",
                        argv[1]
                    );

                char *command_realpath = realpath(command_path, NULL);
                char *command_realpath_dir = strdup(command_realpath);

                dirname(command_realpath_dir);

                char *commands_data_dir =
                    make_catpkg_path(
                        "%s/%s",
                        command_realpath_dir,
                        "DATA"
                    );

                if (
                    command_path == NULL ||
                    commands_data_dir == NULL ||
                    command_realpath == NULL ||
                    command_realpath_dir == NULL
                ) {
                    free(command_path);
                    free(commands_data_dir);
                    free(command_realpath_dir);
                    free(command_realpath);
                    _exit(1);
                }

                size_t command_argc =
                    argc > 2
                        ? (size_t)(argc - 2)
                        : 0;

                /*
                 * catpkg-command
                 * command_realpath
                 * --commands_data_dir
                 * commands_data_dir
                 * --execute
                 * 1
                 * [user args...]
                 * NULL
                 */
                char **catpkg_command_argv =
                    malloc(
                        (
                            6 +
                            command_argc +
                            1
                        ) * sizeof(*catpkg_command_argv)
                    );

                if (catpkg_command_argv == NULL) {
                    free(command_path);
                    free(command_realpath);
                    free(command_realpath_dir);
                    free(commands_data_dir);
                    _exit(1);
                }

                size_t i = 0;

                catpkg_command_argv[i++] =
                    "catpkg-command";

                catpkg_command_argv[i++] =
                    command_realpath;

                catpkg_command_argv[i++] =
                    "--commands_data_dir";

                catpkg_command_argv[i++] =
                    commands_data_dir;

                catpkg_command_argv[i++] =
                    "--execute";

                catpkg_command_argv[i++] =
                    "1";

                for (int j = 2; j < argc; j++)
                    catpkg_command_argv[i++] = argv[j];

                catpkg_command_argv[i] = NULL;

                execv(
                    "/usr/bin/catpkg-command",
                    catpkg_command_argv
                );

                /*
                 * execv() failed.
                 */

                perror("execv");

                free(catpkg_command_argv);
                free(command_realpath);
                free(command_realpath_dir);
                free(commands_data_dir);
                free(command_path);

                _exit(127);
            }

            /*
             * --------------------------------------------------------
             * Parent
             * --------------------------------------------------------
             */

            int status;

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) == -1
            ) {
                perror("waitpid");
                return 1;
            }

            if (WIFEXITED(status))
                return WEXITSTATUS(status);

            if (WIFSIGNALED(status)) {
                fprintf(
                    stderr,
                    "catpkg: command terminated by signal %d\n",
                    WTERMSIG(status)
                );

                return 128 + WTERMSIG(status);
            }

            return 1;
        }

        case COMMAND_UNKNOWN:
        default:

            fprintf(
                stderr,
                "catpkg: unknown command \"%s\"\nTry \"catpkg help\"\n",
                argv[1]
            );

            return 1;
    }
}