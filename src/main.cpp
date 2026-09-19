#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <cstdint>
#include "substrate.h"

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// -----------------------------------------------------------------------------
// Unity IL2CPP RVAs from dump.cs (32-bit / armeabi-v7a build).
// IMPORTANT: These are RVAs from the IL2CPP image, so do NOT subtract 0x10000.
// The runtime address is: module_load_bias + RVA, with +1 for ARM/Thumb.
// -----------------------------------------------------------------------------
static constexpr uintptr_t kTouchScreenKeyboard_SetCharacterLimit_RVA = 0x3598790;
static constexpr uintptr_t kTMP_InputField_SetCharacterLimit_RVA       = 0x34AA4D0;
static constexpr uintptr_t kInputField_SetCharacterLimit_RVA            = 0x3942DE4;

// Original functions filled in by MSHookFunction.
using SetCharacterLimitFn = void (*)(void* instance, int value);

SetCharacterLimitFn orig_Keyboard_setLimit = nullptr;
SetCharacterLimitFn orig_TMP_setLimit      = nullptr;
SetCharacterLimitFn orig_InputField_setLimit = nullptr;

// -----------------------------------------------------------------------------
// Hook implementations: Unity uses 0 to mean "no character limit" for these
// InputField characterLimit properties.
// -----------------------------------------------------------------------------
void my_Keyboard_setLimit(void* instance, int value) {
    (void)value;
    if (orig_Keyboard_setLimit != nullptr) {
        LOGI("TouchScreenKeyboard.set_characterLimit(%d) -> 0", value);
        orig_Keyboard_setLimit(instance, 0);
    }
}

void my_TMP_setLimit(void* instance, int value) {
    (void)value;
    if (orig_TMP_setLimit != nullptr) {
        LOGI("TMP_InputField.set_characterLimit(%d) -> 0", value);
        orig_TMP_setLimit(instance, 0);
    }
}

void my_InputField_setLimit(void* instance, int value) {
    (void)value;
    if (orig_InputField_setLimit != nullptr) {
        LOGI("InputField.set_characterLimit(%d) -> 0", value);
        orig_InputField_setLimit(instance, 0);
    }
}

// -----------------------------------------------------------------------------
// Resolve the Substrate-compatible hook function at runtime. This avoids a
// hard undefined-symbol dependency that can make the .so fail to load.
// -----------------------------------------------------------------------------
MSHookFunction_t ResolveHookFunction() {
    return ResolveMSHookFunction();
}

// -----------------------------------------------------------------------------
// Find the ELF load bias for a loaded shared library.
//
// /proc/self/maps may show the first executable mapping at file offset 0x10000
// (or another non-zero offset). Using that address directly as the base and
// subtracting 0x10000 is not generally valid for a dump.cs RVA. Instead we
// prefer the mapping whose file offset is 0 and use its start address as the
// load bias. If an offset-0 mapping is not present, we fall back to start-offset.
// -----------------------------------------------------------------------------
uintptr_t GetModuleLoadBias(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        LOGE("/proc/self/maps acilamadi");
        return 0;
    }

    uintptr_t fallbackBias = 0;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name) == nullptr) {
            continue;
        }

        uintptr_t start = 0;
        uintptr_t end = 0;
        uintptr_t fileOffset = 0;
        char perms[5] = {0};

        int parsed = sscanf(
            line,
            "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR,
            &start,
            &end,
            perms,
            &fileOffset
        );

        if (parsed < 4) {
            continue;
        }

        if (fileOffset == 0) {
            fclose(f);
            return start;
        }

        if (fallbackBias == 0) {
            fallbackBias = start - fileOffset;
        }
    }

    fclose(f);
    return fallbackBias;
}

#if defined(__arm__)
static inline uintptr_t MakeThumbAddress(uintptr_t address) {
    return address | static_cast<uintptr_t>(1);
}
#else
static inline uintptr_t MakeThumbAddress(uintptr_t address) {
    return address;
}
#endif

static inline uintptr_t RvaToHookAddress(uintptr_t loadBias, uintptr_t rva) {
    return MakeThumbAddress(loadBias + rva);
}

void InstallHook(
    MSHookFunction_t hookFunction,
    uintptr_t target,
    void* replacement,
    void** original,
    const char* label
) {
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction bulunamadi; %s hooklanamadi.", label);
        return;
    }

    LOGI("%s target = 0x%" PRIxPTR, label, target);
    hookFunction(reinterpret_cast<void*>(target), replacement, original);

    if (*original != nullptr) {
        LOGI("%s hook basarili.", label);
    } else {
        LOGE("%s hook sonrasi original pointer NULL.", label);
    }
}

void* hack_thread(void*) {
    LOGI("Hack thread baslatildi, libil2cpp.so bekleniyor...");

    uintptr_t il2cppLoadBias = 0;
    while (il2cppLoadBias == 0) {
        il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (il2cppLoadBias == 0) {
            sleep(1);
        }
    }

    LOGI("libil2cpp.so load bias: 0x%" PRIxPTR, il2cppLoadBias);
    LOGI("RVA duzeltmesi: -0x10000 YOK.");

    const uintptr_t keyboardHookAddr =
        RvaToHookAddress(il2cppLoadBias, kTouchScreenKeyboard_SetCharacterLimit_RVA);
    const uintptr_t tmpHookAddr =
        RvaToHookAddress(il2cppLoadBias, kTMP_InputField_SetCharacterLimit_RVA);
    const uintptr_t inputFieldHookAddr =
        RvaToHookAddress(il2cppLoadBias, kInputField_SetCharacterLimit_RVA);

    LOGI("TouchScreenKeyboard RVA = 0x%" PRIxPTR, kTouchScreenKeyboard_SetCharacterLimit_RVA);
    LOGI("TMP_InputField RVA      = 0x%" PRIxPTR, kTMP_InputField_SetCharacterLimit_RVA);
    LOGI("InputField RVA           = 0x%" PRIxPTR, kInputField_SetCharacterLimit_RVA);

    MSHookFunction_t hookFunction = ResolveHookFunction();
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction runtime'da bulunamadi. Hooklar kurulmayacak.");
        return nullptr;
    }

    InstallHook(
        hookFunction,
        keyboardHookAddr,
        reinterpret_cast<void*>(my_Keyboard_setLimit),
        reinterpret_cast<void**>(&orig_Keyboard_setLimit),
        "TouchScreenKeyboard.set_characterLimit"
    );

    InstallHook(
        hookFunction,
        tmpHookAddr,
        reinterpret_cast<void*>(my_TMP_setLimit),
        reinterpret_cast<void**>(&orig_TMP_setLimit),
        "TMP_InputField.set_characterLimit"
    );

    InstallHook(
        hookFunction,
        inputFieldHookAddr,
        reinterpret_cast<void*>(my_InputField_setLimit),
        reinterpret_cast<void**>(&orig_InputField_setLimit),
        "InputField.set_characterLimit"
    );

    return nullptr;
}

extern "C" {
    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
        (void)vm;
        (void)reserved;

        pthread_t ptid;
        int result = pthread_create(&ptid, nullptr, hack_thread, nullptr);
        if (result != 0) {
            LOGE("hack_thread olusturulamadi: %d", result);
        } else {
            pthread_detach(ptid);
        }

        return JNI_VERSION_1_6;
    }
}
