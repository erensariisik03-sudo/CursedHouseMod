# CursedHouse Native Keyboard v1

This revision stops trying to defeat Unity's `TouchScreenKeyboard.characterLimit` and instead bypasses `TouchScreenKeyboard` entirely.

## Runtime flow

```text
Game TMP_InputField / InputField
          |
          | ActivateInputFieldInternal()
          v
NativeKeyboardBridge
          |
          +--> Android EditText (1x1, invisible)
          |
          +--> Android system IME
          |
          +--> native polling thread reads EditText text
          |
          v
Unity OnUpdateSelected()
          |
          v
TMP_InputField.text / InputField.text
```

The native EditText has no `InputFilter` and no maximum-length setting. The game never receives an Android `TouchScreenKeyboard` object from the intercepted activation path.

## Dump RVAs used

### TMPro.TMP_InputField
- `ActivateInputFieldInternal` = `0x34AE794`
- `OnUpdateSelected` = `0x34B2634`
- `DeactivateInputField` = `0x34ACC18`
- `OnDeselect` = `0x34B7480`
- `OnSubmit` = `0x34B74B0`
- `UpdateTouchKeyboardFromEditChanges` = `0x34B1A68`
- `get_text` = `0x34A92A0`
- `set_text` = `0x34A92A8`

### UnityEngine.UI.InputField
- `ActivateInputFieldInternal` = `0x3944DC0`
- `OnUpdateSelected` = `0x3948160`
- `DeactivateInputField` = `0x3943C10`
- `OnDeselect` = `0x394C440`
- `OnSubmit` = `0x394C464`
- `UpdateTouchKeyboardFromEditChanges` = `0x3947EFC`
- `get_text` = `0x3941B18`
- `set_text` = `0x3941B20`

## ARM32 address calculation

For every function RVA:

```text
runtime = libil2cpp_load_bias + RVA
runtime |= 1   // Thumb on armeabi-v7a
```

There is intentionally no `-0x10000` adjustment.

## Why this version is different

The previous versions kept the Unity keyboard path alive and attempted to alter the `characterLimit` parameter. This version does not hook `TouchScreenKeyboard` at all. It intercepts the input-field activation call, keeps Unity's normal selection/send lifecycle by calling the original method, and immediately moves Android IME focus to our native `EditText`. While our bridge is active, `UpdateTouchKeyboardFromEditChanges()` is blocked for that field so Unity does not push its limited keyboard state back to Android. The Unity-side `m_CharacterLimit` field is also forced to `0` before text is written back, so the field itself does not clamp the returned text.

Every 30 ms a native thread reads the Android EditText. The pending text is applied from the input field's `OnUpdateSelected()` callback, which runs on the Unity/player update path.

## Important limitations of v1

1. The native keyboard's IME action button is not wired to a Java `OnEditorActionListener` in this pure-`.so` build. The normal game Send/Submit path is supported through the hooked `OnSubmit()` methods.
2. If the game's particular chat UI does not call `OnUpdateSelected()` after our activation is intercepted, the text bridge will need a per-frame Unity update hook instead. The log output will make this obvious.
3. The library expects `MSHookFunction` to be provided by the same runtime/injector used by the earlier project.

## Logcat test

```bash
adb logcat -c
adb logcat -s NativeKeyboard:V
```

On tapping a chat field you should see:

```text
TMP_InputField activation intercepted: ...
Native Android keyboard acildi. field=... type=TMP_InputField initialLen=...
```

While typing, if needed for diagnostics, add logging around `g_pendingText` in `KeyboardPollThread`.

## Build

```bash
./build.sh
```

For a 32-bit target:

```bash
./build.sh armeabi-v7a
```

Output:

```text
build/libcursedhouse_native_keyboard.so
```
