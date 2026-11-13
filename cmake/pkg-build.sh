#!/usr/bin/env bash
# 构建上游 libwebp（CMake）→ <out>/prefix：静态库 webp / webpdemux / webpmux。
set -euo pipefail
SRC=$1
OUT=$2
cmake -S "$SRC" -B "$OUT/bld" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$OUT/prefix" \
  -DBUILD_SHARED_LIBS=OFF \
  -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF \
  -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF \
  -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF \
  -DWEBP_BUILD_LIBWEBPMUX=OFF -DWEBP_BUILD_TESTS=OFF
cmake --build "$OUT/bld" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --install "$OUT/bld"
