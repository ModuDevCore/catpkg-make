// ============================================================
// COMPILER CONFIGURATION
// ============================================================

// Compiler executables used for the supported target architectures.
#define GCC_AARCH64 "aarch64-linux-gnu-gcc"
#define GCC_X86_64 "gcc"

// ============================================================
// CLI CONFIGURATION
// ============================================================

// Welcome message displayed when the CATPKG Make utility starts.
#define MAKE_CLI_WELCOME_TEXT \
    "========================================\n" \
    "             CATPKG MAKE\n" \
    "      The CATPKG utility builder.\n" \
    "========================================\n" \
    "\n" \
    "Welcome to the CATPKG utility builder!\n" \
    "Choose which build you need:\n\n"

// ============================================================
// BUILD VERSIONS
// ============================================================

// Architecture placeholder used in build option descriptions.
#define POSTFIX_AARCH64ORX86_64 "(aarch64/x86_64)"

// Version identifiers assigned to individual build targets.
#define CATSTRAP_CATPKG_VERSION "2.0.2b"
#define CATSTRAP_CATPKG_LITE_VERSION "2.0.2b"
#define CATSTRAP_CATPKG_NET_VERSION "2.0.2b"
#define CATSTRAP_CATPKG_CUSTOM_VERSION "2.0.0b"
#define CATPKG_CATPACKAGE_VERSION "2.0.2b"
#define CATSTRAPLIB_CATPACKAGE_VERSION "2.0.2b"
#define CATSTRAP_BIN_CATPACKAGE_VERSION "2.0.2b"
#define CATPKG_BUILD_CATPACKAGE_VERSION "1.0.0b"
#define FILESYSTEM_CATPACKAGE_VERSION "1.0"
#define FILESYSTEM_ANDROID_CATPACKAGE_VERSION "1.0"

// ============================================================
// BUILD OPTIONS
// ============================================================

// Declares available build targets and their CLI descriptions.
// Each entry maps a build identifier to its displayed description.
#define BUILD_OPTIONS(X) \
    X(CATSTRAP_CATPKG, "Building catstrap-catpkg-" CATSTRAP_CATPKG_VERSION "-" POSTFIX_AARCH64ORX86_64) \
    X(CATSTRAP_CATPKG_LITE, "Building catstrap-catpkg-lite-" CATSTRAP_CATPKG_LITE_VERSION "-" POSTFIX_AARCH64ORX86_64) \
    X(CATSTRAP_CATPKG_NET, "Building catstrap-catpkg-net-" CATSTRAP_CATPKG_NET_VERSION "-" POSTFIX_AARCH64ORX86_64) \
    X(CATSTRAP_CATPKG_CUSTOM, "Building catstrap-catpkg-custom-" CATSTRAP_CATPKG_CUSTOM_VERSION "-" POSTFIX_AARCH64ORX86_64) \
    X(CATSTRAP_EMPTY, "Building catstrap (Empty)\n") \
    X(CATPKG_CATPACKAGE, "Building the catpkg-" POSTFIX_AARCH64ORX86_64 "@" CATPKG_CATPACKAGE_VERSION ".catpackage") \
    X(CATSTRAPLIB_CATPACKAGE, "Building the catstraplib-" POSTFIX_AARCH64ORX86_64 "@" CATSTRAPLIB_CATPACKAGE_VERSION ".catpackage") \
    X(CATSTRAP_BIN_CATPACKAGE, "Building the catstrap-bin-" POSTFIX_AARCH64ORX86_64 "@" CATSTRAP_BIN_CATPACKAGE_VERSION ".catpackage") \
    X(CATPKG_BUILD_CATPACKAGE, "Building the catpkg-build-" POSTFIX_AARCH64ORX86_64 "@" CATPKG_BUILD_CATPACKAGE_VERSION ".catpackage\n") \
    X(FILESYSTEM_CATPACKAGE, "Building the filesystem@" FILESYSTEM_CATPACKAGE_VERSION ".catpackage") \
    X(FILESYSTEM_ANDROID_CATPACKAGE, "Building the filesystem-android@" FILESYSTEM_ANDROID_CATPACKAGE_VERSION ".catpackage\n") \
    X(CATPKG_MAKE, "Building catpkg-make-" POSTFIX_AARCH64ORX86_64 " (Update)")

