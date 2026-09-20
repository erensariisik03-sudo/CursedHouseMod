
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <atomic>
#include <inttypes.h>

#include "substrate.h"

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using MethodInfoPtr = void*;
struct Il2CppString;

// -----------------------------------------------------------------------------
// Addresses verified against the supplied dump_dosyasi.cs
// ARM32 / armeabi-v7a: runtime = libil2cpp load bias + RVA + Thumb bit.
// No -0x10000 adjustment.
// -----------------------------------------------------------------------------

// Game-specific chat class: "public class chat : MonoBehaviourPunCallbacks"
// chat.inputField is the second serialized field:
//   bool chatOpen @ 0x14
//   TMP_InputField inputField @ 0x18
static constexpr uintptr_t kChat_OnEnable_RVA   = 0xF696A0;
static constexpr uintptr_t kChat_SendMessage_RVA = 0xF69758;
static constexpr uintptr_t kChat_Update_RVA     = 0xF6A6CC;
static constexpr uintptr_t kChat_InputField_Offset = 0x18;

// TMP_InputField
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x34AA4D0;
static constexpr uintptr_t kTMP_GetText_RVA            = 0x34A92A0;
static constexpr uintptr_t kTMP_SetText_RVA            = 0x34A92B0;
static constexpr uintptr_t kTMP_SetTextWithoutNotify_RVA = 0x34A9440;
static constexpr uintptr_t kTMP_AppendString_RVA        = 0x34B47DC;
static constexpr uintptr_t kTMP_AppendChar_RVA          = 0x34B4884;
static constexpr uintptr_t kTMP_InsertChar_RVA          = 0x34B4CF8;
static constexpr uintptr_t kTMP_ActivateInternal_RVA    = 0x34AE794;
static constexpr uintptr_t kTMP_UpdateTouchKeyboard_RVA = 0x34B1A68;

// TMP_InputField::m_CharacterLimit is explicitly 0x114 in the dump.
static constexpr uintptr_t kTMP_CharacterLimit_Offset = 0x114;

// -----------------------------------------------------------------------------
// Function pointer types
// -----------------------------------------------------------------------------

using VoidInstanceFn = void (*)(void* instance, MethodInfoPtr methodInfo);

using SetCharacterLimitFn =
    void (*)(void* instance, int value, MethodInfoPtr methodInfo);

using GetTextFn =
    Il2CppString* (*)(void* instance, MethodInfoPtr methodInfo);

using SetTextFn =
    void (*)(void* instance, Il2CppString* value, MethodInfoPtr methodInfo);

using SetText2Fn =
    void (*)(void* instance, Il2CppString* value, bool sendCallback, MethodInfoPtr methodInfo);

using AppendStringFn =
    void (*)(void* instance, Il2CppString* value, MethodInfoPtr methodInfo);

using AppendCharFn =
    void (*)(void* instance, uint16_t value, MethodInfoPtr methodInfo);

using ActivateFn =
    void (*)(void* instance, MethodInfoPtr methodInfo);

// -----------------------------------------------------------------------------
// Originals
// -----------------------------------------------------------------------------

static VoidInstanceFn g_origChatOnEnable = nullptr;
static VoidInstanceFn g_origChatUpdate = nullptr;
static VoidInstanceFn g_origChatSendMessage = nullptr;

static SetCharacterLimitFn g_origTmpSetCharacterLimit = nullptr;
static GetTextFn g_tmpGetText = nullptr;
static SetTextFn g_origTmpSetTextWithoutNotify = nullptr;
static SetText2Fn g_origTmpSetText = nullptr;
static AppendStringFn g_origTmpAppendString = nullptr;
static AppendCharFn g_origTmpAppendChar = nullptr;
static AppendCharFn g_origTmpInsertChar = nullptr;
static ActivateFn g_origTmpActivate = nullptr;
static VoidInstanceFn g_origTmpUpdateTouchKeyboard = nullptr;

static std::atomic<bool> g_chatSeen{false};
static void* g_lastChatInstance = nullptr;
static void* g_lastInputField = nullptr;

// -----------------------------------------------------------------------------
// Address helpers
// -----------------------------------------------------------------------------

static uintptr_t GetModuleLoadBias(const char* name) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    uintptr_t fallback = 0;
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        if (!strstr(line, name)) continue;

        uintptr_t start = 0;
        uintptr_t end = 0;
        uintptr_t offset = 0;
        char perms[8] = {};

        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s %" SCNxPTR,
                   &start, &end, perms, &offset) != 4) {
            continue;
        }

        if (offset == 0) {
            fclose(fp);
            return start;
        }

        if (fallback == 0 && start >= offset) {
            fallback = start - offset;
        }
    }

    fclose(fp);
    return fallback;
}

