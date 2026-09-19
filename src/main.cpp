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
// 32-bit armeabi-v7a IL2CPP RVAs taken directly from dump.cs.
// Runtime target = libil2cpp.so load bias + RVA, then Thumb bit (+1).
// There is NO -0x10000 adjustment here.
// -----------------------------------------------------------------------------

// TouchScreenKeyboard creation / limit path.
static constexpr uintptr_t kTSK_Ctor_RVA                              = 0x3597938;
static constexpr uintptr_t kTSK_InternalConstructorHelper_RVA         = 0x3597A90;
static constexpr uintptr_t kTSK_InternalConstructorHelper_Injected_RVA = 0x3597D28;
static constexpr uintptr_t kTSK_Open_RVA                              = 0x3597F84;
static constexpr uintptr_t kTSK_SetCharacterLimit_RVA                 = 0x3598790;
static constexpr uintptr_t kTSK_SetCharacterLimit_Injected_RVA        = 0x3598804;

// TMP_InputField.
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA                 = 0x34AA4D0;
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA         = 0x34AE794;
static constexpr uintptr_t kTMP_UpdateTouchKeyboardFromEditChanges_RVA = 0x34B1A68;
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset            = 0x114;

// Legacy UnityEngine.UI.InputField.
static constexpr uintptr_t kInputField_SetCharacterLimit_RVA                 = 0x3942DE4;
static constexpr uintptr_t kInputField_ActivateInputFieldInternal_RVA         = 0x3944DC0;
static constexpr uintptr_t kInputField_UpdateTouchKeyboardFromEditChanges_RVA = 0x3947EFC;
static constexpr uintptr_t kInputField_CharacterLimit_FieldOffset            = 0xDC;

// Unity's TouchScreenKeyboard documentation defines characterLimit == 0 as
// unlimited, so every path that carries the keyboard limit is forced to 0.
static constexpr int kUnlimitedCharacterLimit = 0;

using MethodInfoPtr = void*;

using SetCharacterLimitFn = void (*)(void* instance, int value, MethodInfoPtr methodInfo);

using TSKCtorFn = void (*)(void* self,
                           void* text,
                           int keyboardType,
                           bool autocorrection,
                           bool multiline,
                           bool secure,
                           bool alert,
                           void* textPlaceholder,
                           int characterLimit,
                           MethodInfoPtr methodInfo);

using TSKOpenFn = void* (*)(void* text,
                            int keyboardType,
                            bool autocorrection,
                            bool multiline,
                            bool secure,
                            bool alert,
                            void* textPlaceholder,
                            int characterLimit,
                            MethodInfoPtr methodInfo);

struct TSK_InternalConstructorHelperArguments {
    uint32_t keyboardType;      // 0x00
    uint32_t autocorrection;    // 0x04
    uint32_t multiline;         // 0x08
    uint32_t secure;            // 0x0C
    uint32_t alert;             // 0x10
    int32_t characterLimit;     // 0x14
};

using TSKHelperFn = void* (*)(TSK_InternalConstructorHelperArguments* arguments,
                              void* text,
                              void* textPlaceholder,
                              MethodInfoPtr methodInfo);

using TSKHelperInjectedFn = void* (*)(TSK_InternalConstructorHelperArguments* arguments,
                                      void* text,
                                      void* textPlaceholder,
                                      MethodInfoPtr methodInfo);

using ActivateInternalFn = void (*)(void* instance, MethodInfoPtr methodInfo);
using UpdateTouchKeyboardFn = void (*)(void* instance, MethodInfoPtr methodInfo);

// Originals.
SetCharacterLimitFn orig_Keyboard_setLimit = nullptr;
SetCharacterLimitFn orig_TMP_setLimit = nullptr;
SetCharacterLimitFn orig_InputField_setLimit = nullptr;

TSKCtorFn orig_TSK_ctor = nullptr;
TSKOpenFn orig_TSK_open = nullptr;
TSKHelperFn orig_TSK_helper = nullptr;
TSKHelperInjectedFn orig_TSK_helperInjected = nullptr;
SetCharacterLimitFn orig_TSK_setLimitInjected = nullptr;

ActivateInternalFn orig_TMP_activateInternal = nullptr;
ActivateInternalFn orig_InputField_activateInternal = nullptr;

UpdateTouchKeyboardFn orig_TMP_updateTouchKeyboard = nullptr;
UpdateTouchKeyboardFn orig_InputField_updateTouchKeyboard = nullptr;

// -----------------------------------------------------------------------------
// Helpers.
// -----------------------------------------------------------------------------

static inline void ForceFieldCharacterLimit(void* instance,
                                            uintptr_t fieldOffset,
                                            const char* label) {
    if (instance == nullptr) {
        return;
    }

    auto* limit = reinterpret_cast<int*>(
        reinterpret_cast<uintptr_t>(instance) + fieldOffset);

    const int oldLimit = *limit;
    if (oldLimit != kUnlimitedCharacterLimit) {
        LOGI("%s m_CharacterLimit=%d -> 0", label, oldLimit);
        *limit = kUnlimitedCharacterLimit;
    }
}

