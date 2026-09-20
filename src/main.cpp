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

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// These are RVA values from the supplied dump.cs. They are NOT Ghidra file/image
// offsets, so there is no -0x10000 adjustment here.
static constexpr uintptr_t kChat_OnEnable_RVA = 0xF696A0;
static constexpr uintptr_t kChat_Update_RVA = 0xF6A6CC;

// chat class layout from dump.cs:
//   TMP_InputField inputField; // 0x18
static constexpr uintptr_t kChat_InputField_FieldOffset = 0x18;

// TMP_InputField layout from dump.cs:
//   int m_CharacterLimit; // 0x114
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;

// Additional TMP methods from the same dump. These are used as backup points so
// the limit is also cleared immediately before/while Unity updates the native
// keyboard from edit changes.
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x34AE794;
static constexpr uintptr_t kTMP_UpdateTouchKeyboardFromEditChanges_RVA = 0x34B1A68;

// IL2CPP instance methods without explicit arguments are normally native
// functions with (this, MethodInfo*) parameters. We do not need to know the
// actual MethodInfo layout; it is forwarded as an opaque pointer.
using VoidInstanceMethodFn = void (*)(void* instance, void* methodInfo);

VoidInstanceMethodFn orig_chat_OnEnable = nullptr;
VoidInstanceMethodFn orig_chat_Update = nullptr;
VoidInstanceMethodFn orig_TMP_ActivateInputFieldInternal = nullptr;
VoidInstanceMethodFn orig_TMP_UpdateTouchKeyboardFromEditChanges = nullptr;

static inline uintptr_t ReadPtr(uintptr_t address) {
    return *reinterpret_cast<uintptr_t*>(address);
}

static inline int ReadInt(uintptr_t address) {
    return *reinterpret_cast<int*>(address);
}

static inline void WriteInt(uintptr_t address, int value) {
    *reinterpret_cast<int*>(address) = value;
}

// chat.this + 0x18 => TMP_InputField*
// TMP_InputField* + 0x114 => m_CharacterLimit
static void ForceChatInputLimit(void* chatInstance, const char* reason) {
    if (chatInstance == nullptr) {
        return;
    }

    const uintptr_t chatAddress = reinterpret_cast<uintptr_t>(chatInstance);
    const uintptr_t inputFieldAddress = ReadPtr(chatAddress + kChat_InputField_FieldOffset);
    if (inputFieldAddress == 0) {
        return;
    }

    const uintptr_t limitAddress = inputFieldAddress + kTMP_CharacterLimit_FieldOffset;
    const int oldLimit = ReadInt(limitAddress);

    if (oldLimit != 0) {
        WriteInt(limitAddress, 0);
        LOGI("chat.inputField=%p reason=%s m_CharacterLimit %d -> 0",
             reinterpret_cast<void*>(inputFieldAddress), reason, oldLimit);
    }
}

void my_chat_OnEnable(void* instance, void* methodInfo) {
    if (orig_chat_OnEnable != nullptr) {
        orig_chat_OnEnable(instance, methodInfo);
    }

    // Let the game's OnEnable finish wiring the serialized inputField first,
    // then clear the limit immediately afterward.
    ForceChatInputLimit(instance, "OnEnable");
}

void my_chat_Update(void* instance, void* methodInfo) {
    if (orig_chat_Update != nullptr) {
        orig_chat_Update(instance, methodInfo);
    }

    // Reinforce every frame. This also catches any code that restores the
    // serialized m_CharacterLimit after opening the chat.
    ForceChatInputLimit(instance, "Update");
}

void my_TMP_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    // Clear the TMP field immediately before Unity creates/updates the
    // TouchScreenKeyboard for this input field.
    if (instance != nullptr) {
        const uintptr_t limitAddress =
            reinterpret_cast<uintptr_t>(instance) + kTMP_CharacterLimit_FieldOffset;
        const int oldLimit = ReadInt(limitAddress);
        if (oldLimit != 0) {
            WriteInt(limitAddress, 0);
            LOGI("TMP_InputField=%p ActivateInputFieldInternal: m_CharacterLimit %d -> 0",
                 instance, oldLimit);
        }
    }

    if (orig_TMP_ActivateInputFieldInternal != nullptr) {
        orig_TMP_ActivateInputFieldInternal(instance, methodInfo);
    }
}