static uintptr_t RvaToAddress(uintptr_t bias, uintptr_t rva) {
#if defined(__arm__)
    return (bias + rva) | 1u;
#else
    return bias + rva;
#endif
}

static inline int32_t ReadCharacterLimit(void* inputField) {
    if (!inputField) return -1;
    return *reinterpret_cast<int32_t*>(
        reinterpret_cast<uintptr_t>(inputField) + kTMP_CharacterLimit_Offset
    );
}

static inline void ForceUnlimited(void* inputField, const char* reason) {
    if (!inputField) return;

    int32_t* limit = reinterpret_cast<int32_t*>(
        reinterpret_cast<uintptr_t>(inputField) + kTMP_CharacterLimit_Offset
    );

    if (*limit != 0) {
        LOGI("%s: TMP_InputField=%p characterLimit %d -> 0",
             reason, inputField, *limit);
        *limit = 0;
    }
}

static inline void* GetChatInputField(void* chatInstance) {
    if (!chatInstance) return nullptr;

    return *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(chatInstance) + kChat_InputField_Offset
    );
}

static void LogChatField(void* chatInstance, const char* reason) {
    void* field = GetChatInputField(chatInstance);

    if (field != g_lastInputField || chatInstance != g_lastChatInstance) {
        g_lastChatInstance = chatInstance;
        g_lastInputField = field;

        LOGI("%s: chat=%p inputField=%p characterLimit=%d",
             reason, chatInstance, field, ReadCharacterLimit(field));
    }

    ForceUnlimited(field, reason);
}

// -----------------------------------------------------------------------------
// Chat-specific hooks
// -----------------------------------------------------------------------------

static void my_ChatOnEnable(void* instance, MethodInfoPtr methodInfo) {
    LOGI("chat.OnEnable this=%p", instance);

    if (g_origChatOnEnable) {
        g_origChatOnEnable(instance, methodInfo);
    }

    LogChatField(instance, "chat.OnEnable AFTER");
    g_chatSeen.store(true);
}

static void my_ChatUpdate(void* instance, MethodInfoPtr methodInfo) {
    if (g_origChatUpdate) {
        g_origChatUpdate(instance, methodInfo);
    }

    // This is intentionally light: no log every frame.
    LogChatField(instance, "chat.Update");
}

static void my_ChatSendMessage(void* instance, MethodInfoPtr methodInfo) {
    LogChatField(instance, "chat.sendMessage BEFORE");

    if (g_origChatSendMessage) {
        g_origChatSendMessage(instance, methodInfo);
    }

    LogChatField(instance, "chat.sendMessage AFTER");
}

// -----------------------------------------------------------------------------
// TMP_InputField hooks
// -----------------------------------------------------------------------------

static void my_TMP_SetCharacterLimit(
    void* instance,
    int value,
    MethodInfoPtr methodInfo
) {
    LOGI("TMP_InputField.set_characterLimit(%d) -> 0 field=%p",
         value, instance);

    if (g_origTmpSetCharacterLimit) {
        g_origTmpSetCharacterLimit(instance, 0, methodInfo);
    }
}

static void my_TMP_SetText(
    void* instance,
    Il2CppString* value,
    bool sendCallback,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.SetText");

    if (g_origTmpSetText) {
        g_origTmpSetText(instance, value, sendCallback, methodInfo);
    }
}

static void my_TMP_SetTextWithoutNotify(
    void* instance,
    Il2CppString* value,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.SetTextWithoutNotify");

    if (g_origTmpSetTextWithoutNotify) {
        g_origTmpSetTextWithoutNotify(instance, value, methodInfo);
    }
}

static void my_TMP_AppendString(
    void* instance,
    Il2CppString* value,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.Append(string)");

    if (g_origTmpAppendString) {
        g_origTmpAppendString(instance, value, methodInfo);
    }
}

static void my_TMP_AppendChar(
    void* instance,
    uint16_t value,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.Append(char)");

    if (g_origTmpAppendChar) {
        g_origTmpAppendChar(instance, value, methodInfo);
    }
}

static void my_TMP_InsertChar(
    void* instance,
    uint16_t value,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.Insert(char)");

    if (g_origTmpInsertChar) {
        g_origTmpInsertChar(instance, value, methodInfo);
    }
}

static void my_TMP_Activate(
    void* instance,
    MethodInfoPtr methodInfo
) {
    ForceUnlimited(instance, "TMP_InputField.ActivateInputFieldInternal BEFORE");

    if (g_origTmpActivate) {
        g_origTmpActivate(instance, methodInfo);
    }

    ForceUnlimited(instance, "TMP_InputField.ActivateInputFieldInternal AFTER");
}

