#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>  
#include <stdint.h>

#include "catpkg/pkginfo.h"
#include "catpkg/path.h"
#include "configuration.h"

static struct PackageReader *package_reader_init(
    int fd_r,
    const char *base_path,
    struct PackageReader *reuse
)
{
    if (reuse == NULL) {
        reuse = malloc(sizeof(*reuse));

        if (reuse == NULL) {
            perror("catpkg: malloc");
            return NULL;
        }

        reuse->fd_r = fd_r;
        reuse->fd_bf_w = -1;
        reuse->fd_bf_r = -1;
        reuse->fd_main_w = -1;
        reuse->fd_main_r = fd_r;
        reuse->bytepos_buffer_offset = 0;
        reuse->bytepos = 0;
        reuse->base_path = base_path;

        return reuse;
    }

    size_t remaining = reuse->bytepos;
    char buffer[4096];

    while (remaining > 0) {
        size_t chunk =
            remaining < sizeof(buffer)
                ? remaining
                : sizeof(buffer);

        ssize_t n = read(fd_r, buffer, chunk);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return NULL;
        }

        if (n == 0)
            return NULL;

        remaining -= (size_t)n;
    }

    if (reuse->fd_main_r != reuse->fd_bf_r)
        reuse->fd_main_r = fd_r;

    reuse->fd_r = fd_r;
    reuse->base_path = base_path;

    return reuse;
}

static char *package_reader_getline(
    struct PackageReader *reader,
    char *buffer,
    size_t buffer_size
)
{
    if (
        reader == NULL ||
        buffer == NULL ||
        buffer_size == 0
    ) {
        return NULL;
    }

    size_t i = 0;

    while (i < buffer_size - 1) {
        char c;

        ssize_t result = read(
            reader->fd_main_r,
            &c,
            1
        );

        /*
         * Character successfully read.
         */
        if (result == 1) {
            buffer[i++] = c;

            /*
             * Copy data to the rewind buffer
             * when buffering is active.
             */
            if (reader->fd_bf_w >= 0) {
                ssize_t write_result = write(
                    reader->fd_bf_w,
                    &c,
                    1
                );

                if (write_result != 1) {
                    return NULL;
                }

                reader->bytepos_buffer_offset++;
            }

            reader->bytepos++;

            if (c == '\n')
                break;

            continue;
        }

        /*
         * read() error.
         */
        if (result == (ssize_t) SIZE_MAX) {
            if (errno == EINTR)
                continue;

            return NULL;
        }

        /*
         * EOF of the rewind buffer.
         *
         * Continue reading from the original source.
         * Nothing was written to buffer[], therefore
         * `i` must remain unchanged.
         */
        if (reader->fd_main_r == reader->fd_bf_r) {
            close(reader->fd_bf_r);

            reader->fd_bf_r = -1;
            reader->fd_main_r = reader->fd_r;
            reader->fd_main_w = -1;

            continue;
        }

        /*
         * EOF of the original source.
         */
        if (i == 0)
            return NULL;

        break;
    }

    buffer[i] = '\0';

    char *comment = strstr(buffer, "// ");

    if (comment != NULL)
        *comment = '\0';

    return buffer;
}

static void package_reader_close(
    struct PackageReader *reader
)
{
    if (reader == NULL)
        return;

    if (reader->fd_bf_w >= 0)
        close(reader->fd_bf_w);

    if (reader->fd_bf_r >= 0)
        close(reader->fd_bf_r);

    /*
     * fd_r is borrowed.
     * Its owner closes it.
     */
    reader->fd_bf_w = -1;
    reader->fd_bf_r = -1;

    reader->fd_main_w = -1;
    reader->fd_main_r = -1;

    reader->fd_r = -1;
}

static int package_reader_buffer_write_start(
    struct PackageReader *reader
) {
    int p[2];

    if (pipe(p) < 0) {
        perror("pipe");
        return 1;
    }

    reader->fd_bf_r = p[0];
    reader->fd_bf_w = p[1];

    return 0;
}

