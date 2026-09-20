# CursedHouseMod - chat-target v2 diagnostic

Bu sürümün amacı iki şeyi birbirinden ayırmaktır:

1. dump.cs RVA + 32-bit Thumb adres hesabı doğru mu?
2. Runtime'da gerçekten bir native hook backend'i var mı?

Önemli: Önceki logdaki `MSHookFunction bulunamadi` mesajı nedeniyle hiçbir hook kurulmamıştı. Bu durumda limitin değişmemesi beklenir; adreslerin doğru/yanlış olduğunu o sürüm test edemiyordu.

## Hedefler

- `chat.OnEnable` = `0xF696A0`
- `chat.sendMessage` = `0xF69758`
- `chat.Update` = `0xF6A6CC`
- `chat.inputField` = `this + 0x18`
- `TMP_InputField.m_CharacterLimit` = `0x114`
- `TMP_InputField.set_characterLimit` = `0x34AA4D0`
- `TMP_InputField.SetText(string)` = `0x34BD1AC`
- `TMP_InputField.Append(string)` = `0x34B47DC`
- `TMP_InputField.Append(char)` = `0x34B4884`
- `TMP_InputField.Insert(char)` = `0x34B4CF8`
- `TMP_InputField.ActivateInputFieldInternal` = `0x34AE794`
- `TMP_InputField.UpdateTouchKeyboardFromEditChanges` = `0x34B1A68`
- `TouchScreenKeyboard.set_characterLimit` = `0x3598790`
- `UnityEngine.UI.InputField.set_characterLimit` = `0x3942DE4`
- `UnityEngine.UI.InputField.ActivateInputFieldInternal` = `0x3944DC0`

Fonksiyon adresleri `loadBias + RVA` ile hesaplanır ve `armeabi-v7a` için Thumb biti `+1` olarak eklenir. Field offsetlerine `+1` eklenmez.

## Yeni loglar

```sh
adb logcat -c
adb logcat -s CursedHouseChat:V
```

Önce şunları görmelisin:

```text
libil2cpp load bias=...
MAP ... libil2cpp.so
RVA CHECK chat.OnEnable ...
BYTES chat.OnEnable: ...
...
HOOK BACKEND ...
```

### A) `HOOK BACKEND YOK`
Bu durumda native hook motoru runtime'da yoktur. `libmultiplayermod.so` kendi başına `MSHookFunction` üretmiyor; Substrate veya Dobby gibi bir hook backend'i gerekir.

### B) `TARGET INVALID`
Hedef adres `libil2cpp.so` executable mapping'i içinde değilse load-bias/RVA eşleşmesini tekrar incelemek gerekir.

### C) `HOOK RESULT ... original=...`
Hook kurulmuştur. Bundan sonra uygulamada chat açılırken şu çağrıları görmeliyiz:

```text
CALL chat.OnEnable
CALL chat.Update
CALL TMP_InputField.set_characterLimit
CALL TMP_InputField.ActivateInputFieldInternal
CALL TMP_InputField.UpdateTouchKeyboardFromEditChanges
CALL TMP_InputField.Append(string)
CALL TMP_InputField.Append(char)
```

`chat.Update` logunda `chat+0x18` ile elde edilen gerçek `TMP_InputField*` ve `m_CharacterLimit` değeri de yazdırılır.

## Not

Bu sürüm, çalışan hook backend'i bulunmadığında yanlış adrese native patch uygulamaz. Önce tanı koyar; bu nedenle `MSHookFunction` yoksa limitin kaldırılması yine gerçekleşmez.
