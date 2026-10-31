#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>

#include "utils/postprocess.h"

#define COMMAND_DIRECTIVE_MAX 256
#define COMMAND_BUFFER_SIZE 4096

#define COMMAND_PATH_INDEX 1
#define COMMAND_OPERATOR_INDEX 4
#define COMMAND_ARGS_INDEX (COMMAND_OPERATOR_INDEX + 2)

#define POSTPROCESS_CHUNK_SIZE 1024

extern char **environ;

struct PostprocessReader {
    int input_fd;
    int pipe_read;
    int pipe_write;

    ssize_t available;
    int eof;

    struct MacroDeclaration *macros;
    size_t macros_size;
};

static int write_all(
    int fd,
    const void *buffer,
    size_t size
)
{
    const char *ptr = buffer;

    while (size > 0) {
        ssize_t written =
            write(fd, ptr, size);

        if (written < 0)
            return 1;

        ptr += written;
        size -= (size_t)written;
    }

    return 0;
}

static int postprocess_reader_fill(
    struct PostprocessReader *reader
)
{
    if (reader->eof)
        return 0;

    ssize_t produced =
        postproccess_format_file_macros(
            reader->input_fd,
            reader->pipe_write,
            reader->macros,
            reader->macros_size,
            POSTPROCESS_CHUNK_SIZE
        );

    if (produced < 0)
        return -1;

    if (produced == 0) {
        reader->eof = 1;
        return 0;
    }

    reader->available = produced;

    return 1;
}

static int postprocess_reader_read(
    struct PostprocessReader *reader,
    void *buffer,
    size_t size
)
{
    char *ptr = buffer;
    size_t total = 0;

    while (total < size) {
        if (reader->available == 0) {
            int result =
                postprocess_reader_fill(reader);

            if (result < 0)
                return -1;

            if (result == 0)
                break;
        }

        size_t count = size - total;

        if (
            count >
            (size_t)reader->available
        ) {
            count =
                (size_t)reader->available;
        }

        ssize_t readed =
            read(
                reader->pipe_read,
                ptr + total,
                count
            );

        if (readed < 0)
            return -1;

        if (readed == 0)
            return -1;

        reader->available -= readed;
        total += (size_t)readed;
    }

    return (int)total;
}

static int postprocess_reader_char(
    struct PostprocessReader *reader,
    char *c
)
{
    return postprocess_reader_read(
        reader,
        c,
        1
    );
}

static int read_directive(
    struct PostprocessReader *reader,
    char *directive,
    size_t size
)
{
    size_t pos = 0;
    char c;

    while (pos + 1 < size) {
        int readed =
            postprocess_reader_char(
                reader,
                &c
            );

        if (readed < 0)
            return -1;

        if (readed == 0 || c == '\n')
            break;

        directive[pos++] = c;
    }

    directive[pos] = '\0';

    return 0;
}