static int package_reader_buffer_flush(
    struct PackageReader *reader
) {
    close(reader -> fd_bf_w);
    reader -> fd_bf_w = -1;

    reader -> fd_main_r = reader -> fd_bf_r;
    reader -> fd_main_w = reader -> fd_bf_w;


    reader->bytepos -= reader->bytepos_buffer_offset;
    reader->bytepos_buffer_offset = 0;
    return 0;
}

static char *package_reader_read_section(
    struct PackageReader *reader
)
{
    if (reader == NULL)
        return NULL;

    char buffer[PATH_MAX];

    char *content = NULL;
    size_t content_size = 0;

    package_reader_buffer_write_start(reader);

    while (
        package_reader_getline(
            reader,
            buffer,
            sizeof(buffer)
        ) != NULL
    ) {
        /*
         * "-" belongs to the separator,
         * not to section content.
         */

        char *line_end = strpbrk(
            buffer,
            "\r\n"
        );

        size_t line_length =
            line_end != NULL
                ? (size_t)(line_end - buffer)
                : strlen(buffer);

        if (
            line_length == 1 &&
            buffer[0] == '-'
        ) {
            break;
        }

        size_t length = strlen(buffer);

        char *tmp = realloc(
            content,
            content_size + length + 1
        );
        content = tmp;

        memcpy(
            content + content_size,
            buffer,
            length
        );

        content_size += length;
        content[content_size] = '\0';
    }


    package_reader_buffer_flush(reader);

    /*
     * Empty section.
     */

    if (content == NULL) {
        content = strdup("");

        if (content == NULL)
            return NULL;
    }

    return content;
}

/*
 * CATPKG 1.0 — PackageInfo
 */

static size_t catpkg_readline_v1_packageinfo(
    struct PackageReader *reader,
    char *buffer,
    struct PackageInfo *info,
    struct PackageField **last_field,
    size_t *current_section,
    char **section,
    size_t *section_size,
    long *section_bytepos,
    long field_bytepos
)
{
    size_t fields_before =
        info->fields_count;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    /*
     * Helper for adding a field.
     */

    #define ADD_FIELD(field_name, field_value, bytepos_)       \
        do {                                                   \
            struct PackageField *field = malloc(              \
                sizeof(*field)                                 \
            );                                                 \
                                                               \
            if (field == NULL)                                 \
                return -1;                                     \
                                                               \
            field->name = strdup(field_name);                  \
            field->value = strdup(field_value);                \
            field->bytepos = bytepos_;                         \
            field->section = *current_section;                 \
            field->next_field = NULL;                          \
                                                               \
            if (field->name == NULL ||                         \
                field->value == NULL) {                        \
                free(field->name);                             \
                free(field->value);                            \
                free(field);                                   \
                return -1;                                     \
            }                                                  \
                                                               \
            if (info->fields == NULL)                          \
                info->fields = field;                          \
            else                                               \
                (*last_field)->next_field = field;             \
                                                               \
            *last_field = field;                               \
            info->fields_count++;                              \
        } while (0)

        #define ADD_FIELD_DEPENDENCY(field_name, field_value, bytepos_)                                 \
            do {                                                                                        \
                ADD_FIELD(field_name, field_value, bytepos_);                                           \
                                                                                                        \
                struct PackageMatches versions =                                                        \
                    catpkg_find_version(                                                                \
                        field_value                                                                     \
                    );                                                                                  \
                                                                                                        \
                if (                                                                                    \
                    versions.items == NULL ||                                                           \
                    versions.count == 0 ||                                                              \
                    versions.items[0] == NULL                                                           \
                ) {                                                                                     \
                    break;                                                                              \
                }                                                                                       \
                                                                                                        \
                char package_fullname[256];                                                             \
                                                                                                        \
                snprintf(                                                                               \
                    package_fullname,                                                                   \
                    sizeof(package_fullname),                                                           \
                    "%s@%s",                                                                            \
                    field_value,                                                                        \
                    versions.items[0]                                                                   \
                );                                                                                      \
                                                                                                        \
                struct PackageInfo *dep_info =                                                          \
                    catpkg_find_package_info(                                                           \
                        package_fullname                                                                \
                    );                                                                                  \
                                                                                                        \
                if (dep_info == NULL)                                                                   \
                    break;                                                                              \
                                                                                                        \
                if (CATPKG_ReadSection(dep_info, 5) == (ssize_t) SIZE_MAX) {                            \
                    PackageInfo_Free(dep_info);                                                         \
                    break;                                                                              \
                }                                                                                       \
                                                                                                        \
                if (dep_info->fields != NULL) {                                                         \
                    if (*last_field != NULL)                                                            \
                        (*last_field)->next_field = dep_info->fields;                                   \
                    else                                                                                \
                        info->fields = dep_info->fields;                                                \
                                                                                                        \
                    info->fields_count +=                                                               \
                        dep_info->fields_count;                                                         \
                                                                                                        \
                    *last_field =                                                                       \
                        dep_info->fields;                                                               \
                                                                                                        \
                    while ((*last_field)->next_field != NULL)                                           \
                        *last_field = (*last_field)->next_field;                                        \
                                                                                                        \
                    dep_info->fields = NULL;                                                            \
                    dep_info->fields_count = 0;                                                         \
                }                                                                                       \
                                                                                                        \
                PackageInfo_Free(dep_info);                                                             \
            } while (0)
    switch (*current_section) {
        #include "pkginfo/SHEME/SHEME_PACKAGE_INFO_READER.inc"
    }

    #undef ADD_FIELD
    #undef ADD_FIELD_DEPENDENCY

    return (
        info->fields_count -
        fields_before
    );
}

