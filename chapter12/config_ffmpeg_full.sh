#!/bin/bash

NDK_HOME=/usr/local/src/android-ndk-r21e
SYSTEM=windows-x86_64
ARCH=aarch64
API=24
HOST=aarch64-linux-android

echo "config for FFmpeg"
./configure \
  --prefix=/usr/local/app_ffmpeg \
  --enable-shared \
  --disable-static \
  --disable-doc \
  --enable-cross-compile \
  --target-os=android \
  --arch=$ARCH \
  --cc=${NDK_HOME}/toolchains/llvm/prebuilt/$SYSTEM/bin/$HOST$API-clang \
  --cross-prefix=${NDK_HOME}/toolchains/$HOST-4.9/prebuilt/$SYSTEM/bin/$HOST- \
  --disable-ffmpeg \
  --disable-ffplay \
  --disable-ffprobe \
  --pkg-config-flags=--static \
  --pkg-config=pkg-config \
  --enable-libx264 \
  --enable-libfreetype \
  --enable-libmp3lame \
  --extra-cflags="-I/usr/local/app_lame/include" \
  --extra-ldflags="-L/usr/local/app_lame/lib" \
  --enable-gpl \
  --enable-nonfree

echo "config for FFmpeg completed"
