#!/bin/bash
BUILD_DIR="./build"
EXEFS_DIR="/mnt/c/Users/Binston/AppData/Roaming/Ryujinx/sdcard/atmosphere/contents/0100000000010000/exefs"
ROMFS_DIR="/mnt/c/Users/Binston/AppData/Roaming/Ryujinx/sdcard/atmosphere/contents/0100000000010000"

mkdir -p "$EXEFS_DIR"
cp "$BUILD_DIR/main.npdm" "$EXEFS_DIR"
cp "$BUILD_DIR/subsdk9" "$EXEFS_DIR"
echo "Done copying to exefs folder"
cp -rf "./romfs" "$ROMFS_DIR"
echo "Done copying to romfs folder"