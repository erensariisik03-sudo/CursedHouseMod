# multiplayermod

Android NDK + CMake project that builds `libmultiplayermod.so`.

## Project layout

```text
multiplayermod/
├── .github/
│   └── workflows/
│       └── build.yml
├── include/
│   └── substrate.h
├── src/
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```

## Local build

Install Android SDK/NDK and run:

```bash
cmake -S . -B build -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake ^
  -DANDROID_ABI=armeabi-v7a ^
  -DANDROID_PLATFORM=android-21 ^
  -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON

cmake --build build --parallel
```

The output is:

```text
build/libmultiplayermod.so
```

## GitHub Actions

Every push, pull request, or manual workflow run builds the configured ABI and
uploads the `.so` as a workflow artifact.

The current workflow pins Android NDK `30.0.16248370` and uses CMake/Ninja.

## Important runtime dependency

`include/substrate.h` only declares `MSHookFunction`; it does **not** contain
the Substrate implementation. The generated `.so` therefore expects that symbol
to be supplied by the runtime environment.

The source currently uses a Thumb-mode address (`base + offset + 1`), so
`armeabi-v7a` is the default ABI. Do not enable `arm64-v8a` until the offsets and
calling convention have been verified for AArch64.
