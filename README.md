# multiplayermod — Keyboard Limit Revize V3

Android NDK + CMake project that builds `libmultiplayermod.so`.

## What changed in V3

This revision keeps the original 32-bit ARM32/Thumb RVA handling and strengthens the keyboard-limit path in four layers:

1. `TMP_InputField` and legacy `UnityEngine.UI.InputField` have their stored `m_CharacterLimit` cleared before the keyboard is activated.
2. The same stored field limit is cleared again from `UpdateTouchKeyboardFromEditChanges()`, which is the path Unity uses to synchronize the edited field back to the native keyboard after changes.
3. `TouchScreenKeyboard` is hooked at both the managed creation helpers and the lower-level `_Injected` binding, so `characterLimit` is forced to `0` at the point the native keyboard receives it.
4. The hidden IL2CPP `MethodInfo*` argument is explicitly present in the hook signatures. This avoids the previous mismatch where the hook prototypes did not fully match the generated IL2CPP method ABI.

Unity documents `TouchScreenKeyboard.characterLimit == 0` as unlimited input on Android/iOS.

## RVAs used from dump_dosyasi.cs

### TouchScreenKeyboard
- `.ctor` = `0x3597938`
- `TouchScreenKeyboard_InternalConstructorHelper` = `0x3597A90`
- `TouchScreenKeyboard_InternalConstructorHelper_Injected` = `0x3597D28`
- `Open` = `0x3597F84`
- `set_characterLimit` = `0x3598790`
- `set_characterLimit_Injected` = `0x3598804`

### TMP_InputField
- `set_characterLimit` = `0x34AA4D0`
- `ActivateInputFieldInternal` = `0x34AE794`
- `UpdateTouchKeyboardFromEditChanges` = `0x34B1A68`
- `m_CharacterLimit` offset = `0x114`

### UnityEngine.UI.InputField
- `set_characterLimit` = `0x3942DE4`
- `ActivateInputFieldInternal` = `0x3944DC0`
- `UpdateTouchKeyboardFromEditChanges` = `0x3947EFC`
- `m_CharacterLimit` offset = `0xDC`

## ARM32 address handling

The project is for `armeabi-v7a`.

Runtime hook address:

```text
loadBias + RVA
then Thumb bit (+1)
```

There is intentionally no `-0x10000` adjustment.

Field offsets such as `0x114` and `0xDC` are data offsets, so no `+1` is applied to them.

## Build

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

Or:

```bash
./build.sh
```

The output is:

```text
build/libmultiplayermod.so
```

## Important runtime dependency

`include/substrate.h` only declares `MSHookFunction`; it does not contain the Substrate implementation. The runtime/injector must expose `MSHookFunction`.

## Test logging

The library writes diagnostic messages with the Android log tag:

```text
ModMenu
```

For the keyboard test, look for lines such as:

```text
TouchScreenKeyboard.Open limit=... -> 0
TouchScreenKeyboard.InternalConstructorHelper limit=... -> 0
TouchScreenKeyboard.InternalConstructorHelper_Injected limit=... -> 0
TouchScreenKeyboard.set_characterLimit_Injected(...) -> 0
TMP_InputField.ActivateInputFieldInternal m_CharacterLimit=... -> 0
```

and the corresponding `InputField` lines.

## Note about the legacy InputField hard ceiling

The supplied dump also contains a legacy `InputField` constant:

```text
k_MaxTextLength = 16382
```

That is separate from the configurable `m_CharacterLimit`. The V3 changes remove the normal character-limit path and the native keyboard limit. If the game's chat control is the legacy `InputField` and you specifically need more than 16,382 characters in that exact control, the next step would be a separate targeted patch for that hard-coded Unity ceiling. TMP fields do not show that same `k_MaxTextLength` constant in the supplied dump.