static size_t catpkg_skipsection_v1_packageinfo(
    struct PackageReader *reader,
    char *buffer,
    struct PackageInfo *info,
    struct PackageField **last_field,
    size_t *current_section,
    char **section,
    size_t *section_size,
    long *section_bytepos,
    long field_bytepos
)
{
    size_t sections_before =
        *current_section;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    #define ADD_FIELD(field_name, field_value, bytepos_)
    #define ADD_FIELD_DEPENDENCY(field_name, field_value, bytepos_)
    
    switch (*current_section) {
        #include "pkginfo/SHEME/SHEME_PACKAGE_INFO_READER.inc"
    }

    #undef ADD_FIELD
    #undef ADD_FIELD_DEPENDENCY

    return (int)(
        *current_section -
        sections_before
    );
}


/*
 * CATPKG 1.1 — FileInfo
 */

static int catpkg_readline_v1_fileinfo(
    struct PackageReader *reader,
    char *buffer,
    struct PackageInfo *info,
    struct PackageField **last_field,
    size_t *current_section,
    char **section,
    size_t *section_size,
    long *section_bytepos,
    long field_bytepos
)
{
    size_t fields_before =
        info->fields_count;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    /*
     * Helper for adding a field.
     */

    #define ADD_FIELD(field_name, field_value, bytepos_)       \
        do {                                                   \
            struct PackageField *field = malloc(              \
                sizeof(*field)                                 \
            );                                                 \
                                                               \
            if (field == NULL)                                 \
                return -1;                                     \
                                                               \
            field->name = strdup(field_name);                  \
            field->value = strdup(field_value);                \
            field->bytepos = bytepos_;                         \
            field->section = *current_section;                 \
            field->next_field = NULL;                          \
                                                               \
            if (field->name == NULL ||                         \
                field->value == NULL) {                        \
                free(field->name);                             \
                free(field->value);                            \
                free(field);                                   \
                return -1;                                     \
            }                                                  \
                                                               \
            if (info->fields == NULL)                          \
                info->fields = field;                          \
            else                                               \
                (*last_field)->next_field = field;             \
                                                               \
            *last_field = field;                               \
            info->fields_count++;                              \
        } while (0)

    switch (*current_section) {
        #include "pkginfo/SHEME/SHEME_FILE_INFO_READER.inc"
    }

    #undef ADD_FIELD

    return (
        info->fields_count -
        fields_before
    );
}

