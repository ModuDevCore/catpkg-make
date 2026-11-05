#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <ftw.h>
#include <fcntl.h>

#include "configuration.h"
#include "catpkg/pkginfo.h"
#include "utils/sha256.h"
#include "utils/postprocess.h"
#include "utils/fs.h"
#include "build.h"
#include "verify.h"

/*
 * Check whether PACKAGEINFO exists
 * and is a regular file.
 */
static int packageinfo_exists(
    const char *path
)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISREG(st.st_mode);
}


/*
 * Create TAR archive from the current
 * working directory.
 *
 * Equivalent to:
 *
 * tar -cf <archive> .
 *
 * The archive itself is excluded.
 */
static int create_tar(
    const char *archive
)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");
        return 1;
    }



    if (pid == 0) {
        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "-cf",
            archive,
            "--exclude",
            archive,
            "--transform=s|^\\./||",
            ".",
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");
        return 1;
    }


    if (!WIFEXITED(status)) {

        fprintf(
            stderr,
            "catpkg: tar terminated abnormally\n"
        );

        return 1;
    }


    int exit_code =
        WEXITSTATUS(status);


    if (exit_code != 0) {

        fprintf(
            stderr,
            "catpkg: tar exited with code %d\n",
            exit_code
        );

        return 1;
    }


    return 0;
}


/*
 * Remove PACKAGEINFO from an existing TAR
 * and insert the new PACKAGEINFO.
 *
 * packageinfo_tmp:
 *
 *     ./PACKAGEINFO.tmp
 *
 * archive:
 *
 *     package.tar
 *
 * Result:
 *
 *     package.tar
 *     └── ./PACKAGEINFO
 *
 * The temporary file itself is never stored
 * in the archive.
 */
static int replace_packageinfo(
    const char *archive,
    const char *packageinfo_tmp
)
{
    char temp_dir[] = ".catpkg_packageinfo_XXXXXX";

    /*
     * Create temporary directory.
     */
    if (mkdtemp(temp_dir) == NULL) {
        perror(
            "catpkg: failed to create temporary directory"
        );

        return 1;
    }


    /*
     * Build path:
     *
     * .catpkg_packageinfo_XXXXXX/PACKAGEINFO
     */
    char packageinfo_path[PATH_MAX];

    int written = snprintf(
        packageinfo_path,
        sizeof(packageinfo_path),
        "%s/" CATPKG_PACKAGEINFO,
        temp_dir
    );

    if (written < 0 ||
        (size_t)written >= sizeof(packageinfo_path)) {

        fprintf(
            stderr,
            "catpkg: temporary PACKAGEINFO path is too long\n"
        );

        rmdir(temp_dir);

        return 1;
    }


    /*
     * Copy PACKAGEINFO.tmp into the temporary
     * directory under the final name PACKAGEINFO.
     */
    FILE *src = fopen(
        packageinfo_tmp,
        "rb"
    );

    if (src == NULL) {
        perror(
            "catpkg: failed to open temporary PACKAGEINFO"
        );

        rmdir(temp_dir);

        return 1;
    }


    FILE *dst = fopen(
        packageinfo_path,
        "wb"
    );

    if (dst == NULL) {
        perror(
            "catpkg: failed to create temporary PACKAGEINFO copy"
        );

        fclose(src);
        rmdir(temp_dir);

        return 1;
    }


    char buffer[65536];
    size_t bytes;

    int copy_failed = 0;

    while ((bytes = fread(
                buffer,
                1,
                sizeof(buffer),
                src
            )) > 0) {

        if (fwrite(
                buffer,
                1,
                bytes,
                dst
            ) != bytes) {

            perror(
                "catpkg: failed to write PACKAGEINFO copy"
            );

            copy_failed = 1;
            break;
        }
    }


    if (ferror(src)) {
        perror(
            "catpkg: failed to read temporary PACKAGEINFO"
        );

        copy_failed = 1;
    }


    if (fclose(src) != 0) {
        perror(
            "catpkg: failed to close temporary PACKAGEINFO"
        );

        copy_failed = 1;
    }


    if (fclose(dst) != 0) {
        perror(
            "catpkg: failed to close PACKAGEINFO copy"
        );

        copy_failed = 1;
    }


    if (copy_failed) {
        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * First remove the old PACKAGEINFO.
     *
     * tar --delete -f archive ./PACKAGEINFO
     */
    pid_t pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");
        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (pid == 0) {

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "--delete",
            "-f",
            archive,
            CATPKG_PACKAGEINFO,
            (char *)NULL
        );

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "--delete",
            "-f",
            archive,
            CATPKG_PACKAGEINFO,
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to remove old PACKAGEINFO from archive\n"
        );

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * Now append the new PACKAGEINFO.
     *
     * We use:
     *
     * tar -rf archive -C temp_dir PACKAGEINFO
     *
     * This is important because the file is stored
     * as "PACKAGEINFO", not ".catpkg_.../PACKAGEINFO".
     */
    pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (pid == 0) {


        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "-rf",
            archive,
            "-C",
            temp_dir,
            "PACKAGEINFO",
            (char *)NULL
        );

        execl(
            getenv("CATSTRAP-BIN"),
            getenv("CATSTRAP-BIN"),
            "tar",
            "-rf",
            archive,
            "-C",
            temp_dir,
            "PACKAGEINFO",
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * Remove temporary files.
     */
    unlink(packageinfo_path);
    rmdir(temp_dir);


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to insert new PACKAGEINFO into archive\n"
        );

        return 1;
    }


    return 0;
}


