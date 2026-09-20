#!/usr/bin/env bash
set -euo pipefail

rm -rf build
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_ABI=armeabi-v7a \
  -DANDROID_PLATFORM=android-21

cmake --build build --parallel

echo "SO: build/libcursedhouse_chat.so"