static int copy_to_memfd(
    struct PostprocessReader *reader,
    int memfd
)
{
    char buffer[COMMAND_BUFFER_SIZE];

    for (;;) {
        int readed =
            postprocess_reader_read(
                reader,
                buffer,
                sizeof(buffer)
            );

        if (readed < 0)
            return 1;

        if (readed == 0)
            break;

        if (
            write_all(
                memfd,
                buffer,
                (size_t)readed
            ) != 0
        ) {
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc <= COMMAND_OPERATOR_INDEX)
        return 1;

    char *command_path =
        argv[COMMAND_PATH_INDEX];

    char *commands_data_dir =
        NULL;

    for (
        size_t i = 2;
        i < COMMAND_OPERATOR_INDEX;
        i++
    ) {
        if (
            strcmp(
                argv[i],
                "--commands_data_dir"
            ) == 0
        ) {
            if (
                i + 1 >=
                COMMAND_OPERATOR_INDEX
            )
                return 1;

            commands_data_dir =
                argv[++i];
        }
    }

    struct {
        const char *option;
        const char *section;
        int requested;
        int found;
        int output_fd;
    } sections[] = {
        {
            "--name",
            "name",
            0,
            0,
            -1
        },
        {
            "--declaration",
            "declaration",
            0,
            0,
            -1
        },
        {
            "--description",
            "description",
            0,
            0,
            -1
        },
        {
            "--execute",
            "execute",
            0,
            0,
            -1
        }
    };

    size_t sections_count =
        sizeof(sections) /
        sizeof(sections[0]);

    size_t requested_count = 0;

    for (
        size_t i = COMMAND_OPERATOR_INDEX;
        i < (size_t)argc;
        i++
    ) {
        int found = 0;

        for (
            size_t j = 0;
            j < sections_count;
            j++
        ) {
            if (
                strcmp(
                    argv[i],
                    sections[j].option
                ) == 0
            ) {
                if (
                    i + 1 >=
                    (size_t)argc
                )
                    return 1;

                char *endptr = NULL;

                long output_fd =
                    strtol(
                        argv[++i],
                        &endptr,
                        10
                    );

                if (
                    *argv[i] == '\0' ||
                    *endptr != '\0' ||
                    output_fd < 0 ||
                    output_fd > INT_MAX
                )
                    return 1;

                if (!sections[j].requested) {
                    sections[j].requested = 1;
                    requested_count++;
                }

                sections[j].output_fd =
                    (int)output_fd;

                found = 1;
                break;
            }
        }

        if (!found)
            break;
    }

    if (requested_count == 0)
        return 1;

    int fd =
        open(
            command_path,
            O_RDONLY
        );

    if (fd < 0)
        return 1;

    #define __COMMANDS_DATA_DIR__ commands_data_dir
    
    struct MacroDeclaration macros[] = {
        #define MACRO(name, value, is_string) { name, (intptr_t) value, is_string },

        #include "../common/postprocess_macros/commands.inc"

        #undef MACRO
    };
    
    #undef __COMMANDS_DATA_DIR__

    int p[2];

    if (pipe(p) < 0) {
        close(fd);
        return 1;
    }

    struct PostprocessReader reader = {
        .input_fd = fd,
        .pipe_read = p[0],
        .pipe_write = p[1],
        .available = 0,
        .eof = 0,
        .macros = macros,
        .macros_size = 1
    };

    char c;

    char directive[
        COMMAND_DIRECTIVE_MAX
    ];

    char pending_directive[
        COMMAND_DIRECTIVE_MAX
    ];

    int has_pending_directive = 0;

    unsigned char match = 1;
    size_t completed = 0;

    for (;;) {
        if (!has_pending_directive) {
            for (;;) {
                int readed =
                    postprocess_reader_char(
                        &reader,
                        &c
                    );

                if (readed < 0) {
                    close(fd);
                    close(p[0]);
                    close(p[1]);
                    return 1;
                }

                if (readed == 0)
                    goto done;

                if (
                    (match == 0 && c == '\n') ||
                    (match == 1 && c == '#') ||
                    (match == 2 && c == '@')
                ) {
                    match++;
                }
                else {
                    match =
                        c == '\n'
                            ? 1
                            : 0;
                }

                if (match == 3)
                    break;
            }

            if (
                read_directive(
                    &reader,
                    directive,
                    sizeof(directive)
                ) != 0
            ) {
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }
        }
        else {
            memcpy(
                directive,
                pending_directive,
                sizeof(directive)
            );

            has_pending_directive = 0;
        }

        match = 1;

        char *name =
            strtok(
                directive,
                " \t"
            );

        char *interpreter =
            strtok(
                NULL,
                " \t"
            );

        if (name == NULL)
            continue;

        size_t section_index =
            sections_count;

        for (
            size_t i = 0;
            i < sections_count;
            i++
        ) {
            if (
                sections[i].requested &&
                !sections[i].found &&
                strcmp(
                    name,
                    sections[i].section
                ) == 0
            ) {
                section_index = i;
                break;
            }
        }

        int memfd =
            -1;

        if (section_index != sections_count) {
            memfd =
                memfd_create(
                    "catpkg-command",
                    0
                );

            if (memfd < 0) {
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }
        }

        unsigned char section_match = 1;

        for (;;) {
            int readed =
                postprocess_reader_char(
                    &reader,
                    &c
                );

            if (readed < 0) {
                if (memfd >= 0)
                    close(memfd);

                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            if (readed == 0)
                break;

            if (section_match == 1) {
                if (c == '#') {
                    section_match = 2;
                    continue;
                }

                if (memfd >= 0) {
                    if (
                        write_all(
                            memfd,
                            &c,
                            1
                        ) != 0
                    ) {
                        close(memfd);
                        close(fd);
                        close(p[0]);
                        close(p[1]);
                        return 1;
                    }
                }

                section_match =
                    c == '\n'
                        ? 1
                        : 0;

                continue;
            }

            if (section_match == 2) {
                if (c == '@') {
                    if (
                        read_directive(
                            &reader,
                            pending_directive,
                            sizeof(pending_directive)
                        ) != 0
                    ) {
                        if (memfd >= 0)
                            close(memfd);

                        close(fd);
                        close(p[0]);
                        close(p[1]);
                        return 1;
                    }

                    has_pending_directive = 1;
                    break;
                }

                if (memfd >= 0) {
                    if (
                        write_all(
                            memfd,
                            "#",
                            1
                        ) != 0
                    ) {
                        close(memfd);
                        close(fd);
                        close(p[0]);
                        close(p[1]);
                        return 1;
                    }

                    if (
                        write_all(
                            memfd,
                            &c,
                            1
                        ) != 0
                    ) {
                        close(memfd);
                        close(fd);
                        close(p[0]);
                        close(p[1]);
                        return 1;
                    }
                }

                section_match =
                    c == '\n'
                        ? 1
                        : 0;

                continue;
            }

            if (memfd >= 0) {
                if (
                    write_all(
                        memfd,
                        &c,
                        1
                    ) != 0
                ) {
                    close(memfd);
                    close(fd);
                    close(p[0]);
                    close(p[1]);
                    return 1;
                }
            }

            section_match =
                c == '\n'
                    ? 1
                    : 0;
        }

        if (section_index == sections_count)
            continue;

        sections[section_index].found = 1;

        if (
            lseek(
                memfd,
                0,
                SEEK_SET
            ) < 0
        ) {
            close(memfd);
            close(fd);
            close(p[0]);
            close(p[1]);
            return 1;
        }

        if (
            interpreter != NULL &&
            strcmp(
                interpreter,
                "default"
            ) == 0
        ) {
            off_t size =
                lseek(
                    memfd,
                    0,
                    SEEK_END
                );

            if (size < 0) {
                close(memfd);
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            /*
             * Exclude the final newline.
             */
            if (size > 0) {
                char last;

                if (
                    lseek(
                        memfd,
                        size - 1,
                        SEEK_SET
                    ) < 0 ||
                    read(
                        memfd,
                        &last,
                        1
                    ) != 1
                ) {
                    close(memfd);
                    close(fd);
                    close(p[0]);
                    close(p[1]);
                    return 1;
                }

                if (last == '\n')
                    size--;
            }

            if (
                lseek(
                    memfd,
                    0,
                    SEEK_SET
                ) < 0
            ) {
                close(memfd);
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            char buffer[COMMAND_BUFFER_SIZE];
            off_t remaining = size;

            while (remaining > 0) {
                size_t to_read =
                    remaining < (off_t)sizeof(buffer)
                        ? (size_t)remaining
                        : sizeof(buffer);

                ssize_t readed =
                    read(
                        memfd,
                        buffer,
                        to_read
                    );

                if (readed <= 0) {
                    close(memfd);
                    close(fd);
                    close(p[0]);
                    close(p[1]);
                    return 1;
                }

                if (
                    write_all(
                        sections[section_index].output_fd,
                        buffer,
                        (size_t)readed
                    ) != 0
                ) {
                    close(memfd);
                    close(fd);
                    close(p[0]);
                    close(p[1]);
                    return 1;
                }

                remaining -= readed;
            }

            if (
                write_all(
                    sections[section_index].output_fd,
                    "\0",
                    1
                ) != 0
            ) {
                close(memfd);
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            close(memfd);

            completed++;

            if (completed == requested_count) {
                close(fd);
                close(p[0]);
                close(p[1]);
                return 0;
            }

            continue;
        }

        size_t user_argc =
            argc > COMMAND_ARGS_INDEX
                ? (size_t)(
                    argc -
                    COMMAND_ARGS_INDEX
                )
                : 0;

        if (
            interpreter != NULL &&
            strcmp(
                interpreter,
                "sh"
            ) == 0
        ) {
            char memfd_path[64];

            snprintf(
                memfd_path,
                sizeof(memfd_path),
                "/proc/self/fd/%d",
                memfd
            );

            char **exec_argv =
                calloc(
                    user_argc + 3,
                    sizeof(*exec_argv)
                );

            if (exec_argv == NULL) {
                close(memfd);
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            exec_argv[0] =
                "sh";

            exec_argv[1] =
                memfd_path;

            for (
                size_t i = 0;
                i < user_argc;
                i++
            ) {
                exec_argv[i + 2] =
                    argv[
                        COMMAND_ARGS_INDEX +
                        i
                    ];
            }

            exec_argv[
                user_argc + 2
            ] = NULL;

            close(fd);
            close(p[0]);
            close(p[1]);

            execv(
                "/usr/bin/sh",
                exec_argv
            );

            free(exec_argv);
            close(memfd);
            return 1;
        }

        if (interpreter == NULL) {
            char **exec_argv =
                calloc(
                    user_argc + 2,
                    sizeof(*exec_argv)
                );

            if (exec_argv == NULL) {
                close(memfd);
                close(fd);
                close(p[0]);
                close(p[1]);
                return 1;
            }

            exec_argv[0] =
                command_path;

            for (
                size_t i = 0;
                i < user_argc;
                i++
            ) {
                exec_argv[i + 1] =
                    argv[
                        COMMAND_ARGS_INDEX +
                        i
                    ];
            }

            exec_argv[
                user_argc + 1
            ] = NULL;

            close(fd);
            close(p[0]);
            close(p[1]);

            fexecve(
                memfd,
                exec_argv,
                environ
            );

            free(exec_argv);
            close(memfd);
            return 1;
        }

        close(memfd);
        close(fd);
        close(p[0]);
        close(p[1]);
        return 1;
    }

done:
    close(fd);
    close(p[0]);
    close(p[1]);

    return completed == requested_count
        ? 0
        : 1;
}