static off_t package_size;


static int catpkg_size_callback(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)ftwbuf;

    if (strcmp(path, "./CATPACKAGE") == 0)
        return 0;

    if (typeflag == FTW_F)
        package_size += st->st_size;

    return 0;
}


off_t catpkg_get_size(
    const char *path
)
{
    package_size = 0;

    if (nftw(
            path,
            catpkg_size_callback,
            20,
            FTW_PHYS
        ) != 0) {

        return -1;
    }

    return package_size;
}


/*
 * Build package.
 *
 * Reads:
 *
 *     PACKAGEINFO
 *
 * Creates:
 *
 *     <name>@<version>.catpackage
 *
 *
 * Build process:
 *
 *     1. Create TAR
 *     2. Calculate SHA-256 excluding PACKAGEINFO
 *     3. Generate PACKAGEINFO.tmp
 *     4. Replace PACKAGEINFO inside TAR
 *     5. Rename TAR -> CATPACKAGE
 */

int catpkg_build(char *a10e)
{
    printf("[INFO] Starting package build...\n");

    /*
     * Check PACKAGEINFO.
     */
    printf(
        "[INFO] Checking %s...\n",
        CATPKG_PACKAGEINFO
    );

    if (!packageinfo_exists(CATPKG_PACKAGEINFO)) {
        fprintf(
            stderr,
            "catpkg: %s not found\n",
            CATPKG_PACKAGEINFO
        );

        return 1;
    }

    printf(
        "[INFO] %s found.\n",
        CATPKG_PACKAGEINFO
    );


    /*
     * Calculate package size.
     *
     * This is done before generating the processed PACKAGEINFO.
     */
    off_t size =
        catpkg_get_size("./");

    if (size < 0) {
        fprintf(
            stderr,
            "catpkg: failed to calculate package size\n"
        );

        return 1;
    }


    char size_buffer[64];

    catpkg_format_size(
        size,
        size_buffer,
        sizeof(size_buffer)
    );


    /*
     * Convert raw package size to string for __SIZE__.
     */
    char size_raw_buffer[64];

    snprintf(
        size_raw_buffer,
        sizeof(size_raw_buffer),
        "%-63jd",
        (intmax_t)size
    );

    /*
     * Create temporary PACKAGEINFO path.
     */
    char packageinfo_tmp[PATH_MAX];

    int written = snprintf(
        packageinfo_tmp,
        sizeof(packageinfo_tmp),
        "%s.tmp",
        CATPKG_PACKAGEINFO
    );

    if (written < 0 ||
        (size_t)written >= sizeof(packageinfo_tmp)) {

        fprintf(
            stderr,
            "catpkg: temporary PACKAGEINFO path is too long\n"
        );

        return 1;
    }


    /*
     * Create temporary TAR.
     *
     * At this stage we don't know the final package name yet,
     * because the processed PACKAGEINFO has not been parsed.
     */
    char tar_path[PATH_MAX];

    written = snprintf(
        tar_path,
        sizeof(tar_path),
        "%s",
        ".catpkg-build.tmp.tar"
    );

    if (written < 0 ||
        (size_t)written >= sizeof(tar_path)) {

        fprintf(
            stderr,
            "catpkg: temporary TAR path is too long\n"
        );

        return 1;
    }


    /*
     * Create TAR archive.
     */
    printf(
        "[INFO] Creating TAR archive...\n"
    );

    printf(
        "[INFO] Destination: %s\n",
        tar_path
    );

    if (create_tar(tar_path) != 0) {
        fprintf(
            stderr,
            "catpkg: failed to create TAR archive\n"
        );

        return 1;
    }

    printf(
        "[INFO] TAR archive created successfully.\n"
    );


    /*
     * Calculate SHA-256.
     *
     * catpkg_sign_catpackage() ignores PACKAGEINFO.
     */
    printf(
        "[INFO] Calculating package SHA-256...\n"
    );

    struct sha256_buff hash;

    if (catpkg_sign_catpackage(
            tar_path,
            &hash
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to calculate package SHA-256\n"
        );

        unlink(tar_path);

        return 1;
    }


    char hex[65];

    sha256_read_hex(
        &hash,
        hex
    );

    hex[64] = '\0';


    printf(
        "Package size: %s\n",
        size_buffer
    );

    printf(
        "sha256: %s\n",
        hex
    );


    /*
     * Generate processed PACKAGEINFO.
     *
     * The original PACKAGEINFO remains unchanged.
     */
    printf(
        "[INFO] Generating %s...\n",
        packageinfo_tmp
    );

    struct MacroDeclaration macros[2];

    macros[0] = (struct MacroDeclaration) {
        "ARCHITECTURE",
        (intptr_t) a10e,
        1
    };

    macros[1] = (struct MacroDeclaration) {
        "SHA",
        (intptr_t) hex,
        1
    };

    // macros[2] = (struct MacroDeclaration) {
    //     "SIZE",
    //     (intptr_t) size_raw_buffer,
    //     1
    // };

    int input_fd = open(
        CATPKG_PACKAGEINFO,
        O_RDONLY
    );

    int output_fd = open(
        packageinfo_tmp,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (postproccess_format_file_macros(
            input_fd,
            output_fd,
            macros,
            2,
            0
        ) == -1) {

        fprintf(
            stderr,
            "catpkg: failed to postprocess %s\n",
            CATPKG_PACKAGEINFO
        );

        unlink(packageinfo_tmp);
        unlink(tar_path);

        close(input_fd);
        close(output_fd);

        return 1;
    }
    close(input_fd);
    close(output_fd);


    printf(
        "[INFO] %s generated successfully.\n",
        packageinfo_tmp
    );


    /*
     * Parse the processed PACKAGEINFO.
     *
     * From this point onward all PACKAGEINFO fields contain
     * their final build-time values.
     */
    printf(
        "[INFO] Parsing %s...\n",
        packageinfo_tmp
    );

    struct PackageInfo *info = CATPKG_Parse(packageinfo_tmp, NULL);
    CATPKG_ReadSections(info, 2);

    if (info == NULL) {
        fprintf(
            stderr,
            "catpkg: failed to parse %s\n",
            packageinfo_tmp
        );

        unlink(packageinfo_tmp);
        unlink(tar_path);

        return 1;
    }


    /*
     * Find package name.
     */
    const struct PackageField *name_field =
        PackageInfo_Find(
            info,
            "name"
        );

    if (name_field == NULL ||
        name_field->value == NULL ||
        name_field->value[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO does not contain "
            "a valid package name\n"
        );

        PackageInfo_Free(info);
        unlink(packageinfo_tmp);
        unlink(tar_path);

        return 1;
    }


    /*
     * Find package version.
     */
    const struct PackageField *version_field =
        PackageInfo_Find(
            info,
            "version"
        );

    if (version_field == NULL ||
        version_field->value == NULL ||
        version_field->value[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO does not contain "
            "a valid package version\n"
        );

        PackageInfo_Free(info);
        unlink(packageinfo_tmp);
        unlink(tar_path);

        return 1;
    }


    /*
     * Build final CATPACKAGE path.
     */
    char catpackage_path[PATH_MAX];

    written = snprintf(
        catpackage_path,
        sizeof(catpackage_path),
        "%s@%s.catpackage",
        name_field->value,
        version_field->value
    );

    if (written < 0 ||
        (size_t)written >= sizeof(catpackage_path)) {

        fprintf(
            stderr,
            "catpkg: CATPACKAGE path is too long\n"
        );

        PackageInfo_Free(info);
        unlink(packageinfo_tmp);
        unlink(tar_path);

        return 1;
    }


    /*
     * Replace PACKAGEINFO inside TAR.
     *
     * The archive has already been signed.
     * PACKAGEINFO is excluded from the signature,
     * therefore replacing it does not invalidate
     * the calculated SHA-256.
     */
    printf(
        "[INFO] Replacing PACKAGEINFO inside TAR...\n"
    );

    if (replace_packageinfo(
            tar_path,
            packageinfo_tmp
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to replace PACKAGEINFO "
            "inside TAR archive\n"
        );

        PackageInfo_Free(info);
        unlink(packageinfo_tmp);
        unlink(tar_path);

        return 1;
    }


    /*
     * PACKAGEINFO.tmp is no longer needed.
     */
    if (unlink(packageinfo_tmp) != 0) {
        perror(
            "catpkg: failed to remove temporary PACKAGEINFO"
        );
    }


    /*
     * Remove old CATPACKAGE.
     */
    if (unlink(catpackage_path) != 0 &&
        errno != ENOENT) {

        fprintf(
            stderr,
            "catpkg: failed to remove old \"%s\": ",
            catpackage_path
        );

        perror(NULL);

        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    /*
     * Rename temporary TAR -> final CATPACKAGE.
     */
    printf(
        "[INFO] Converting TAR archive to CATPACKAGE...\n"
    );

    if (rename(
            tar_path,
            catpackage_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to rename \"%s\" "
            "to \"%s\": ",
            tar_path,
            catpackage_path
        );

        perror(NULL);

        unlink(tar_path);
        PackageInfo_Free(info);

        return 1;
    }


    printf(
        "[INFO] CATPACKAGE created successfully.\n"
    );

    printf(
        "[INFO] Package: %s\n",
        catpackage_path
    );


    PackageInfo_Free(info);


    printf(
        "[INFO] Package build completed successfully.\n"
    );

    return 0;
}