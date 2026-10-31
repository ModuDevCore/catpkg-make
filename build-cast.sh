#!/bin/sh
set -e

GCC=$1
EXTRA_PATH=$2
EXTRA_PACKAGE_FOLDER=$3
TRUNCATE=$4
CATPSTRAP_NAME=$5

echo "The compiler used: $GCC"

$GCC \
    -std=c23 \
    -I./catstrap/cast/include \
    -I include \
    "$EXTRA_PATH/catstrap.o" \
    ./catstrap/cast-cli.c \
    -static \
    -o .tmp/cast-cli.elf

tar -cf .tmp/cast-cli.tar -C .tmp cast-cli.elf

tar -rf .tmp/cast-cli.tar --dereference -C "$EXTRA_PATH/$EXTRA_PACKAGE_FOLDER" packages/

tar -rf .tmp/cast-cli.tar --dereference -C "$EXTRA_PATH/$EXTRA_PACKAGE_FOLDER" bin/

tar -rf .tmp/cast-cli.tar --dereference -C "$EXTRA_PATH/$EXTRA_PACKAGE_FOLDER" InstallManifest

$GCC \
    -std=c23 \
    -I./catstrap/cast/include \
    ./catstrap/cast/cast-header.c \
    -static \
    -o $CATPSTRAP_NAME

echo "Truncate file: $TRUNCATE"
truncate -s $TRUNCATE $CATPSTRAP_NAME

cat .tmp/cast-cli.tar >> $CATPSTRAP_NAME

echo "Compilation successful."