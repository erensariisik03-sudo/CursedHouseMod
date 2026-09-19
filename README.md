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

The source targets the 32-bit `armeabi-v7a` build. The addresses in `dump_dosyasi.cs`
are treated as IL2CPP RVAs and are added to the ELF load bias detected from
`/proc/self/maps`. No fixed `0x10000` subtraction is performed. On ARM32 the
target address is adjusted to Thumb mode (`+1`).

Current dump.cs RVAs:

- `TouchScreenKeyboard.set_characterLimit` = `0x3598790`
- `TMP_InputField.set_characterLimit` = `0x34AA4D0`
- `UnityEngine.UI.InputField.set_characterLimit` = `0x3942DE4`

The hook implementation resolves `MSHookFunction` with `dlsym()` at runtime, so
the generated library does not require a direct undefined symbol at load time.