// ============================================================
// BUILD DIRECTORY STRUCTURE
// ============================================================

// Declares directories that must be created for each build target.
// Paths are relative to the corresponding build working directory.
#define BUILD_DIRECTORIES(X) \
    X(CATPKG_BUILD_CATPACKAGE, "extra") \
    X(CATPKG_BUILD_CATPACKAGE, "extra/COMMAND_DATA") \
    X(CATPKG_CATPACKAGE, "usr") \
    X(CATSTRAP_CATPKG_LITE, "usr") \
    X(CATSTRAP_CATPKG_NET, "usr") \
    X(CATSTRAPLIB_CATPACKAGE, "usr") \
    X(CATSTRAP_BIN_CATPACKAGE, "usr") \
    X(CATPKG_CATPACKAGE, "usr/bin") \
    X(CATSTRAP_CATPKG_LITE, "usr/bin") \
    X(CATSTRAP_CATPKG_NET, "usr/bin") \
    X(CATSTRAPLIB_CATPACKAGE, "usr/bin") \
    X(CATSTRAP_BIN_CATPACKAGE, "usr/bin") \
    X(CATSTRAPLIB_CATPACKAGE, "usr/lib") \
    X(CATSTRAPLIB_CATPACKAGE, "usr/lib/catpkg")

// ============================================================
// BUILD DATA IMPORT
// ============================================================

// Declares files to import into the build working directory.
// Each entry specifies the build target, source path, and
// destination path relative to the build directory.
#define BUILD_DATA_IMPORT(X) \
    X(CATPKG_BUILD_CATPACKAGE, "build-data/catpkg-build/PACKAGEINFO", "PACKAGEINFO") \
    X(CATPKG_BUILD_CATPACKAGE, "build-data/catpkg-build/CATPKG-BUILD-COMMAND", "CATPKG-BUILD-COMMAND") \
    X(CATPKG_CATPACKAGE, "build-data/PACKAGEINFO", "PACKAGEINFO") \
    X(CATSTRAP_BIN_CATPACKAGE, "build-data/catstrap-bin/PACKAGEINFO", "PACKAGEINFO") \
    X(CATSTRAP_BIN_CATPACKAGE, "build-data/catstrap-bin/catstrap-bin", "usr/bin/catstrap-bin") \
    X(CATSTRAPLIB_CATPACKAGE, "build-data/catstraplib/PACKAGEINFO", "PACKAGEINFO") \
    X(FILESYSTEM_CATPACKAGE, "build-data/filesystem/PACKAGEINFO", "PACKAGEINFO") \
    X(FILESYSTEM_ANDROID_CATPACKAGE, "build-data/filesystem-android/PACKAGEINFO", "PACKAGEINFO")

// ============================================================
// BUILD MACRO DECLARATIONS
// ============================================================

// Associates build targets with named macro values.
// These declarations can be used when processing build metadata
// and substituting macros in generated files.
#define BUILD_MACRO(X) \
    X(CATSTRAP_CATPKG, "BUILD_VERSION", CATSTRAP_CATPKG_VERSION) \
    X(CATSTRAP_CATPKG_LITE, "BUILD_VERSION", CATSTRAP_CATPKG_LITE_VERSION) \
    X(CATSTRAP_CATPKG_NET, "BUILD_VERSION", CATSTRAP_CATPKG_NET_VERSION) \
    X(CATSTRAP_CATPKG_CUSTOM, "BUILD_VERSION", CATSTRAP_CATPKG_CUSTOM_VERSION) \
    X(CATPKG_CATPACKAGE, "BUILD_VERSION", CATPKG_CATPACKAGE_VERSION) \
    X(CATSTRAPLIB_CATPACKAGE, "BUILD_VERSION", CATSTRAPLIB_CATPACKAGE_VERSION) \
    X(CATSTRAP_BIN_CATPACKAGE, "BUILD_VERSION", CATSTRAP_BIN_CATPACKAGE_VERSION) \
    X(CATPKG_BUILD_CATPACKAGE, "BUILD_VERSION", CATPKG_BUILD_CATPACKAGE_VERSION) \
    X(FILESYSTEM_CATPACKAGE, "BUILD_VERSION", FILESYSTEM_CATPACKAGE_VERSION) \
    X(FILESYSTEM_ANDROID_CATPACKAGE, "BUILD_VERSION", FILESYSTEM_ANDROID_CATPACKAGE_VERSION)

