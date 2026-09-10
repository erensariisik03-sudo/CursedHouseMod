#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "substrate.h"

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// TouchScreenKeyboard limit fonksiyonu için işaretçiler
void (*orig_Keyboard_setLimit)(void* instance, int value) = nullptr;

// Android klavyesine giden limiti her zaman 0 (sınırsız) olarak değiştiriyoruz
void my_Keyboard_setLimit(void* instance, int value) {
    if (orig_Keyboard_setLimit != nullptr) {
        LOGI("Klavye limiti algilandi! Android sistemine 0 (sinirsiz) gonderiliyor...");
        orig_Keyboard_setLimit(instance, 0);
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

    // TouchScreenKeyboard::set_characterLimit adresini önceki dump'tan aldık (0x3598790)
    // Ghidra Base (0x10000) çıkartılmış GERÇEK offset:
    uintptr_t keyboardLimitOffset = 0x3598790 - 0x10000;

#if defined(__arm__)
    uintptr_t hookAddress = il2cppBase + keyboardLimitOffset + 1; // Thumb Modu (+1)
#else
    uintptr_t hookAddress = il2cppBase + keyboardLimitOffset;
#endif

    // Substrate ile klavyeye giden emri kancalıyoruz
    MSHookFunction((void*)hookAddress, (void*)my_Keyboard_setLimit, (void**)&orig_Keyboard_setLimit);
    
    LOGI("TouchScreenKeyboard kancasi basariyla atildi!");

    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // Toast fonksiyonunu çökme yapmaması için kaldırdık. 
    // Logcat üzerinden takip edeceğiz.
    
    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);
    
    return JNI_VERSION_1_6;
}