static void my_TMP_UpdateTouchKeyboard(
    void* instance,
    MethodInfoPtr methodInfo
) {
    // Keep the game's normal keyboard path. We only remove its character cap.
    ForceUnlimited(instance, "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    if (g_origTmpUpdateTouchKeyboard) {
        g_origTmpUpdateTouchKeyboard(instance, methodInfo);
    }

    // The game may reassign the limit during the original function, so clear it
    // once more on return.
    ForceUnlimited(instance, "TMP_InputField.UpdateTouchKeyboard AFTER");
}

// -----------------------------------------------------------------------------
// Hook installer
// -----------------------------------------------------------------------------

static void InstallHook(
    MSHookFunction_t hook,
    uintptr_t target,
    void* replacement,
    void** original,
    const char* name
) {
    if (!hook) {
        LOGE("MSHookFunction NULL: %s", name);
        return;
    }

    LOGI("HOOK %s target=0x%" PRIxPTR, name, target);
    hook(reinterpret_cast<void*>(target), replacement, original);
    LOGI("HOOK %s original=%p", name, original ? *original : nullptr);
}

static void* HookThread(void*) {
    LOGI("=== CursedHouseChat TARGETED LIMIT MOD ===");
    LOGI("Dump target: chat(TypeDefIndex 3387) -> inputField @ +0x18");

    uintptr_t bias = 0;
    for (;;) {
        bias = GetModuleLoadBias("libil2cpp.so");
        if (bias) break;
        sleep(1);
    }

    LOGI("libil2cpp loadBias=0x%" PRIxPTR, bias);
    LOGI("ARM32 address rule: loadBias + RVA + Thumb(+1). No -0x10000.");

    MSHookFunction_t hook = ResolveMSHookFunction();
    if (!hook) {
        LOGE("ResolveMSHookFunction failed.");
        return nullptr;
    }

    // The actual in-game chat class.
    InstallHook(
        hook,
        RvaToAddress(bias, kChat_OnEnable_RVA),
        reinterpret_cast<void*>(my_ChatOnEnable),
        reinterpret_cast<void**>(&g_origChatOnEnable),
        "chat.OnEnable"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kChat_Update_RVA),
        reinterpret_cast<void*>(my_ChatUpdate),
        reinterpret_cast<void**>(&g_origChatUpdate),
        "chat.Update"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kChat_SendMessage_RVA),
        reinterpret_cast<void*>(my_ChatSendMessage),
        reinterpret_cast<void**>(&g_origChatSendMessage),
        "chat.sendMessage"
    );

    // The exact TMP_InputField code used by chat.
    g_tmpGetText =
        reinterpret_cast<GetTextFn>(RvaToAddress(bias, kTMP_GetText_RVA));

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_SetCharacterLimit_RVA),
        reinterpret_cast<void*>(my_TMP_SetCharacterLimit),
        reinterpret_cast<void**>(&g_origTmpSetCharacterLimit),
        "TMP_InputField.set_characterLimit"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_SetText_RVA),
        reinterpret_cast<void*>(my_TMP_SetText),
        reinterpret_cast<void**>(&g_origTmpSetText),
        "TMP_InputField.SetText"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_SetTextWithoutNotify_RVA),
        reinterpret_cast<void*>(my_TMP_SetTextWithoutNotify),
        reinterpret_cast<void**>(&g_origTmpSetTextWithoutNotify),
        "TMP_InputField.SetTextWithoutNotify"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_AppendString_RVA),
        reinterpret_cast<void*>(my_TMP_AppendString),
        reinterpret_cast<void**>(&g_origTmpAppendString),
        "TMP_InputField.Append(string)"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_AppendChar_RVA),
        reinterpret_cast<void*>(my_TMP_AppendChar),
        reinterpret_cast<void**>(&g_origTmpAppendChar),
        "TMP_InputField.Append(char)"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_InsertChar_RVA),
        reinterpret_cast<void*>(my_TMP_InsertChar),
        reinterpret_cast<void**>(&g_origTmpInsertChar),
        "TMP_InputField.Insert(char)"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_ActivateInternal_RVA),
        reinterpret_cast<void*>(my_TMP_Activate),
        reinterpret_cast<void**>(&g_origTmpActivate),
        "TMP_InputField.ActivateInputFieldInternal"
    );

    InstallHook(
        hook,
        RvaToAddress(bias, kTMP_UpdateTouchKeyboard_RVA),
        reinterpret_cast<void*>(my_TMP_UpdateTouchKeyboard),
        reinterpret_cast<void**>(&g_origTmpUpdateTouchKeyboard),
        "TMP_InputField.UpdateTouchKeyboardFromEditChanges"
    );

    LOGI("=== TARGETED CHAT HOOKS INSTALLED ===");
    LOGI("Bu surum Android klavyeyi degistirmiyor; once gercek chat limit yolunu dogrudan hedefliyor.");
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, HookThread, nullptr) == 0) {
        pthread_detach(thread);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                             "HookThread olusturulamadi");
    }

    return JNI_VERSION_1_6;
}
