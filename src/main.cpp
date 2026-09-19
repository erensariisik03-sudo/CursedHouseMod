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

// 32-bit armeabi-v7a IL2CPP RVAs taken directly from dump.cs.
// Do NOT subtract 0x10000. Runtime target = ELF load bias + RVA, Thumb (+1).
static constexpr uintptr_t kTSK_Ctor_RVA                       = 0x3597938;
static constexpr uintptr_t kTSK_Open_RVA                       = 0x3597F84;
static constexpr uintptr_t kTSK_InternalConstructorHelper_RVA  = 0x3597A90;
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA          = 0x3598790;
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA          = 0x34AA4D0;
static constexpr uintptr_t kInputField_SetCharacterLimit_RVA   = 0x3942DE4;
// More direct target: these methods are entered immediately before Unity creates
// the TouchScreenKeyboard for the selected field. We clear the field's stored
// characterLimit before the keyboard is constructed.
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x34AE794;
static constexpr uintptr_t kInputField_ActivateInputFieldInternal_RVA = 0x3944DC0;
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;
static constexpr uintptr_t kInputField_CharacterLimit_FieldOffset = 0xDC;

using SetCharacterLimitFn = void (*)(void* instance, int value);
using TSKCtorFn = void (*)(void* self, void* text, int keyboardType,
                           bool autocorrection, bool multiline, bool secure,
                           bool alert, void* textPlaceholder, int characterLimit);
using TSKOpenFn = void* (*)(void* text, int keyboardType,
                            bool autocorrection, bool multiline, bool secure,
                            bool alert, void* textPlaceholder, int characterLimit);

struct TSK_InternalConstructorHelperArguments {
    uint32_t keyboardType;
    uint32_t autocorrection;
    uint32_t multiline;
    uint32_t secure;
    uint32_t alert;
    int32_t characterLimit;
};

using TSKHelperFn = void* (*)(TSK_InternalConstructorHelperArguments* arguments,
                              void* text, void* textPlaceholder);
using ActivateInternalFn = void (*)(void* instance, void* methodInfo);

SetCharacterLimitFn orig_Keyboard_setLimit = nullptr;
SetCharacterLimitFn orig_TMP_setLimit = nullptr;
SetCharacterLimitFn orig_InputField_setLimit = nullptr;
TSKCtorFn orig_TSK_ctor = nullptr;
TSKOpenFn orig_TSK_open = nullptr;
TSKHelperFn orig_TSK_helper = nullptr;
ActivateInternalFn orig_TMP_activateInternal = nullptr;
ActivateInternalFn orig_InputField_activateInternal = nullptr;

// -----------------------------------------------------------------------------
// Existing field/property hooks.
// -----------------------------------------------------------------------------
void my_Keyboard_setLimit(void* instance, int value) {
    if (orig_Keyboard_setLimit != nullptr) {
        LOGI("TouchScreenKeyboard.set_characterLimit(%d) -> 0", value);
        orig_Keyboard_setLimit(instance, 0);
    }
}

void my_TMP_setLimit(void* instance, int value) {
    if (orig_TMP_setLimit != nullptr) {
        LOGI("TMP_InputField.set_characterLimit(%d) -> 0", value);
        orig_TMP_setLimit(instance, 0);
    }
}

void my_InputField_setLimit(void* instance, int value) {
    if (orig_InputField_setLimit != nullptr) {
        LOGI("InputField.set_characterLimit(%d) -> 0", value);
        orig_InputField_setLimit(instance, 0);
    }
}

// -----------------------------------------------------------------------------
// The important part for this game's input flow:
// Unity creates the native/on-screen keyboard with a characterLimit argument.
// Changing the later property setter does not necessarily change the limit that
// was already sent to the Android keyboard.  Force the constructor/Open/helper
// argument to 0 while the keyboard is being created.
// -----------------------------------------------------------------------------
void my_TSK_ctor(void* self, void* text, int keyboardType,
                 bool autocorrection, bool multiline, bool secure,
                 bool alert, void* textPlaceholder, int characterLimit) {
    LOGI("TouchScreenKeyboard::.ctor limit=%d -> 0", characterLimit);
    if (orig_TSK_ctor != nullptr) {
        orig_TSK_ctor(self, text, keyboardType, autocorrection, multiline,
                      secure, alert, textPlaceholder, 0);
    }
}

void* my_TSK_open(void* text, int keyboardType,
                  bool autocorrection, bool multiline, bool secure,
                  bool alert, void* textPlaceholder, int characterLimit) {
    LOGI("TouchScreenKeyboard.Open limit=%d -> 0", characterLimit);
    if (orig_TSK_open != nullptr) {
        return orig_TSK_open(text, keyboardType, autocorrection, multiline,
                             secure, alert, textPlaceholder, 0);
    }
    return nullptr;
}

