#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <cstdint>
#include <set>
#include <vector>
#include <string>
#include <string>
#include "arm_hook.h"

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// -----------------------------
// dump.cs RVAs (32-bit IL2CPP)
// -----------------------------
static constexpr uintptr_t kChat_OnEnable_RVA = 0xF696A0;
static constexpr uintptr_t kChat_SendMessage_RVA = 0xF69758;
static constexpr uintptr_t kChat_Update_RVA = 0xF6A6CC;
static constexpr uintptr_t kChat_InputField_FieldOffset = 0x18;

static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset = 0x114;
static constexpr uintptr_t kTMP_SetCharacterLimit_RVA = 0x34AA4D0;
static constexpr uintptr_t kTMP_SetText_RVA = 0x34BD1AC;
static constexpr uintptr_t kTMP_AppendString_RVA = 0x34B47DC;
static constexpr uintptr_t kTMP_AppendChar_RVA = 0x34B4884;
static constexpr uintptr_t kTMP_InsertChar_RVA = 0x34B4CF8;
static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x34AE794;
static constexpr uintptr_t kTMP_UpdateTouchKeyboardFromEditChanges_RVA = 0x34B1A68;

static constexpr uintptr_t kTSK_SetCharacterLimit_RVA = 0x3598790;

// Legacy UI.InputField targets are logged/hooked as an additional diagnostic path.
static constexpr uintptr_t kLegacy_SetCharacterLimit_RVA = 0x3942DE4;
static constexpr uintptr_t kLegacy_ActivateInputFieldInternal_RVA = 0x3944DC0;
static constexpr uintptr_t kLegacy_CharacterLimit_FieldOffset = 0xDC;

// IL2CPP instance method ABI includes a hidden MethodInfo* argument.
typedef void (*VoidInstanceFn)(void* instance, void* methodInfo);
typedef void (*IntInstanceFn)(void* instance, int value, void* methodInfo);
typedef void (*StringInstanceFn)(void* instance, void* stringObject, void* methodInfo);
typedef void (*CharInstanceFn)(void* instance, uint32_t ch, void* methodInfo);

typedef void (*SetTextFn)(void* instance, void* stringObject, void* methodInfo);

typedef void (*ChatSendMessageFn)(void* instance, void* methodInfo);

static VoidInstanceFn orig_chat_OnEnable = nullptr;
static VoidInstanceFn orig_chat_Update = nullptr;
static ChatSendMessageFn orig_chat_sendMessage = nullptr;
static VoidInstanceFn orig_TMP_ActivateInputFieldInternal = nullptr;
static VoidInstanceFn orig_TMP_UpdateTouchKeyboardFromEditChanges = nullptr;
static IntInstanceFn orig_TMP_SetCharacterLimit = nullptr;
static IntInstanceFn orig_TSK_SetCharacterLimit = nullptr;
static IntInstanceFn orig_Legacy_SetCharacterLimit = nullptr;
static VoidInstanceFn orig_Legacy_ActivateInputFieldInternal = nullptr;
static StringInstanceFn orig_TMP_AppendString = nullptr;
static CharInstanceFn orig_TMP_AppendChar = nullptr;
static CharInstanceFn orig_TMP_InsertChar = nullptr;
static SetTextFn orig_TMP_SetText = nullptr;

static uintptr_t g_il2cppLoadBias = 0;
struct MapEntry {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t offset = 0;
    char perms[5] = {0};
    char path[512] = {0};
};


static std::string Hex(uintptr_t v) {
    char b[32];
    snprintf(b, sizeof(b), "0x%" PRIxPTR, v);
    return std::string(b);
}

static bool ReadMaps(std::vector<MapEntry>& out) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        MapEntry e{};
        unsigned int devMajor = 0, devMinor = 0;
        unsigned long inode = 0;
        char pathname[512] = {0};
        const int n = sscanf(line,
            "%" SCNxPTR "-" "%" SCNxPTR " %4s %" SCNxPTR " %x:%x %lu %511[^\n]",
            &e.start, &e.end, e.perms, &e.offset, &devMajor, &devMinor, &inode, pathname);
        if (n >= 7) {
            if (n >= 8) {
                strncpy(e.path, pathname, sizeof(e.path) - 1);
                // trim leading spaces
                char* p = e.path;
                while (*p == ' ') ++p;
                if (p != e.path) memmove(e.path, p, strlen(p) + 1);
            }
            out.push_back(e);
        }
    }
    fclose(f);
    return true;
}

static bool FindMapContaining(uintptr_t address, MapEntry* result) {
    std::vector<MapEntry> maps;
    if (!ReadMaps(maps)) return false;
    for (const auto& e : maps) {
        if (address >= e.start && address < e.end) {
            if (result) *result = e;
            return true;
        }
    }
    return false;
}