static int catpkg_skipsection_v1_fileinfo(
    struct PackageReader *reader,
    char *buffer,
    struct PackageInfo *info,
    struct PackageField **last_field,
    size_t *current_section,
    char **section,
    size_t *section_size,
    long *section_bytepos,
    long field_bytepos
)
{
    size_t sections_before =
        *current_section;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    #define ADD_FIELD(field_name, field_value, bytepos_)

    switch (*current_section) {
        #include "pkginfo/SHEME/SHEME_FILE_INFO_READER.inc"
    }

    #undef ADD_FIELD

    return (int)(
        *current_section -
        sections_before
    );
}

const struct PackageField *PackageInfo_Find(
    const struct PackageInfo *info,
    const char *name
)
{
    if (info == NULL || name == NULL)
        return NULL;

    const struct PackageField *field = info->fields;
    

    while (field != NULL) {
        if (
            field->name != NULL &&
            strcmp(field->name, name) == 0
        ) {
            return field;
        }

        field = field->next_field;
    }

    return NULL;
}
struct PackageFieldMatches PackageInfo_FindAll(
    const struct PackageInfo *info,
    const char *name
)
{
    struct PackageFieldMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (info == NULL || name == NULL)
        return matches;

    const struct PackageField *field = info->fields;

    while (field != NULL) {

        if (
            field->name != NULL &&
            strcmp(field->name, name) == 0
        ) {
            const struct PackageField **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                PackageFieldMatches_Free(&matches);
                return matches;
            }

            matches.items = tmp;

            matches.items[matches.count] = field;
            matches.count++;
        }

        field = field->next_field;
    }

    return matches;
}

struct PackageInfo *CATPKG_FD_Parse(
    int fd_r,
    const char *base_path,
    struct PackageInfo *reuse
)
{
    if (fd_r < 0)
        return NULL;


    /*
     * Allocate PackageInfo.
     */

    struct PackageInfo *info = reuse;

    if(info == NULL)
        info = calloc(
            1,
            sizeof(*info)
        );

    if (info == NULL) {
        perror("catpkg: ");
        return NULL;
    }

    /*
     * Allocate PackageReader.
     */

    struct PackageReader *reader = info -> reader;
    
    reader = package_reader_init(
        fd_r,
        base_path,
        reader
    );
    
    info->reader = reader;

    if(reuse == NULL) {


        /*
         * Read CATPKG format.
         */

        char format[256];
        
        if (
            package_reader_getline(
                reader,
                format,
                sizeof(format)
            ) == NULL
        ) {
            fprintf(
                stderr,
                "catpkg: empty PACKAGEINFO\n"
            );

            goto error;
        }

        format[
            strcspn(format, "\r\n")
        ] = '\0';

        /*reader
         * CATPKG 1.0 — PackageInfo
         */

        if (
            strcmp(
                format,
                "CATPKG 1.0"
            ) == 0
        ) {
            info->patch = 0;
        }

        /*
         * CATPKG 1.1 — FileInfo
         */

        else if (
            strcmp(
                format,
                "CATPKG 1.1"
            ) == 0
        ) {
            info->patch = 1;
        }


        /*
         * CATPKG 2.0
         */

        else if (
            strcmp(
                format,
                "CATPKG 2.0"
            ) == 0
        ) {
            fprintf(
                stderr,
                "catpkg: CATPKG 2.0 is not supported yet\n"
            );

            goto error;
        }


        /*
         * Unknown format.
         */

        else {
            fprintf(
                stderr,
                "catpkg: unsupported PACKAGEINFO format: %s\n",
                format
            );

            goto error;
        }
    }

    return info;


error:
    PackageInfo_Free(info);

    return NULL;
}

