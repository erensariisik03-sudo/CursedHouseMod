# CursedHouseMod - chat-target v1

This variant targets the game's own `chat` object instead of relying primarily on Unity's global keyboard API.

## Dump-derived targets

From the supplied `dump.cs`:

- `chat` input field: `this + 0x18`
- `chat.OnEnable`: `0xF696A0`
- `chat.Update`: `0xF6A6CC`
- `TMP_InputField.m_CharacterLimit`: `0x114`
- `TMP_InputField.ActivateInputFieldInternal`: `0x34AE794`
- `TMP_InputField.UpdateTouchKeyboardFromEditChanges`: `0x34B1A68`

The runtime function target is computed as:

`load_bias + RVA`, then Thumb bit `+1` is applied on `armeabi-v7a`.

There is deliberately **no `-0x10000`** adjustment because these values are the RVA values from `dump.cs`, not the earlier Ghidra address representation.

## What the hook does

`chat.OnEnable` and `chat.Update` read:

`chat + 0x18 -> TMP_InputField*`

then:

`TMP_InputField* + 0x114 -> m_CharacterLimit`

and force that integer to `0`.

The two TMP keyboard-edit methods are backup points that clear the same field immediately around the keyboard synchronization path.

## Runtime logging

Use:

```sh
adb logcat -c
adb logcat -s CursedHouseChat:V
```

Useful messages include:

```text
chat.OnEnable
chat.Update
chat.inputField=... m_CharacterLimit X -> 0
TMP_InputField=... ActivateInputFieldInternal: m_CharacterLimit X -> 0
TMP_InputField=... UpdateTouchKeyboardFromEditChanges: X -> 0
```

## Build

The GitHub Actions workflow builds `armeabi-v7a` with Android NDK `30.0.16248370`.

The output is:

`build/libmultiplayermod.so`