void* my_TSK_helper(TSK_InternalConstructorHelperArguments* arguments,
                    void* text, void* textPlaceholder) {
    if (arguments != nullptr) {
        LOGI("TouchScreenKeyboard.InternalConstructorHelper limit=%d -> 0",
             arguments->characterLimit);
        arguments->characterLimit = 0;
    }
    if (orig_TSK_helper != nullptr) {
        return orig_TSK_helper(arguments, text, textPlaceholder);
    }
    return nullptr;
}

void my_TMP_activateInternal(void* instance, void* methodInfo) {
    if (instance != nullptr) {
        auto* limit = reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(instance) + kTMP_CharacterLimit_FieldOffset);
        LOGI("TMP_InputField opening keyboard: m_CharacterLimit=%d -> 0", *limit);
        *limit = 0;
    }

    if (orig_TMP_activateInternal != nullptr) {
        orig_TMP_activateInternal(instance, methodInfo);
    }
}

void my_InputField_activateInternal(void* instance, void* methodInfo) {
    if (instance != nullptr) {
        auto* limit = reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(instance) + kInputField_CharacterLimit_FieldOffset);
        LOGI("InputField opening keyboard: m_CharacterLimit=%d -> 0", *limit);
        *limit = 0;
    }

    if (orig_InputField_activateInternal != nullptr) {
        orig_InputField_activateInternal(instance, methodInfo);
    }
}

MSHookFunction_t ResolveHookFunction() {
    return ResolveMSHookFunction();
}

uintptr_t GetModuleLoadBias(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        LOGE("/proc/self/maps acilamadi");
        return 0;
    }

    uintptr_t fallbackBias = 0;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name) == nullptr) continue;

        uintptr_t start = 0;
        uintptr_t end = 0;
        uintptr_t fileOffset = 0;
        char perms[5] = {0};

        int parsed = sscanf(line,
                            "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR,
                            &start, &end, perms, &fileOffset);
        if (parsed < 4) continue;

        if (fileOffset == 0) {
            fclose(f);
            return start;
        }

        if (fallbackBias == 0 && start >= fileOffset) {
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

void InstallHook(MSHookFunction_t hookFunction, uintptr_t target, void* replacement,
                 void** original, const char* label) {
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
    for (;;) {
        il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (il2cppLoadBias != 0) break;
        sleep(1);
    }

    LOGI("libil2cpp.so load bias: 0x%" PRIxPTR, il2cppLoadBias);
    LOGI("dump.cs RVA kullaniliyor; -0x10000 YOK.");

    MSHookFunction_t hookFunction = ResolveHookFunction();
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction runtime'da bulunamadi. Hooklar kurulmayacak.");
        return nullptr;
    }

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTMP_ActivateInputFieldInternal_RVA),
                reinterpret_cast<void*>(my_TMP_activateInternal),
                reinterpret_cast<void**>(&orig_TMP_activateInternal),
                "TMP_InputField.ActivateInputFieldInternal");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kInputField_ActivateInputFieldInternal_RVA),
                reinterpret_cast<void*>(my_InputField_activateInternal),
                reinterpret_cast<void**>(&orig_InputField_activateInternal),
                "InputField.ActivateInputFieldInternal");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTSK_Ctor_RVA),
                reinterpret_cast<void*>(my_TSK_ctor),
                reinterpret_cast<void**>(&orig_TSK_ctor),
                "TouchScreenKeyboard::.ctor");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTSK_Open_RVA),
                reinterpret_cast<void*>(my_TSK_open),
                reinterpret_cast<void**>(&orig_TSK_open),
                "TouchScreenKeyboard.Open");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTSK_InternalConstructorHelper_RVA),
                reinterpret_cast<void*>(my_TSK_helper),
                reinterpret_cast<void**>(&orig_TSK_helper),
                "TouchScreenKeyboard.InternalConstructorHelper");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTSK_SetCharacterLimit_RVA),
                reinterpret_cast<void*>(my_Keyboard_setLimit),
                reinterpret_cast<void**>(&orig_Keyboard_setLimit),
                "TouchScreenKeyboard.set_characterLimit");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTMP_SetCharacterLimit_RVA),
                reinterpret_cast<void*>(my_TMP_setLimit),
                reinterpret_cast<void**>(&orig_TMP_setLimit),
                "TMP_InputField.set_characterLimit");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kInputField_SetCharacterLimit_RVA),
                reinterpret_cast<void*>(my_InputField_setLimit),
                reinterpret_cast<void**>(&orig_InputField_setLimit),
                "InputField.set_characterLimit");

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