void my_TMP_UpdateTouchKeyboardFromEditChanges(void* instance, void* methodInfo) {
    // Keep the managed TMP limit cleared whenever Unity synchronizes edit
    // changes back to the native keyboard.
    if (instance != nullptr) {
        const uintptr_t limitAddress =
            reinterpret_cast<uintptr_t>(instance) + kTMP_CharacterLimit_FieldOffset;
        const int oldLimit = ReadInt(limitAddress);
        if (oldLimit != 0) {
            WriteInt(limitAddress, 0);
            LOGI("TMP_InputField=%p UpdateTouchKeyboardFromEditChanges: %d -> 0",
                 instance, oldLimit);
        }
    }

    if (orig_TMP_UpdateTouchKeyboardFromEditChanges != nullptr) {
        orig_TMP_UpdateTouchKeyboardFromEditChanges(instance, methodInfo);
    }
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

        const int parsed = sscanf(line,
                                  "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR,
                                  &start, &end, perms, &fileOffset);
        if (parsed < 4) {
            continue;
        }

        // Preferred case: a mapping of the ELF load segment at file offset 0.
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
    // The supplied 32-bit IL2CPP code is ARM/Thumb code. Function pointers need
    // the Thumb bit set, while field offsets do not.
    return address | static_cast<uintptr_t>(1);
}
#else
static inline uintptr_t MakeThumbAddress(uintptr_t address) {
    return address;
}
#endif

static inline uintptr_t RvaToFunctionAddress(uintptr_t loadBias, uintptr_t rva) {
    return MakeThumbAddress(loadBias + rva);
}

static void InstallHook(MSHookFunction_t hookFunction,
                        uintptr_t target,
                        void* replacement,
                        void** original,
                        const char* label) {
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction yok: %s", label);
        return;
    }

    LOGI("%s target=0x%" PRIxPTR, label, target);
    hookFunction(reinterpret_cast<void*>(target), replacement, original);

    if (original != nullptr && *original != nullptr) {
        LOGI("%s hook basarili", label);
    } else {
        LOGE("%s hook sonrasi original NULL", label);
    }
}

void* hack_thread(void*) {
    LOGI("basladi; libil2cpp.so bekleniyor...");

    uintptr_t il2cppLoadBias = 0;
    for (;;) {
        il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (il2cppLoadBias != 0) {
            break;
        }
        sleep(1);
    }

    LOGI("libil2cpp load bias=0x%" PRIxPTR, il2cppLoadBias);
    LOGI("dump.cs RVA kullaniliyor; -0x10000 uygulanmiyor");

    MSHookFunction_t hookFunction = ResolveMSHookFunction();
    if (hookFunction == nullptr) {
        LOGE("MSHookFunction bulunamadi; hooklar kurulmayacak");
        return nullptr;
    }

    // Primary target: the game's own chat object.
    InstallHook(hookFunction,
                RvaToFunctionAddress(il2cppLoadBias, kChat_OnEnable_RVA),
                reinterpret_cast<void*>(my_chat_OnEnable),
                reinterpret_cast<void**>(&orig_chat_OnEnable),
                "chat.OnEnable");

    InstallHook(hookFunction,
                RvaToFunctionAddress(il2cppLoadBias, kChat_Update_RVA),
                reinterpret_cast<void*>(my_chat_Update),
                reinterpret_cast<void**>(&orig_chat_Update),
                "chat.Update");

    // Secondary targets: clear the same TMP field immediately around the native
    // keyboard synchronization path.
    InstallHook(hookFunction,
                RvaToFunctionAddress(il2cppLoadBias, kTMP_ActivateInputFieldInternal_RVA),
                reinterpret_cast<void*>(my_TMP_ActivateInputFieldInternal),
                reinterpret_cast<void**>(&orig_TMP_ActivateInputFieldInternal),
                "TMP_InputField.ActivateInputFieldInternal");

    InstallHook(hookFunction,
                RvaToFunctionAddress(il2cppLoadBias, kTMP_UpdateTouchKeyboardFromEditChanges_RVA),
                reinterpret_cast<void*>(my_TMP_UpdateTouchKeyboardFromEditChanges),
                reinterpret_cast<void**>(&orig_TMP_UpdateTouchKeyboardFromEditChanges),
                "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    LOGI("chat hedefli limit kaldirma hazir: chat+0x18 -> TMP+0x114");
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;

    pthread_t thread;
    const int rc = pthread_create(&thread, nullptr, hack_thread, nullptr);
    if (rc != 0) {
        LOGE("hack_thread olusturulamadi: %d", rc);
    } else {
        pthread_detach(thread);
    }

    return JNI_VERSION_1_6;
}
