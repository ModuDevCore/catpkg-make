#!/bin/sh
set -e

GCC=$1
GCC_AARCH64=$2

EXTRA_PATH=$3
EXTRA_PATH_AARCH64=$4

if command -v "$GCC" >/dev/null 2>&1; then
    echo "Compile bootstrap with $GCC."
    echo "Compile \"$EXTRA_PATH/catstrap.o\"..."

    "$GCC" \
        -D_DEFAULT_SOURCE \
        -D_XOPEN_SOURCE=700 \
        -std=c23 \
        -Wextra \
        -Wpedantic \
        -Iinclude \
        -fPIC \
        -c \
        "bootstrap/catstrap.c" \
        -o "$EXTRA_PATH/catstrap.o"

    echo "Compile \"$EXTRA_PATH/catstraplib.so.0\"..."

    "$GCC" \
        -shared \
        -Wl,-soname,catstraplib.so.0 \
        "$EXTRA_PATH/catstrap.o" \
        -o "$EXTRA_PATH/catstraplib.so.0"
else
    echo "Not found $GCC, SKIP."
fi

if command -v "$GCC_AARCH64" >/dev/null 2>&1; then
    echo "Compile bootstrap with $GCC_AARCH64."

    echo "Compile \"$EXTRA_PATH_AARCH64/catstrap.o\"..."

    "$GCC_AARCH64" \
        -D_DEFAULT_SOURCE \
        -D_XOPEN_SOURCE=700 \
        -std=c23 \
        -Wextra \
        -Wpedantic \
        -Iinclude \
        -fPIC \
        -c \
        "bootstrap/catstrap.c" \
        -o "$EXTRA_PATH_AARCH64/catstrap.o"

    echo "Compile \"$EXTRA_PATH_AARCH64/catstraplib.so.0\"..."

    "$GCC_AARCH64" \
        -shared \
        -Wl,-soname,catstraplib.so.0 \
        "$EXTRA_PATH_AARCH64/catstrap.o" \
        -o "$EXTRA_PATH_AARCH64/catstraplib.so.0"
else
    echo "Not found $GCC_AARCH64, SKIP."
fi

echo "Ready."