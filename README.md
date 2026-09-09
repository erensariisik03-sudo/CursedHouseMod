# MultiPlayerMod - Android IL2CPP Memory Patcher

A native C++ shared library (`.so`) designed for Android ARMv7 (32-bit) Unity games running on the `IL2CPP` scripting backend. The project hooks into the application lifecycle using JNI and dynamically modifies memory segments of `libil2cpp.so` at runtime.

---

## 🚀 Features

* **JNI Integration:** Displays a custom Java Toast notification on library load (`JNI_OnLoad`) to confirm successful injection.
* **Dynamic Memory Patching:** Scans `/proc/self/maps` to retrieve the runtime base address of `libil2cpp.so`.
* **Security Bypass:** Uses `mprotect` to override memory protection flags (`PROT_READ | PROT_WRITE | PROT_EXEC`) prior to writing.
* **Character Limit Removal:** Natively patches Unity's `TMP_InputField::get_characterLimit` and `InputField::get_characterLimit` using Thumb-mode assembly instructions (`return 0`).

---

## 🛠️ Tech Stack & Prerequisites

* **Target Architecture:** `armeabi-v7a` (ARM32)
* **Language:** C++17 / NDK
* **Build System:** CMake & GitHub Actions
* **Target Engine:** Unity (IL2CPP)

---

## ⚙️ Building with GitHub Actions

This repository includes an automated workflow. Every commit pushed to `main` triggers a build using Android NDK r16b / r21 to generate `libmultiplayermod.so`.

### Manual Build via NDK

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=armeabi-v7a \
      -DANDROID_PLATFORM=android-21 ..
make
📝 Technical Overview
Instead of function hooking via heavy third-party frameworks, the patcher overwrites target RVA offsets directly with ARM32 Thumb opcodes:

Plaintext
MOVS R0, #0   ; 0x00 0x20
BX LR         ; 0x70 0x47
This forces get_characterLimit to immediately return 0 (unlimited input), bypassing UI input field restrictions across all standard Unity chat/input controls.

⚠️ Disclaimer
This project is created strictly for educational and research purposes to demonstrate Android memory manipulation, reverse engineering, and JNI dynamic library loading.