struct PackageInfo *CATPKG_Parse(
    const char *file_path,
    struct PackageInfo *reuse
)
{
    if (file_path == NULL)
        return NULL;

    int fd_r =
        open(
            file_path,
            O_RDONLY
        );

    if (fd_r == -1) {
        perror(
            "catpkg: failed to open PACKAGEINFO"
        );

        return NULL;
    }

    struct header_posix_ustar hpu;

    ssize_t header_size =
        read(
            fd_r,
            &hpu,
            sizeof(hpu)
        );

    if (
        header_size == sizeof(hpu) &&
        memcmp(
            hpu.magic,
            "ustar",
            5
        ) == 0
    ) {
        if (
            lseek(
                fd_r,
                0,
                SEEK_SET
            ) == (off_t)-1
        ) {
            perror("lseek");
            close(fd_r);
            return NULL;
        }

        int packageinfo_fd = -1;

        while (
            read(
                fd_r,
                &hpu,
                sizeof(hpu)
            ) == sizeof(hpu)
        ) {
            if (
                memcmp(
                    hpu.magic,
                    "ustar",
                    5
                ) != 0
            ) {
                break;
            }

            unsigned long long file_size =
                strtoull(
                    hpu.size,
                    NULL,
                    8
                );

            size_t name_len =
                strnlen(
                    hpu.name,
                    sizeof(hpu.name)
                );

            if (
                name_len ==
                    sizeof(CATPKG_PACKAGEINFO) - 1 &&
                memcmp(
                    hpu.name,
                    CATPKG_PACKAGEINFO,
                    sizeof(CATPKG_PACKAGEINFO) - 1
                ) == 0
            ) {
                packageinfo_fd =
                    memfd_create(
                        "PACKAGEINFO",
                        0
                    );

                if (packageinfo_fd == -1) {
                    perror("memfd_create");
                    close(fd_r);
                    return NULL;
                }

                size_t remaining =
                    (size_t)file_size;

                while (remaining > 0) {
                    ssize_t n =
                        sendfile(
                            packageinfo_fd,
                            fd_r,
                            NULL,
                            remaining
                        );

                    if (n == -1) {
                        perror("sendfile");
                        close(packageinfo_fd);
                        close(fd_r);
                        return NULL;
                    }

                    if (n == 0) {
                        close(packageinfo_fd);
                        close(fd_r);
                        return NULL;
                    }

                    remaining -=
                        (size_t)n;
                }

                if (
                    lseek(
                        packageinfo_fd,
                        0,
                        SEEK_SET
                    ) == (off_t)-1
                ) {
                    perror("lseek");
                    close(packageinfo_fd);
                    close(fd_r);
                    return NULL;
                }

                break;
            }

            off_t skip =
                (off_t)(
                    ((file_size + 511) / 512) *
                    512
                );

            if (
                lseek(
                    fd_r,
                    skip,
                    SEEK_CUR
                ) == (off_t)-1
            ) {
                perror("lseek");
                close(fd_r);
                return NULL;
            }
        }

        close(fd_r);

        if (packageinfo_fd == -1) {
            fprintf(
                stderr,
                "catpkg: PACKAGEINFO not found in TAR\n"
            );

            return NULL;
        }

        struct PackageInfo *info =
            CATPKG_FD_Parse(
                packageinfo_fd,
                file_path,
                reuse
            );

        if (info == NULL) {
            close(packageinfo_fd);
            return NULL;
        }

        return info;
    }

    if (
        lseek(
            fd_r,
            0,
            SEEK_SET
        ) == (off_t)-1
    ) {
        perror("lseek");
        close(fd_r);
        return NULL;
    }

    struct PackageInfo *info =
        CATPKG_FD_Parse(
            fd_r,
            file_path,
            reuse
        );

    if (info == NULL) {
        close(fd_r);
        return NULL;
    }

    return info;
}

