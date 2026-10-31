#define _GNU_SOURCE

#include "../catpkg/verify.c"
#include "../catpkg/install.c"
#include "../catpkg/remove.c"
#include "../catpkg/update.c"
#include "../catpkg/database.c"

#include "../catpkg/utils/catpkg/command.c"
#include "../catpkg/utils/catpkg/builder.c"
#include "../catpkg/utils/sha256.c"
#include "../catpkg/utils/catpkg/cache.c"
#include "../catpkg/utils/catpkg/path.c"
#include "../catpkg/utils/catpkg/pkginfo.c"
#include "../catpkg/utils/fs.c"
#include "../catpkg/utils/postprocess.c"
#include "../catpkg/utils/allow.c"
#include "../catpkg/utils/catpkg/ctpg.c"