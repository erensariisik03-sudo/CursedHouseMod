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
#include "arm_hook.h"

#define LOG_TAG "CursedHouseChat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
static constexpr uintptr_t kLegacy_SetCharacterLimit_RVA = 0x3942DE4;
static constexpr uintptr_t kLegacy_ActivateInputFieldInternal_RVA = 0x3944DC0;
static constexpr uintptr_t kLegacy_CharacterLimit_FieldOffset = 0xDC;

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
    if (!ReadMaps(maps)) return 0;
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
    return address;
}

static inline uintptr_t RvaToFunctionAddress(uintptr_t rva) {
    return MakeFunctionAddress(g_il2cppLoadBias + rva);
}

static void LogBytes(uintptr_t address, const char* label) {
    MapEntry e{};
    if (!FindMapContaining(address & ~static_cast<uintptr_t>(1), &e)) return;
    const uintptr_t readableAddress = address & ~static_cast<uintptr_t>(1);
    if (strchr(e.perms, 'r') == nullptr) return;
    unsigned char* p = reinterpret_cast<unsigned char*>(readableAddress);
    LOGI("BYTES %s: %02X %02X %02X %02X %02X %02X %02X %02X",
         label, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);
}

static bool InstallArmHook(uintptr_t rva, void* replacement, void** original, const char* label) {
    const uintptr_t target = RvaToFunctionAddress(rva);
    if (target == 0) return false;
    LogBytes(target, label);
    const int rc = ArmHook(reinterpret_cast<void*>(target), replacement, original);
    return rc == 0 && original && *original;
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
    if (oldValue != 0) {
        WriteInt(addr, 0);
        LOGI("TMP OBJECT %p reason=%s Limit: %d -> 0", tmpInstance, reason, oldValue);
    }
}

static void ForceChatInputLimit(void* chatInstance, const char* reason) {
    if (!chatInstance) return;
    const uintptr_t chat = reinterpret_cast<uintptr_t>(chatInstance);
    const uintptr_t fieldPtrAddr = chat + kChat_InputField_FieldOffset;
    const uintptr_t inputField = *reinterpret_cast<uintptr_t*>(fieldPtrAddr);
    if (!inputField) return;
    ForceTMPObjectLimit(reinterpret_cast<void*>(inputField), reason);
}

static void my_chat_Update(void* instance, void* methodInfo) {
    if (orig_chat_Update) orig_chat_Update(instance, methodInfo);
    ForceChatInputLimit(instance, "chat.Update AFTER");
}

static void my_TMP_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("CALL TMP_InputField.set_characterLimit this=%p requested=%d -> forcing 0", instance, value);
    // PC-Relative çökme riski nedeniyle trampoline/orijinal fonksiyonu çağırmıyoruz,
    // hafızadaki limiti doğrudan '0' olarak eziyoruz.
    ForceTMPObjectLimit(instance, "TMP setter intercepted");
}

static void my_TSK_SetCharacterLimit(void* instance, int value, void* methodInfo) {
    LOGI("CALL TouchScreenKeyboard.set_characterLimit this=%p requested=%d -> forcing 0", instance, value);
    // Klavye limiti için orijinal metodun güvenli prologue'u var, 0 göndererek çağırabiliriz.
    if (orig_TSK_SetCharacterLimit) orig_TSK_SetCharacterLimit(instance, 0, methodInfo);
}

static void* hack_thread(void*) {
    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so", false);
        if (g_il2cppLoadBias == 0) sleep(1);
    }

    LOGI("libil2cpp load bias=0x%" PRIxPTR, g_il2cppLoadBias);

    // Ana Update Hook
    const bool updateHooked = InstallArmHook(
        kChat_Update_RVA,
        reinterpret_cast<void*>(my_chat_Update),
        reinterpret_cast<void**>(&orig_chat_Update),
        "chat.Update");
    LOGI("chat.Update hook %s", updateHooked ? "AKTIF" : "BASARISIZ");

    // TMP_InputField Limit Hook
    const bool tmpHooked = InstallArmHook(
        kTMP_SetCharacterLimit_RVA,
        reinterpret_cast<void*>(my_TMP_SetCharacterLimit),
        reinterpret_cast<void**>(&orig_TMP_SetCharacterLimit),
        "TMP_InputField.set_characterLimit");
    LOGI("TMP_InputField hook %s", tmpHooked ? "AKTIF" : "BASARISIZ");

    // Android Ekran Klavyesi Limit Hook
    const bool tskHooked = InstallArmHook(
        kTSK_SetCharacterLimit_RVA,
        reinterpret_cast<void*>(my_TSK_SetCharacterLimit),
        reinterpret_cast<void**>(&orig_TSK_SetCharacterLimit),
        "TouchScreenKeyboard.set_characterLimit");
    LOGI("TouchScreenKeyboard hook %s", tskHooked ? "AKTIF" : "BASARISIZ");

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