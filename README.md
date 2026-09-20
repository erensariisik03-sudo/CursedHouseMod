# CursedHouseMod - chat-target v4 ARM32 inline hook

Bu surum, onceki Dobby derleme hatasi nedeniyle harici Dobby/Substrate bagimliligini tamamen kaldirir.

## Neden v4?

CI logunda Dobby'nin ARM32 assembler kaynaklari `core/arch/Cpu.h` dosyasini bulamiyordu. Bu nedenle Dobby hedefinden vazgecilip proje icine minimal ARM32 inline-hook katmani eklendi.

## ARM32 adresleme

Dump.cs RVA'lari `loadBias + RVA` ile kullanilir.

Bu oyunun `libil2cpp.so` mapping'i `off=0x0` ile basladigi icin `-0x10000` uygulanmaz.

Diagnostic byte'lar ARM-mode prologue gosteriyor, ornegin `chat.Update`:

`10 40 2D E9 00 40 A0 E1 ...`

Bu nedenle bu hedefte Thumb `+1` uygulanmaz.

## Kurulan hook

v4'te ilk test icin yalnizca:

- `chat.Update` RVA `0xF6A6CC`
- `chat.inputField` offset `0x18`
- `TMP_InputField.m_CharacterLimit` offset `0x114`

kullanilir.

`chat.Update` hook'u her cagrisinda `chat + 0x18` adresinden TMP_InputField pointer'ini okuyup `inputField + 0x114` alanini 0'a ceker.

Diger dump.cs hedefleri bu surumde tanisal olarak loglanabilir, fakat minimal ARM trampoline riskini azaltmak icin otomatik olarak patch edilmez.

## Build

GitHub Actions sadece `armeabi-v7a` uretir ve harici hook kutuphanesi cekmez.

```sh
adb logcat -c
adb logcat -s CursedHouseChat:V
```

Beklenen ilk satirlardan biri:

`HOOK BACKEND: builtin ARM32 inline hook`

ve devaminda:

`ArmHook result 0 for chat.Update`

`chat.Update hook AKTIF`

Ardindan sohbet ekranini acip su tur loglar gorulmelidir:

`CALL chat.Update this=...`

`CHAT INSTANCE ... inputFieldPtr=...`

`TMP OBJECT ... m_CharacterLimit=...`

`TMP OBJECT ... m_CharacterLimit X -> 0`

## Not

Minimal ARM trampoline iki adet 32-bit ARM instruction kopyalar. Bu nedenle v4 sadece baslangic talimatlari PC-relative olmayan `chat.Update` hedefini kullanir. Daha farkli hedeflere hook eklemek icin o fonksiyonun ilk talimatlarinin relocation ihtiyaci ayri ele alinmalidir.
