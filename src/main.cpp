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
#include "substrate.h"

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
static HookBackend g_hookBackend;

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

static inline uintptr_t MakeThumbAddress(uintptr_t address) {
#if defined(__arm__)
    return address | static_cast<uintptr_t>(1);
#else
    return address;
#endif
}

static inline uintptr_t RvaToFunctionAddress(uintptr_t rva) {
    return MakeThumbAddress(g_il2cppLoadBias + rva);
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
    LOGI("RVA CHECK %s RVA=0x%" PRIxPTR " abs_no_thumb=0x%" PRIxPTR " abs_thumb=0x%" PRIxPTR,
         label, rva, g_il2cppLoadBias + rva, RvaToFunctionAddress(rva));
    LogBytes(RvaToFunctionAddress(rva), label);
}

static void* ResolveFromHandle(void* handle, const char* symbol) {
    if (!handle) return nullptr;
    return dlsym(handle, symbol);
}

static HookBackend ResolveHookBackend() {
    HookBackend out{};

    // 1) Search the global namespace first.
    out.substrate = ResolveMSHookFunctionDefault();
    out.dobby = ResolveDobbyHookDefault();
    if (out.substrate) {
        out.kind = HookBackend::SUBSTRATE;
        out.ownerName = "RTLD_DEFAULT/MSHookFunction";
        LOGI("HOOK BACKEND: MSHookFunction global export bulundu");
        return out;
    }
    if (out.dobby) {
        out.kind = HookBackend::DOBBY;
        out.ownerName = "RTLD_DEFAULT/DobbyHook";
        LOGI("HOOK BACKEND: DobbyHook global export bulundu");
        return out;
    }

    // 2) Search common already-loaded hook libraries without forcing a new load.
    const char* candidates[] = {
        "libsubstrate.so",
        "libSubstrate.so",
        "libdobby.so",
        "libshadowhook.so",
        "libwhale.so",
        nullptr
    };

    for (int i = 0; candidates[i] != nullptr; ++i) {
        void* h = dlopen(candidates[i], RTLD_NOW | RTLD_NOLOAD);
        if (!h) {
            LOGI("HOOK LIB YOK/NOLOAD: %s", candidates[i]);
            continue;
        }

        void* m = ResolveFromHandle(h, "MSHookFunction");
        void* d = ResolveFromHandle(h, "DobbyHook");
        LOGI("HOOK LIB bulundu: %s MSHookFunction=%p DobbyHook=%p", candidates[i], m, d);

        if (m) {
            out.kind = HookBackend::SUBSTRATE;
            out.substrate = reinterpret_cast<MSHookFunction_t>(m);
            out.ownerHandle = h;
            out.ownerName = candidates[i];
            return out;
        }
        if (d) {
            out.kind = HookBackend::DOBBY;
            out.dobby = reinterpret_cast<DobbyHook_t>(d);
            out.ownerHandle = h;
            out.ownerName = candidates[i];
            return out;
        }

        dlclose(h);
    }

    LOGE("HOOK BACKEND YOK: MSHookFunction ve DobbyHook bulunamadi");
    return out;
}

