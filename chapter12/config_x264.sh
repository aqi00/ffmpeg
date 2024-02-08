#!/bin/bash

NDK_HOME=/usr/local/src/android-ndk-r21e
SYSTEM=windows-x86_64
HOST=aarch64-linux-android
API=24
export CC=${NDK_HOME}/toolchains/llvm/prebuilt/$SYSTEM/bin/$HOST$API-clang

echo "config for x264"
./configure \
  --enable-shared \
  --enable-static \
  --prefix=/usr/local/app_x264 \
  --host=$HOST \
  --cross-prefix=${NDK_HOME}/toolchains/$HOST-4.9/prebuilt/$SYSTEM/bin/$HOST- \
  --enable-pic \
  --enable-strip \
  --extra-cflags="-fPIC"
echo "config for x264 completed"