void PackageFieldMatches_Free(
    struct PackageFieldMatches *matches
)
{
    if (matches == NULL)
        return;

    free(matches->items);

    matches->items = NULL;
    matches->count = 0;
}

void PackageInfo_Free(
    struct PackageInfo *info
)
{
    if (info == NULL)
        return;


    /*
     * Free fields.
     */

    struct PackageField *field =
        info->fields;

    while (field != NULL) {
        struct PackageField *next =
            field->next_field;

        free(field->name);
        free(field->value);
        free(field);

        field = next;
    }


    /*
     * Free format information.
     */


    /*
     * Close and free reader.
     */

    if (info->reader != NULL) {
        int fd_r =
            info->reader->fd_r;

        package_reader_close(
            info->reader
        );

        if (fd_r >= 0)
            close(fd_r);

        free(info->reader);
    }


    /*
     * Free PackageInfo.
     */

    free(info);
}

ssize_t CATPKG_ReadAll(
    struct PackageInfo *info
)
{
    if (
        info == NULL ||
        info->reader == NULL
    ) {
        return SIZE_MAX;
    }

    struct PackageReader *reader =
        info->reader;


    /*
     * Restore PackageInfo state.
     */

    struct PackageField *last_field =
        info->fields;

    if (last_field != NULL) {
        while (last_field->next_field != NULL)
            last_field = last_field->next_field;
    }

    size_t current_section = 0;

    if (last_field != NULL)
        current_section =
            last_field->section + 1;

    char *section = NULL;
    size_t section_size = 0;
    long section_bytepos = 0;

    size_t fields_read = 0;

    char buffer[PATH_MAX];

    /*
     * Read remaining PackageInfo.
     */

    while (1) {
        long field_bytepos =
            reader->bytepos;

        if (
            package_reader_getline(
                reader,
                buffer,
                sizeof(buffer)
            ) == NULL
        ) {
            break;
        }

        size_t result;


        /*
         * CATPKG 1.0 — PackageInfo
         */

        if (info->patch == 0) {
            result =
                catpkg_readline_v1_packageinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
        }


        /*
         * CATPKG 1.1 — FileInfo
         */

        else if (info->patch == 1) {
            result =
                catpkg_readline_v1_fileinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
        }


        /*
         * Unsupported patch.
         */

        else {
            fprintf(
                stderr,
                "catpkg: unsupported PACKAGEINFO patch: %d\n",
                info->patch
            );

            goto error;
        }


        if (result == SIZE_MAX)
            goto error;

        fields_read +=
            (size_t)result;
    }


    free(section);

    return fields_read;


error:
    free(section);

    return SIZE_MAX;
}

ssize_t CATPKG_ReadSections(
    struct PackageInfo *info,
    size_t sections_lim
)
{
    if (
        info == NULL ||
        info->reader == NULL
    ) {
        return SIZE_MAX;
    }

    struct PackageReader *reader =
        info->reader;


    /*
     * Restore PackageInfo state.
     */

    struct PackageField *last_field =
        info->fields;

    if (last_field != NULL) {
        while (last_field->next_field != NULL)
            last_field = last_field->next_field;
    }

    size_t current_section = 0;

    if (last_field != NULL)
        current_section =
            last_field->section + 1;

    size_t offset_section = current_section;
    
    char *section = NULL;
    size_t section_size = 0;
    long section_bytepos = 0;

    size_t fields_read = 0;

    char buffer[PATH_MAX];

    /*
     * Read remaining PackageInfo.
     */

    while (current_section - offset_section < sections_lim) {
        long field_bytepos =
            reader->bytepos;

        if (
            package_reader_getline(
                reader,
                buffer,
                sizeof(buffer)
            ) == NULL
        ) {
            break;
        }

        size_t result;


        /*
         * CATPKG 1.0 — PackageInfo
         */

        if (info->patch == 0) {
            result =
                catpkg_readline_v1_packageinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
        }


        /*
         * CATPKG 1.1 — FileInfo
         */

        else if (info->patch == 1) {
            result =
                catpkg_readline_v1_fileinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
        }


        /*
         * Unsupported patch.
         */

        else {
            fprintf(
                stderr,
                "catpkg: unsupported PACKAGEINFO patch: %d\n",
                info->patch
            );

            goto error;
        }

        if (result == SIZE_MAX)
            goto error;

        fields_read +=
            (size_t)result;
    }


    free(section);

    return fields_read;


error:
    free(section);

    return SIZE_MAX;
}