// ============================================================
// SHELL SCRIPT CONFIGURATION
// ============================================================

// Enables immediate script termination when a command fails.
#define SH_INIT "set -e\n"

// Common compiler flags used when building CATPKG components.
#define SH_COMPILE_FLAGS \
    "-std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE " \
    "-Os -ffunction-sections -fdata-sections " \
    "-Wextra -Wpedantic -I../include"

// Common linker flags for section garbage collection and
// runtime library search paths.
#define SH_LINK_FLAGS "-Wl,--gc-sections -Wl,-rpath,/usr/lib"

// Compiles the main CATPKG command-line executable.
#define SH_BUILD_CATPKG \
    "\"$compiler\" " SH_COMPILE_FLAGS " " \
    "../catpkg/cli.c ../catpkg/main.c " \
    "../extra/$a10e/catstraplib.so.0 " \
    SH_LINK_FLAGS " -o usr/bin/catpkg\n"

// Compiles the CATPKG command execution utility.
#define SH_BUILD_CATPKG_COMMAND \
    "\"$compiler\" " SH_COMPILE_FLAGS " " \
    "../catpkg-command/main.c " \
    "../extra/$a10e/catstraplib.so.0 " \
    SH_LINK_FLAGS " -o usr/bin/catpkg-command\n"

// Compiles the CATPKG package builder CLI.
#define SH_BUILD_CATPKG_BUILD \
    "\"$compiler\" " SH_COMPILE_FLAGS " " \
    "../catpkg-build/build.c ../catpkg-build/catpkg-build-cli.c " \
    "/usr/lib/catstraplib.so.0 " \
    SH_LINK_FLAGS " -o extra/COMMAND_DATA/catpkg-build-cli\n"

// Compiles the CATPKG Make utility as a static executable.
#define SH_BUILD_CATPKG_MAKE \
    "\"$compiler\" -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 " \
    "-std=c23 -Wextra -Wpedantic -Iinclude " \
    "catpkg-build/build.c src/make-cli.c " \
    "extra/$a10e/catstrap.o -static -o catpkg-make-$a10e\n"

// Builds the standard CATPKG bootstrap executable.
#define SH_BUILD_CATSTRAP_CATPKG \
    "./build-cast.sh \"$compiler\" ./extra/$a10e " \
    "catstrap-catpkg " CAST_HEADER_FREE_STR " " \
    "catstrap-catpkg-" CATSTRAP_CATPKG_VERSION "-$a10e\n"

// Builds the lightweight CATPKG bootstrap executable.
#define SH_BUILD_CATSTRAP_CATPKG_LITE \
    "./build-cast.sh \"$compiler\" ./extra/$a10e " \
    "catstrap-catpkg-lite " CAST_HEADER_FREE_STR " " \
    "catstrap-catpkg-lite-" CATSTRAP_CATPKG_LITE_VERSION "-$a10e\n"

// Builds the network-enabled CATPKG bootstrap executable.
#define SH_BUILD_CATSTRAP_CATPKG_NET \
    "./build-cast.sh \"$compiler\" ./extra/$a10e " \
    "catstrap-catpkg-net " CAST_HEADER_FREE_STR " " \
    "catstrap-catpkg-net-" CATSTRAP_CATPKG_NET_VERSION "-$a10e\n"

// ============================================================
// BUILD SCRIPT REGISTRY
// ============================================================

// Associates build targets with their shell compilation scripts.
// Each script is assembled from reusable SH_* command fragments.
#define BUILD_SCRIPTS(X) \
    X(CATPKG_CATPACKAGE, SH_INIT SH_BUILD_CATPKG SH_BUILD_CATPKG_COMMAND) \
    X(CATPKG_BUILD_CATPACKAGE, SH_INIT SH_BUILD_CATPKG_BUILD) \
    X(CATPKG_MAKE, SH_INIT SH_BUILD_CATPKG_MAKE) \
    X(CATSTRAP_CATPKG, SH_INIT SH_BUILD_CATSTRAP_CATPKG) \
    X(CATSTRAP_CATPKG_LITE, SH_INIT SH_BUILD_CATSTRAP_CATPKG_LITE) \
    X(CATSTRAP_CATPKG_NET, SH_INIT SH_BUILD_CATSTRAP_CATPKG_NET)

