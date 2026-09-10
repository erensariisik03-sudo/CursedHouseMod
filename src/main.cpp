#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "substrate.h"

#define LOG_TAG "TMP_Limit_Bypass"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Orijinal fonksiyon işaretçileri
void (*orig_set_characterLimit)(void* instance, int value) = nullptr;
void (*orig_OnEnable)(void* instance) = nullptr;

// 1. Kancalanan set_characterLimit (Oyun limiti değiştirmeye çalıştığında engeller)
void my_set_characterLimit(void* instance, int value) {
    // Gelen 'value' değerini yoksayıp her zaman 0 (sınırsız) gönderiyoruz.
    if (orig_set_characterLimit != nullptr) {
        orig_set_characterLimit(instance, 0);
    }
}

// 2. Kancalanan OnEnable (Girdi kutusu ekranda aktifleştiğinde limiti sıfırlar)
void my_OnEnable(void* instance) {
    // UI bozulmaması için önce orijinal fonksiyonu çalıştırıyoruz
    if (orig_OnEnable != nullptr) {
        orig_OnEnable(instance);
    }
    // Nesne belleğe yüklendiği an limiti doğrudan 0'a zorla
    if (instance != nullptr && orig_set_characterLimit != nullptr) {
        orig_set_characterLimit(instance, 0);
        LOGI("OnEnable tetiklendi, karakter limiti 0'a zorlandi!");
    }
}

uintptr_t GetBaseAddress(const char* name) {
    uintptr_t base = 0;
    char line[512];
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            sscanf(line, "%" SCNxPTR, &base);
            break;
        }
    }
    fclose(f);
    return base;
}

void *hack_thread(void *) {
    LOGI("Hack thread baslatildi, libil2cpp.so bekleniyor...");

    uintptr_t il2cppBase = 0;
    while (il2cppBase == 0) {
        il2cppBase = GetBaseAddress("libil2cpp.so");
        sleep(1);
    }

    LOGI("libil2cpp.so bulundu! Base Address: 0x%" PRIxPTR, il2cppBase);

    // Ghidra Image Base çıkartılmış offsetler
    uintptr_t setLimitOffset = 0x34AA4D0 - 0x10000;
    uintptr_t onEnableOffset = 0x34AB4CC - 0x10000;

#if defined(__arm__)
    uintptr_t setLimitAddr = il2cppBase + setLimitOffset + 1;
    uintptr_t onEnableAddr = il2cppBase + onEnableOffset + 1;
#else
    uintptr_t setLimitAddr = il2cppBase + setLimitOffset;
    uintptr_t onEnableAddr = il2cppBase + onEnableOffset;
#endif

    MSHookFunction_t hookFunction = ResolveMSHookFunction();
    if (hookFunction != nullptr) {
        // set_characterLimit kancalama
        hookFunction((void*)setLimitAddr, (void*)my_set_characterLimit, (void**)&orig_set_characterLimit);
        LOGI("set_characterLimit kancalandi!");

        // OnEnable kancalama
        hookFunction((void*)onEnableAddr, (void*)my_OnEnable, (void**)&orig_OnEnable);
        LOGI("OnEnable kancalandi!");
    } else {
        LOGI("MSHookFunction baglantisi saglanamadi!");
    }

    return nullptr;
}

// ShowToast ve JNI_OnLoad fonksiyonların aynı kalacak...
// (Önceki kodundaki ShowToast ve JNI_OnLoad bloklarını buraya ekle)
