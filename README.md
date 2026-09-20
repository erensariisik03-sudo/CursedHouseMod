# CursedHouseMod — Native Keyboard V2

Bu sürüm eski Unity Android klavyesini **gerçekten bloke eder** ve yerine native Android `EditText` üzerinden sistem klavyesini açar.

## Yeni akış

```text
Unity TMP_InputField / InputField
          |
          v
ActivateInputFieldInternal
          |
          +----> Unity kendi aktivasyon durumunu korur
          |
          +----> TouchScreenKeyboard.InternalConstructorHelper
                    |
                    X  ORIGINAL NATIVE KEYBOARD BLOCKED
                    |
                    v
              Native Android EditText
                    |
                    v
               Android IME
```

Önemli fark: önceki sürümde `ActivateInputFieldInternal` sonrasında Unity'nin kendi klavye oluşturma yolu engellenmeden kalabiliyordu. V2'de `TouchScreenKeyboard.InternalConstructorHelper` ve `_Injected` çağrıları hook'lanıyor ve **orijinal fonksiyonlar çağrılmıyor**.

## Kullanılan adresler

- `TouchScreenKeyboard.InternalConstructorHelper` = `0x3597A90`
- `TouchScreenKeyboard.InternalConstructorHelper_Injected` = `0x3597D28`
- `TMP_InputField.ActivateInputFieldInternal` = `0x34AE794`
- `InputField.ActivateInputFieldInternal` = `0x3944DC0`
- `TMP_InputField.OnUpdateSelected` = `0x34B2634`
- `InputField.OnUpdateSelected` = `0x3948160`
- `TMP_InputField.UpdateTouchKeyboardFromEditChanges` = `0x34B1A68`
- `InputField.UpdateTouchKeyboardFromEditChanges` = `0x3947EFC`
- `TMP_InputField.DeactivateInputField` = `0x34ACC18`
- `InputField.DeactivateInputField` = `0x3943C10`
- `TMP_InputField.OnDeselect` = `0x34B7480`
- `InputField.OnDeselect` = `0x394C440`
- `TMP_InputField.OnSubmit` = `0x34B74B0`
- `InputField.OnSubmit` = `0x394C464`

Adresleme:

```text
runtime = libil2cpp loadBias + RVA + Thumb(+1)
```

`-0x10000` kullanılmaz.

## Beklenen loglar

```text
NativeKeyboard mod thread baslatildi...
libil2cpp.so load bias = ...
Hook TouchScreenKeyboard.InternalConstructorHelper [BLOCK] ...
Hook TouchScreenKeyboard.InternalConstructorHelper_Injected [BLOCK] ...
Hook TMP_InputField.ActivateInputFieldInternal ...
```

Sohbet kutusuna dokununca:

```text
TMP_InputField activation intercepted: ...
BLOCK TouchScreenKeyboard.InternalConstructorHelper: ...
Native Android keyboard acildi (old TouchScreenKeyboard BLOCKED). ...
```

Bunları görüyorsan eski Unity klavyesinin native oluşturma çağrısı artık çalışmıyor demektir.

## Build

```bash
./build.sh
```

Çıktı:

```text
build/libcursedhouse_native_keyboard.so
```

GitHub Actions da yalnızca gerekli build dosyalarıyla `armeabi-v7a` üretir.