static uintptr_t GetModuleLoadBias(const char* moduleName, bool verbose) {
    std::vector<MapEntry> maps;
    if (!ReadMaps(maps)) {
        LOGE("/proc/self/maps acilamadi");
        return 0;
    }

    uintptr_t best = 0;
    uintptr_t minBias = UINTPTR_MAX;
    bool found = false;

    for (const auto& e : maps) {
        if (strstr(e.path, moduleName) == nullptr) continue;
        found = true;

        if (verbose) {
            LOGI("MAP %s-%s off=%s perms=%s path=%s",
                 Hex(e.start).c_str(), Hex(e.end).c_str(), Hex(e.offset).c_str(),
                 e.perms, e.path[0] ? e.path : "[anon]");
        }

        const uintptr_t bias = e.start - e.offset;
        if (bias < minBias) minBias = bias;
        if (e.offset == 0) best = e.start;
    }

    if (!found) return 0;
    if (best != 0) return best;
    return (minBias == UINTPTR_MAX) ? 0 : minBias;
}

static inline uintptr_t MakeFunctionAddress(uintptr_t address) {
    // The diagnostic bytes prove this ARM32 libil2cpp is using ARM-mode
    // prologues (for example 30 48 2D E9), not Thumb-mode prologues.
    // Therefore do NOT add +1 to these dump.cs RVAs.
    return address;
}

static inline uintptr_t RvaToFunctionAddress(uintptr_t rva) {
    return MakeFunctionAddress(g_il2cppLoadBias + rva);
}

static void LogBytes(uintptr_t address, const char* label) {
    MapEntry e{};
    if (!FindMapContaining(address & ~static_cast<uintptr_t>(1), &e)) {
        LOGE("TARGET INVALID label=%s addr=0x%" PRIxPTR " RVA-mapped region not found", label, address);
        return;
    }

    const uintptr_t readableAddress = address & ~static_cast<uintptr_t>(1);
    if (strchr(e.perms, 'r') == nullptr) {
        LOGE("TARGET NOT READABLE label=%s addr=0x%" PRIxPTR " map=%s", label, address, e.perms);
        return;
    }

    unsigned char* p = reinterpret_cast<unsigned char*>(readableAddress);
    LOGI("TARGET label=%s RVA=%s addr=%s map=[%s-%s] off=%s perms=%s",
         label, Hex(readableAddress - g_il2cppLoadBias).c_str(), Hex(address).c_str(),
         Hex(e.start).c_str(), Hex(e.end).c_str(), Hex(e.offset).c_str(), e.perms);

    LOGI("BYTES %s: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
         label, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
}

static void LogTarget(uintptr_t rva, const char* label) {
    LOGI("RVA CHECK %s RVA=0x%" PRIxPTR " abs_arm=0x%" PRIxPTR,
         label, rva, RvaToFunctionAddress(rva));
    LogBytes(RvaToFunctionAddress(rva), label);
}

static void* ResolveFromHandle(void* handle, const char* symbol) {
    if (!handle) return nullptr;
    return dlsym(handle, symbol);
}

static bool InstallArmHook(uintptr_t rva, void* replacement, void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    LOGI("HOOK ATTEMPT %s RVA=0x%" PRIxPTR " target=0x%" PRIxPTR, label, rva, target);
    LogBytes(target, label);

    if (target == 0) {
        LOGE("HOOK FAIL %s: target=0", label);
        return false;
    }

    const int rc = ArmHook(reinterpret_cast<void*>(target), replacement, original);
    LOGI("ArmHook result %d for %s original=%p", rc, label, original ? *original : nullptr);
    if (rc != 0) {
        LOGE("ArmHook FAILED %s rc=%d", label, rc);
        return false;
    }
    return original && *original;
}

static inline int ReadInt(uintptr_t address) {
    return *reinterpret_cast<int*>(address);
}

static inline void WriteInt(uintptr_t address, int value) {
    *reinterpret_cast<int*>(address) = value;
}

static void ForceTMPObjectLimit(void* tmpInstance, const char* reason) {
    if (!tmpInstance) return;
    const uintptr_t base = reinterpret_cast<uintptr_t>(tmpInstance);
    const uintptr_t addr = base + kTMP_CharacterLimit_FieldOffset;
    const int oldValue = ReadInt(addr);
    LOGI("TMP OBJECT %p reason=%s m_CharacterLimit=%d", tmpInstance, reason, oldValue);
    if (oldValue != 0) {
        WriteInt(addr, 0);
        LOGI("TMP OBJECT %p reason=%s m_CharacterLimit %d -> 0", tmpInstance, reason, oldValue);
    }
}

static void ForceChatInputLimit(void* chatInstance, const char* reason) {
    if (!chatInstance) return;
    const uintptr_t chat = reinterpret_cast<uintptr_t>(chatInstance);
    const uintptr_t fieldPtrAddr = chat + kChat_InputField_FieldOffset;
    const uintptr_t inputField = *reinterpret_cast<uintptr_t*>(fieldPtrAddr);

    LOGI("CHAT INSTANCE %p reason=%s inputFieldPtr=%p", chatInstance, reason,
         reinterpret_cast<void*>(inputField));

    if (!inputField) return;
    ForceTMPObjectLimit(reinterpret_cast<void*>(inputField), reason);
}

// -----------------------------
// Hooks
// -----------------------------
static void my_chat_OnEnable(void* instance, void* methodInfo) {
    LOGI("CALL chat.OnEnable this=%p", instance);
    if (orig_chat_OnEnable) orig_chat_OnEnable(instance, methodInfo);
    ForceChatInputLimit(instance, "chat.OnEnable AFTER");
}

static void my_chat_Update(void* instance, void* methodInfo) {
    LOGI("CALL chat.Update this=%p", instance);
    if (orig_chat_Update) orig_chat_Update(instance, methodInfo);
    ForceChatInputLimit(instance, "chat.Update AFTER");
}

static void my_chat_sendMessage(void* instance, void* methodInfo) {
    LOGI("CALL chat.sendMessage this=%p", instance);
    ForceChatInputLimit(instance, "before chat.sendMessage");
    if (orig_chat_sendMessage) orig_chat_sendMessage(instance, methodInfo);
    ForceChatInputLimit(instance, "after chat.sendMessage");
}

static void my_TMP_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("CALL TMP_InputField.set_characterLimit this=%p requested=%d -> forcing 0", instance, value);
    if (orig_TMP_SetCharacterLimit) orig_TMP_SetCharacterLimit(instance, 0, methodInfo);
    else ForceTMPObjectLimit(instance, "TMP setter no-original");
}