static bool InstallHook(uintptr_t rva, void* replacement, void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    LOGI("HOOK ATTEMPT %s RVA=0x%" PRIxPTR " target=0x%" PRIxPTR " backend=%s",
         label, rva, target, g_hookBackend.ownerName ? g_hookBackend.ownerName : "NONE");
    LogBytes(target, label);

    if (g_hookBackend.substrate) {
        g_hookBackend.substrate(reinterpret_cast<void*>(target), replacement, original);
    } else if (g_hookBackend.dobby) {
        const int rc = g_hookBackend.dobby(reinterpret_cast<void*>(target), replacement, original);
        LOGI("DobbyHook result %d for %s", rc, label);
        if (rc != 0) {
            LOGE("DobbyHook FAILED %s rc=%d", label, rc);
            return false;
        }
    } else {
        LOGE("HOOK SKIP %s: backend yok", label);
        return false;
    }

    LOGI("HOOK RESULT %s original=%p", label, original ? *original : nullptr);
    return original == nullptr || *original != nullptr;
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
    LOGI("ARM32 build=%s sizeof(void*)=%zu", sizeof(void*) == 4 ? "yes" : "no", sizeof(void*));

    // Verbose maps: confirms whether the load-bias calculation matches ELF file offsets.
    const uintptr_t verboseBias = GetModuleLoadBias("libil2cpp.so", true);
    LOGI("verbose map bias=0x%" PRIxPTR " computed_bias=0x%" PRIxPTR,
         verboseBias, g_il2cppLoadBias);

    LogAllTargets();

    g_hookBackend = ResolveHookBackend();
    if (!g_hookBackend.valid()) {
        LOGE("LIMIT KALDIRMA DEVREDE DEGIL: hook backend yok.");
        LOGE("Bu noktaya kadar RVA/base adresleri test edildi; MSHookFunction olmadigi icin kod native fonksiyonlara dokunmuyor.");
        LOGE("Sonraki test icin yuklu hook motorunun (Substrate veya Dobby) export edilmesi gerekiyor.");
        return nullptr;
    }

    LOGI("HOOK BACKEND SECILDI: %s", g_hookBackend.ownerName ? g_hookBackend.ownerName : "unknown");

    InstallHook(kChat_OnEnable_RVA,
                reinterpret_cast<void*>(my_chat_OnEnable),
                reinterpret_cast<void**>(&orig_chat_OnEnable),
                "chat.OnEnable");

    InstallHook(kChat_SendMessage_RVA,
                reinterpret_cast<void*>(my_chat_sendMessage),
                reinterpret_cast<void**>(&orig_chat_sendMessage),
                "chat.sendMessage");

    InstallHook(kChat_Update_RVA,
                reinterpret_cast<void*>(my_chat_Update),
                reinterpret_cast<void**>(&orig_chat_Update),
                "chat.Update");

    InstallHook(kTMP_SetCharacterLimit_RVA,
                reinterpret_cast<void*>(my_TMP_SetCharacterLimit),
                reinterpret_cast<void**>(&orig_TMP_SetCharacterLimit),
                "TMP_InputField.set_characterLimit");

    InstallHook(kTMP_SetText_RVA,
                reinterpret_cast<void*>(my_TMP_SetText),
                reinterpret_cast<void**>(&orig_TMP_SetText),
                "TMP_InputField.SetText");

    InstallHook(kTMP_AppendString_RVA,
                reinterpret_cast<void*>(my_TMP_AppendString),
                reinterpret_cast<void**>(&orig_TMP_AppendString),
                "TMP_InputField.Append(string)");

    InstallHook(kTMP_AppendChar_RVA,
                reinterpret_cast<void*>(my_TMP_AppendChar),
                reinterpret_cast<void**>(&orig_TMP_AppendChar),
                "TMP_InputField.Append(char)");

    InstallHook(kTMP_InsertChar_RVA,
                reinterpret_cast<void*>(my_TMP_InsertChar),
                reinterpret_cast<void**>(&orig_TMP_InsertChar),
                "TMP_InputField.Insert(char)");

    InstallHook(kTMP_ActivateInputFieldInternal_RVA,
                reinterpret_cast<void*>(my_TMP_ActivateInputFieldInternal),
                reinterpret_cast<void**>(&orig_TMP_ActivateInputFieldInternal),
                "TMP_InputField.ActivateInputFieldInternal");

    InstallHook(kTMP_UpdateTouchKeyboardFromEditChanges_RVA,
                reinterpret_cast<void*>(my_TMP_UpdateTouchKeyboardFromEditChanges),
                reinterpret_cast<void**>(&orig_TMP_UpdateTouchKeyboardFromEditChanges),
                "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    InstallHook(kTSK_SetCharacterLimit_RVA,
                reinterpret_cast<void*>(my_TSK_SetCharacterLimit),
                reinterpret_cast<void**>(&orig_TSK_SetCharacterLimit),
                "TouchScreenKeyboard.set_characterLimit");

    InstallHook(kLegacy_SetCharacterLimit_RVA,
                reinterpret_cast<void*>(my_Legacy_SetCharacterLimit),
                reinterpret_cast<void**>(&orig_Legacy_SetCharacterLimit),
                "InputField.set_characterLimit");

    InstallHook(kLegacy_ActivateInputFieldInternal_RVA,
                reinterpret_cast<void*>(my_Legacy_ActivateInputFieldInternal),
                reinterpret_cast<void**>(&orig_Legacy_ActivateInputFieldInternal),
                "InputField.ActivateInputFieldInternal");

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
