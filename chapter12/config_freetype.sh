#!/bin/bash

NDK_HOME=/usr/local/src/android-ndk-r21e
SYSTEM=windows-x86_64
HOST=aarch64-linux-android
API=24
TOOLCHAIN=${NDK_HOME}/toolchains/llvm/prebuilt/$SYSTEM

export AR=$TOOLCHAIN/bin/$HOST-ar
export AS=$TOOLCHAIN/bin/$HOST-as
export LD=$TOOLCHAIN/bin/$HOST-ld
export RANLIB=$TOOLCHAIN/bin/$HOST-ranlib
export STRIP=$TOOLCHAIN/bin/$HOST-strip
export CC=$TOOLCHAIN/bin/$HOST$API-clang

echo "config for freetype"
./configure \
  --host=$HOST \
  --target=$HOST \
  --enable-shared \
  --enable-static \
  --with-bzip2=no \
  --prefix=/usr/local/app_freetype
echo "config for freetype completed"