static void my_TSK_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("CALL TouchScreenKeyboard.set_characterLimit this=%p requested=%d -> forcing 0", instance, value);
    if (orig_TSK_SetCharacterLimit) orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
}

static void my_Legacy_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("CALL UnityEngine.UI.InputField.set_characterLimit this=%p requested=%d -> forcing 0", instance, value);
    if (orig_Legacy_SetCharacterLimit) orig_Legacy_SetCharacterLimit(instance, 0, methodInfo);
}

static void my_Legacy_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    LOGI("CALL Legacy InputField.ActivateInputFieldInternal this=%p", instance);
    if (orig_Legacy_ActivateInputFieldInternal) orig_Legacy_ActivateInputFieldInternal(instance, methodInfo);
    const uintptr_t addr = reinterpret_cast<uintptr_t>(instance) + kLegacy_CharacterLimit_FieldOffset;
    LOGI("LEGACY OBJECT %p m_CharacterLimit=%d", instance, ReadInt(addr));
}

static void my_TMP_ActivateInputFieldInternal(void* instance, void* methodInfo) {
    LOGI("CALL TMP_InputField.ActivateInputFieldInternal this=%p", instance);
    ForceTMPObjectLimit(instance, "before ActivateInputFieldInternal");
    if (orig_TMP_ActivateInputFieldInternal) orig_TMP_ActivateInputFieldInternal(instance, methodInfo);
    ForceTMPObjectLimit(instance, "after ActivateInputFieldInternal");
}

static void my_TMP_UpdateTouchKeyboardFromEditChanges(void* instance, void* methodInfo) {
    LOGI("CALL TMP_InputField.UpdateTouchKeyboardFromEditChanges this=%p", instance);
    ForceTMPObjectLimit(instance, "before UpdateTouchKeyboardFromEditChanges");
    if (orig_TMP_UpdateTouchKeyboardFromEditChanges) orig_TMP_UpdateTouchKeyboardFromEditChanges(instance, methodInfo);
    ForceTMPObjectLimit(instance, "after UpdateTouchKeyboardFromEditChanges");
}

static void my_TMP_AppendString(void* instance, void* stringObject, void* methodInfo) {
    LOGI("CALL TMP_InputField.Append(string) this=%p arg=%p", instance, stringObject);
    ForceTMPObjectLimit(instance, "before Append(string)");
    if (orig_TMP_AppendString) orig_TMP_AppendString(instance, stringObject, methodInfo);
    ForceTMPObjectLimit(instance, "after Append(string)");
}

static void my_TMP_AppendChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("CALL TMP_InputField.Append(char) this=%p charCode=%u", instance, ch);
    ForceTMPObjectLimit(instance, "before Append(char)");
    if (orig_TMP_AppendChar) orig_TMP_AppendChar(instance, ch, methodInfo);
    ForceTMPObjectLimit(instance, "after Append(char)");
}