// -----------------------------------------------------------------------------
// Managed InputField property hooks.
// These are retained as a second line of defence in case the game writes a
// characterLimit again after activation.
// -----------------------------------------------------------------------------

void my_Keyboard_setLimit(void* instance, int value, MethodInfoPtr methodInfo) {
    if (orig_Keyboard_setLimit != nullptr) {
        LOGI("TouchScreenKeyboard.set_characterLimit(%d) -> 0", value);
        orig_Keyboard_setLimit(instance, kUnlimitedCharacterLimit, methodInfo);
    }
}

void my_TMP_setLimit(void* instance, int value, MethodInfoPtr methodInfo) {
    if (orig_TMP_setLimit != nullptr) {
        LOGI("TMP_InputField.set_characterLimit(%d) -> 0", value);
        orig_TMP_setLimit(instance, kUnlimitedCharacterLimit, methodInfo);
    }
}

void my_InputField_setLimit(void* instance, int value, MethodInfoPtr methodInfo) {
    if (orig_InputField_setLimit != nullptr) {
        LOGI("InputField.set_characterLimit(%d) -> 0", value);
        orig_InputField_setLimit(instance, kUnlimitedCharacterLimit, methodInfo);
    }
}

// -----------------------------------------------------------------------------
// TouchScreenKeyboard constructors / Open.
// The hidden IL2CPP MethodInfo* parameter is explicitly present in every hook.
// -----------------------------------------------------------------------------

void my_TSK_ctor(void* self,
                 void* text,
                 int keyboardType,
                 bool autocorrection,
                 bool multiline,
                 bool secure,
                 bool alert,
                 void* textPlaceholder,
                 int characterLimit,
                 MethodInfoPtr methodInfo) {
    LOGI("TouchScreenKeyboard::.ctor limit=%d -> 0", characterLimit);

    if (orig_TSK_ctor != nullptr) {
        orig_TSK_ctor(self,
                      text,
                      keyboardType,
                      autocorrection,
                      multiline,
                      secure,
                      alert,
                      textPlaceholder,
                      kUnlimitedCharacterLimit,
                      methodInfo);
    }
}

void* my_TSK_open(void* text,
                  int keyboardType,
                  bool autocorrection,
                  bool multiline,
                  bool secure,
                  bool alert,
                  void* textPlaceholder,
                  int characterLimit,
                  MethodInfoPtr methodInfo) {
    LOGI("TouchScreenKeyboard.Open limit=%d -> 0", characterLimit);

    if (orig_TSK_open != nullptr) {
        return orig_TSK_open(text,
                             keyboardType,
                             autocorrection,
                             multiline,
                             secure,
                             alert,
                             textPlaceholder,
                             kUnlimitedCharacterLimit,
                             methodInfo);
    }

    return nullptr;
}

void* my_TSK_helper(TSK_InternalConstructorHelperArguments* arguments,
                    void* text,
                    void* textPlaceholder,
                    MethodInfoPtr methodInfo) {
    if (arguments != nullptr) {
        LOGI("TouchScreenKeyboard.InternalConstructorHelper limit=%d -> 0",
             arguments->characterLimit);
        arguments->characterLimit = kUnlimitedCharacterLimit;
    }

    if (orig_TSK_helper != nullptr) {
        return orig_TSK_helper(arguments, text, textPlaceholder, methodInfo);
    }

    return nullptr;
}

// Hook the lower-level injected helper too. This catches the actual binding
// call used by the managed wrapper before the native Android keyboard is built.
void* my_TSK_helperInjected(TSK_InternalConstructorHelperArguments* arguments,
                            void* text,
                            void* textPlaceholder,
                            MethodInfoPtr methodInfo) {
    if (arguments != nullptr) {
        LOGI("TouchScreenKeyboard.InternalConstructorHelper_Injected limit=%d -> 0",
             arguments->characterLimit);
        arguments->characterLimit = kUnlimitedCharacterLimit;
    }

    if (orig_TSK_helperInjected != nullptr) {
        return orig_TSK_helperInjected(arguments,
                                       text,
                                       textPlaceholder,
                                       methodInfo);
    }

    return nullptr;
}

// Directly catch the lowest-level native binding used by
// TouchScreenKeyboard.characterLimit.
void my_TSK_setLimitInjected(void* nativeSelf,
                             int value,
                             MethodInfoPtr methodInfo) {
    if (orig_TSK_setLimitInjected != nullptr) {
        LOGI("TouchScreenKeyboard.set_characterLimit_Injected(%d) -> 0", value);
        orig_TSK_setLimitInjected(nativeSelf,
                                   kUnlimitedCharacterLimit,
                                   methodInfo);
    }
}

// -----------------------------------------------------------------------------
// InputField activation hooks.
// We zero the serialized field BEFORE Unity creates the keyboard.
// -----------------------------------------------------------------------------

void my_TMP_activateInternal(void* instance, MethodInfoPtr methodInfo) {
    ForceFieldCharacterLimit(instance,
                             kTMP_CharacterLimit_FieldOffset,
                             "TMP_InputField.ActivateInputFieldInternal");

    if (orig_TMP_activateInternal != nullptr) {
        orig_TMP_activateInternal(instance, methodInfo);
    }
}