ssize_t CATPKG_ReadSection(
    struct PackageInfo *info,
    size_t section_read
)
{
    if (
        info == NULL ||
        info->reader == NULL
    ) {
        return SIZE_MAX;
    }

    struct PackageReader *reader =
        info->reader;


    /*
     * Restore PackageInfo state.
     */

    struct PackageField *last_field =
        info->fields;

    if (last_field != NULL) {
        while (last_field->next_field != NULL)
            last_field = last_field->next_field;
    }

    size_t current_section = 0;

    if (last_field != NULL)
        current_section =
            last_field->section + 1;

    char *section = NULL;
    size_t section_size = 0;
    long section_bytepos = 0;

    size_t fields_read = 0;

    char buffer[PATH_MAX];

    /*
     * Read remaining PackageInfo.
     */

    while (1) {
        long field_bytepos =
            reader->bytepos;

        if (
            package_reader_getline(
                reader,
                buffer,
                sizeof(buffer)
            ) == NULL
        ) {
            break;
        }

        size_t result;


        /*
         * CATPKG 1.0 — PackageInfo
         */

        if (info->patch == 0) {
            if(current_section < section_read) {
                result =
                    catpkg_skipsection_v1_packageinfo(
                        reader,
                        buffer,
                        info,
                        &last_field,
                        &current_section,
                        &section,
                        &section_size,
                        &section_bytepos,
                        field_bytepos
                    );
                if (result == SIZE_MAX)
                    goto error;

                continue;
            }

            result =
                catpkg_readline_v1_packageinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
            break;
        }


        /*
         * CATPKG 1.1 — FileInfo
         */

        else if (info->patch == 1) {
            if(current_section < section_read) {
                result =
                    catpkg_skipsection_v1_fileinfo(
                        reader,
                        buffer,
                        info,
                        &last_field,
                        &current_section,
                        &section,
                        &section_size,
                        &section_bytepos,
                        field_bytepos
                    );
                if (result == SIZE_MAX)
                    goto error;

                continue;
            }

            result =
                catpkg_readline_v1_fileinfo(
                    reader,
                    buffer,
                    info,
                    &last_field,
                    &current_section,
                    &section,
                    &section_size,
                    &section_bytepos,
                    field_bytepos
                );
            break;
        }


        /*
         * Unsupported patch.
         */

        else {
            fprintf(
                stderr,
                "catpkg: unsupported PACKAGEINFO patch: %d\n",
                info->patch
            );

            goto error;
        }


        if (result == SIZE_MAX)
            goto error;

        fields_read +=
            (size_t)result;
    }


    free(section);

    return fields_read;


error:
    free(section);

    return SIZE_MAX;
}

int is_catpackage(
    const char *path
)
{
    if (path == NULL)
        return 0;

    int fd = open(path, O_RDONLY);

    struct header_posix_ustar hpu;

    ssize_t n = pread(fd, &hpu, sizeof(hpu), 0);

    if (n != sizeof(hpu))
        return 0;

    return memcmp(hpu.magic, "ustar", 5) == 0;
}