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

echo "config for mp3lame"
./configure \
  --host=$HOST \
  --target=$HOST \
  --enable-shared \
  --enable-static \
  --prefix=/usr/local/app_lame
echo "config for mp3lame completed"