void my_InputField_activateInternal(void* instance, MethodInfoPtr methodInfo) {
    ForceFieldCharacterLimit(instance,
                             kInputField_CharacterLimit_FieldOffset,
                             "InputField.ActivateInputFieldInternal");

    if (orig_InputField_activateInternal != nullptr) {
        orig_InputField_activateInternal(instance, methodInfo);
    }
}

// -----------------------------------------------------------------------------
// Unity synchronizes the field back into TouchScreenKeyboard after text edits.
// If the game restores its original limit here, kill it again immediately
// before Unity performs that synchronization.
// -----------------------------------------------------------------------------

void my_TMP_updateTouchKeyboard(void* instance, MethodInfoPtr methodInfo) {
    ForceFieldCharacterLimit(instance,
                             kTMP_CharacterLimit_FieldOffset,
                             "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    if (orig_TMP_updateTouchKeyboard != nullptr) {
        orig_TMP_updateTouchKeyboard(instance, methodInfo);
    }
}

void my_InputField_updateTouchKeyboard(void* instance, MethodInfoPtr methodInfo) {
    ForceFieldCharacterLimit(instance,
                             kInputField_CharacterLimit_FieldOffset,
                             "InputField.UpdateTouchKeyboardFromEditChanges");

    if (orig_InputField_updateTouchKeyboard != nullptr) {
        orig_InputField_updateTouchKeyboard(instance, methodInfo);
    }
}

// -----------------------------------------------------------------------------
// Substrate resolver.
// -----------------------------------------------------------------------------

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
        if (strstr(line, name) == nullptr) {
            continue;
        }

        uintptr_t start = 0;
        uintptr_t end = 0;
        uintptr_t fileOffset = 0;
        char perms[5] = {0};

        int parsed = sscanf(line,
                            "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR,
                            &start,
                            &end,
                            perms,
                            &fileOffset);
        if (parsed < 4) {
            continue;
        }

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

void InstallHook(MSHookFunction_t hookFunction,
                 uintptr_t target,
                 void* replacement,
                 void** original,
                 const char* label) {
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction bulunamadi; %s hooklanamadi.", label);
        return;
    }

    LOGI("%s target = 0x%" PRIxPTR, label, target);
    hookFunction(reinterpret_cast<void*>(target), replacement, original);

    if (original != nullptr && *original != nullptr) {
        LOGI("%s hook basarili.", label);
    } else {
        LOGE("%s hook sonrasi original pointer NULL.", label);
    }
}

void* hack_thread(void*) {
    LOGI("Keyboard limit mod thread baslatildi, libil2cpp.so bekleniyor...");

    uintptr_t il2cppLoadBias = 0;
    for (;;) {
        il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (il2cppLoadBias != 0) {
            break;
        }
        sleep(1);
    }

    LOGI("libil2cpp.so load bias: 0x%" PRIxPTR, il2cppLoadBias);
    LOGI("dump.cs RVA kullaniliyor; -0x10000 YOK; ARM32 Thumb +1 aktif.");

    MSHookFunction_t hookFunction = ResolveHookFunction();
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction runtime'da bulunamadi. Hooklar kurulmayacak.");
        return nullptr;
    }

    // InputField field limit before keyboard creation.
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

    // Keep the limit cleared during post-edit keyboard synchronization.
    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTMP_UpdateTouchKeyboardFromEditChanges_RVA),
                reinterpret_cast<void*>(my_TMP_updateTouchKeyboard),
                reinterpret_cast<void**>(&orig_TMP_updateTouchKeyboard),
                "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kInputField_UpdateTouchKeyboardFromEditChanges_RVA),
                reinterpret_cast<void*>(my_InputField_updateTouchKeyboard),
                reinterpret_cast<void**>(&orig_InputField_updateTouchKeyboard),
                "InputField.UpdateTouchKeyboardFromEditChanges");

    // TouchScreenKeyboard creation path.
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
                RvaToHookAddress(il2cppLoadBias, kTSK_InternalConstructorHelper_Injected_RVA),
                reinterpret_cast<void*>(my_TSK_helperInjected),
                reinterpret_cast<void**>(&orig_TSK_helperInjected),
                "TouchScreenKeyboard.InternalConstructorHelper_Injected");

    // Low-level keyboard limit setter.
    InstallHook(hookFunction,
                RvaToHookAddress(il2cppLoadBias, kTSK_SetCharacterLimit_Injected_RVA),
                reinterpret_cast<void*>(my_TSK_setLimitInjected),
                reinterpret_cast<void**>(&orig_TSK_setLimitInjected),
                "TouchScreenKeyboard.set_characterLimit_Injected");

    // Managed/property hooks as additional protection.
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
    const int result = pthread_create(&ptid, nullptr, hack_thread, nullptr);
    if (result != 0) {
        LOGE("hack_thread olusturulamadi: %d", result);
    } else {
        pthread_detach(ptid);
    }

    return JNI_VERSION_1_6;
}
}
