#ifndef CATPKG_PACKAGEMANAGER_CONFIGURATION_H
#define CATPKG_PACKAGEMANAGER_CONFIGURATION_H

#define CATPKG_PACKAGEINFO "PACKAGEINFO"
#define CATPKG_FILE "FILEINFO"
#define CATPKG_DATABASE "DATABASE"
#define CATPKG_DEPENDENCIES "DEPENDENCIES"
#define CATPKG_POLICY "POLICY"

#define CATPKG_PACKAGE_DATA "/var/lib/%s"

#define CATPKG_PACKAGE_ROOT "/var/lib/catpkg"

#define CATPKG_PACKAGES_PATH "/var/lib/catpackage" "/packages"
#define CATPKG_DATABASE_DIR_PATH "/var/lib/catpackage" "/database"
#define CATPKG_COMMANDS_DIR_PATH CATPKG_PACKAGE_ROOT "/commands"
#define CATPKG_PACKAGE_PATH CATPKG_PACKAGES_PATH "/%s"
#define CATPKG_METADATA_PATH CATPKG_PACKAGE_PATH "/metadata"
#define CATPKG_FILES_PATH CATPKG_PACKAGE_PATH "/files"
#define CATPKG_CACHE_CATPACKAGE "%s.catpackage"
#define CATPKG_CACHE_CATPACKAGE_ZSTD "%s.catpackage.zst"
#define CATPKG_CACHE "/var/cache/catpkg"
#define CATPKG_CACHE_PACKAGES CATPKG_CACHE "/packages"

#define PACKAGE_PROGRESS_INFO_SHEME \
    "\n** %s **\n" \
    "Package: %s [%u / %u]\n" \
    "%s"

#define CATPKG_HELP \
    "Usage: catpkg <command> [options]\n" \
    "\n" \
    "Commands:\n"

#define ALLOW_INSTALLATION_INFO \
    "\n!!###################[info]###################!!\n\n" \
    "   Installing: %s\n" \
    "   Packages size: %s\n" \
    "   Will be installed: %s\n" \
    "   Changing the size: %c%s\n" \
    "\n################################################\n\n\r"

#define ALLOW_INSTALLATION_ASK \
    "Install the current packages? [Y\\n]: "

#define ALLOW_INSTALLATION ALLOW_INSTALLATION_INFO ALLOW_INSTALLATION_ASK

#define ALLOW_UPDATE_INFO \
    "\n!!###################[info]###################!!\n\n" \
    "   Update: %s\n" \
    "   Packages size: %s\n" \
    "   Additional space: %s\n" \
    "\n################################################\n\n\r"

#define ALLOW_UPDATE_ASK \
    "Update the current packages? [Y\\n]: "

#define ALLOW_UPDATE ALLOW_UPDATE_INFO ALLOW_UPDATE_ASK

#define ALLOW_REMOVE_INFO \
    "\n!!###################[info]###################!!\n\n" \
    "   Removal: %s\n" \
    "   Will be released: %s\n" \
    "\n################################################\n\n\r"

#define ALLOW_REMOVE_ASK \
    "Delete the current packages? [Y\\n]: "

#define ALLOW_REMOVE ALLOW_REMOVE_INFO ALLOW_REMOVE_ASK

#endif