// ============================================================
// PACKAGE BUILD SELECTION
// ============================================================

// Identifies targets that require CATPKG package generation
// after their build data has been prepared.
#define BUILD_PACKAGE(X) \
    X(CATPKG_CATPACKAGE) \
    X(CATPKG_BUILD_CATPACKAGE) \
    X(CATSTRAP_BIN_CATPACKAGE) \
    X(CATSTRAPLIB_CATPACKAGE) \
    X(FILESYSTEM_CATPACKAGE) \
    X(FILESYSTEM_ANDROID_CATPACKAGE)

// ============================================================
// TEMPORARY BUILD DIRECTORIES
// ============================================================

// Identifies targets that use temporary build directories
// for staging files and intermediate build artifacts.
#define BUILD_TMP(X) \
    X(FILESYSTEM_CATPACKAGE) \
    X(FILESYSTEM_ANDROID_CATPACKAGE) \
    X(CATPKG_BUILD_CATPACKAGE) \
    X(CATPKG_CATPACKAGE) \
    X(CATSTRAPLIB_CATPACKAGE) \
    X(CATSTRAP_BIN_CATPACKAGE) \
    X(CATSTRAP_CATPKG_LITE) \
    X(CATSTRAP_CATPKG_NET) \
    X(CATSTRAP_CATPKG)

// ============================================================
// BOOTSTRAP PREPARATION
// ============================================================

// Identifies targets that require the bootstrap preparation
// stage as part of their build workflow.
#define BUILD_BOOTSTRAP(X) \
    X(CATSTRAP_CATPKG) \
    X(CATSTRAP_CATPKG_LITE) \
    X(CATSTRAP_CATPKG_NET) \
    X(CATSTRAP_BIN_CATPACKAGE) \
    X(CATSTRAPLIB_CATPACKAGE) \
    X(CATPKG_CATPACKAGE) \
    X(CATPKG_MAKE)

// ============================================================
// TEMPORARY SCRIPT EXECUTION
// ============================================================

// Identifies targets whose build scripts execute within
// the temporary build working directory.
#define BUILD_SCRIPT_TMP(X) \
    X(CATPKG_CATPACKAGE) \
    X(CATPKG_BUILD_CATPACKAGE)

// ============================================================
// TARGET ARCHITECTURE PREPARATION
// ============================================================

// Identifies targets that require architecture selection
// and preparation before executing their build operations.
#define BUILD_ARCH_PREPARE(X) \
    X(CATSTRAP_CATPKG) \
    X(CATSTRAP_CATPKG_LITE) \
    X(CATSTRAP_CATPKG_NET) \
    X(CATPKG_CATPACKAGE) \
    X(CATSTRAP_BIN_CATPACKAGE) \
    X(CATSTRAPLIB_CATPACKAGE) \
    X(CATPKG_BUILD_CATPACKAGE) \
    X(CATPKG_MAKE)

// ============================================================
// BUILD ENVIRONMENT VARIABLES
// ============================================================

// Declares environment variables assigned to specific build targets.
// Each entry contains the target identifier, variable name, and value.
// POSTPROC_CUSTOM_MACRO provides additional macro declarations
// to the postprocessor through the process environment.
#define BUILD_ENV(X) \
    X(CATPKG_CATPACKAGE, "POSTPROC_CUSTOM_MACRO", "BUILD_VERSION=" CATPKG_CATPACKAGE_VERSION) \
    X(CATSTRAP_BIN_CATPACKAGE, "POSTPROC_CUSTOM_MACRO", "BUILD_VERSION=" CATSTRAP_BIN_CATPACKAGE_VERSION) \
    X(CATSTRAPLIB_CATPACKAGE, "POSTPROC_CUSTOM_MACRO", "BUILD_VERSION=" CATSTRAPLIB_CATPACKAGE_VERSION) \
    X(CATPKG_BUILD_CATPACKAGE, "POSTPROC_CUSTOM_MACRO", "BUILD_VERSION=" CATPKG_BUILD_CATPACKAGE_VERSION)