static void my_TMP_InsertChar(void* instance, uint32_t ch, void* methodInfo) {
    LOGI("CALL TMP_InputField.Insert(char) this=%p charCode=%u", instance, ch);
    ForceTMPObjectLimit(instance, "before Insert(char)");
    if (orig_TMP_InsertChar) orig_TMP_InsertChar(instance, ch, methodInfo);
    ForceTMPObjectLimit(instance, "after Insert(char)");
}

static void my_TMP_SetText(void* instance, void* stringObject, void* methodInfo) {
    LOGI("CALL TMP_InputField.SetText(string) this=%p arg=%p", instance, stringObject);
    ForceTMPObjectLimit(instance, "before SetText");
    if (orig_TMP_SetText) orig_TMP_SetText(instance, stringObject, methodInfo);
    ForceTMPObjectLimit(instance, "after SetText");
}

static void LogAllTargets() {
    LOGI("========== TARGET DIAGNOSTICS START ==========");
    LogTarget(kChat_OnEnable_RVA, "chat.OnEnable");
    LogTarget(kChat_SendMessage_RVA, "chat.sendMessage");
    LogTarget(kChat_Update_RVA, "chat.Update");
    LogTarget(kTMP_SetCharacterLimit_RVA, "TMP_InputField.set_characterLimit");
    LogTarget(kTMP_SetText_RVA, "TMP_InputField.SetText");
    LogTarget(kTMP_AppendString_RVA, "TMP_InputField.Append(string)");
    LogTarget(kTMP_AppendChar_RVA, "TMP_InputField.Append(char)");
    LogTarget(kTMP_InsertChar_RVA, "TMP_InputField.Insert(char)");
    LogTarget(kTMP_ActivateInputFieldInternal_RVA, "TMP_InputField.ActivateInputFieldInternal");
    LogTarget(kTMP_UpdateTouchKeyboardFromEditChanges_RVA, "TMP_InputField.UpdateTouchKeyboardFromEditChanges");
    LogTarget(kTSK_SetCharacterLimit_RVA, "TouchScreenKeyboard.set_characterLimit");
    LogTarget(kLegacy_SetCharacterLimit_RVA, "InputField.set_characterLimit");
    LogTarget(kLegacy_ActivateInputFieldInternal_RVA, "InputField.ActivateInputFieldInternal");
    LOGI("========== TARGET DIAGNOSTICS END ==========");
}

static void* hack_thread(void*) {
    LOGI("basladi; libil2cpp.so bekleniyor...");

    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so", false);
        if (g_il2cppLoadBias == 0) sleep(1);
    }

    LOGI("libil2cpp load bias=0x%" PRIxPTR, g_il2cppLoadBias);
    LOGI("dump.cs RVA kullaniliyor; -0x10000 uygulanmiyor");
    LOGI("ARM32 build=%s sizeof(void*)=%zu; function addresses use ARM mode (NO +1)", sizeof(void*) == 4 ? "yes" : "no", sizeof(void*));

    // Verbose maps: confirms whether the load-bias calculation matches ELF file offsets.
    const uintptr_t verboseBias = GetModuleLoadBias("libil2cpp.so", true);
    LOGI("verbose map bias=0x%" PRIxPTR " computed_bias=0x%" PRIxPTR,
         verboseBias, g_il2cppLoadBias);

    LogAllTargets();

    LOGI("HOOK BACKEND: builtin ARM32 inline hook");
    LOGI("Not: Bu surum Dobby/Substrate kullanmiyor.");

    // The dump and diagnostic bytes showed chat.Update begins with ARM-mode
    // instructions that are safe for the minimal 8-byte trampoline:
    //   10 40 2D E9   00 40 A0 E1
    // Therefore this is the first/only hook installed in v4. It is enough to
    // reach chat.inputField (this + 0x18) every frame and force m_CharacterLimit
    // (inputField + 0x114) to zero. Other targets remain diagnostics-only.
    const bool updateHooked = InstallArmHook(
        kChat_Update_RVA,
        reinterpret_cast<void*>(my_chat_Update),
        reinterpret_cast<void**>(&orig_chat_Update),
        "chat.Update");

    LOGI("chat.Update hook %s", updateHooked ? "AKTIF" : "BASARISIZ");
    if (!updateHooked) {
        LOGE("LIMIT KALDIRMA DEVREDE DEGIL: chat.Update hook kurulamadı.");
        return nullptr;
    }

    LOGI("========== HOOK INSTALL COMPLETE ==========");
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;
    pthread_t thread;
    const int rc = pthread_create(&thread, nullptr, hack_thread, nullptr);
    if (rc != 0) LOGE("hack_thread olusturulamadi: %d", rc);
    else pthread_detach(thread);
    return JNI_VERSION_1_6;
}
