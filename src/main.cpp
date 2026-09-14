#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <cstdint>
#include "substrate.h" //[cite: 1]

extern "C" {
    void MSHookFunction(void *symbol, void *replace, void **result);
}

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 1. TouchScreenKeyboard limit kancası
void (*orig_Keyboard_setLimit)(void* instance, int value) = nullptr;
void my_Keyboard_setLimit(void* instance, int value) {
    if (orig_Keyboard_setLimit != nullptr) {
        LOGI("Klavye limiti algilandi! Android sistemine 0 (sinirsiz) gonderiliyor...");
        orig_Keyboard_setLimit(instance, 0); //[cite: 6]
    }
}

// 2. TMP_InputField limit kancası
void (*orig_TMP_setLimit)(void* instance, int value) = nullptr;
void my_TMP_setLimit(void* instance, int value) {
    if (orig_TMP_setLimit != nullptr) {
        LOGI("TMP_InputField limiti algilandi! UI limiti kaldiriliyor...");
        orig_TMP_setLimit(instance, 0); 
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

    // text.txt referans alınarak hesaplanan ofsetler (-0x10000 base adjustment)
    uintptr_t keyboardLimitOffset = 0x3598790 - 0x10000; 
    uintptr_t tmpLimitOffset = 0x34AA4D0 - 0x10000;      

#if defined(__arm__)
    // armeabi-v7a için Thumb Modu (+1)[cite: 6]
    uintptr_t keyboardHookAddr = il2cppBase + keyboardLimitOffset + 1; 
    uintptr_t tmpHookAddr = il2cppBase + tmpLimitOffset + 1;           
#else
    uintptr_t keyboardHookAddr = il2cppBase + keyboardLimitOffset;
    uintptr_t tmpHookAddr = il2cppBase + tmpLimitOffset;
#endif

    // Kancaları belleğe yazma işlemi
    MSHookFunction((void*)keyboardHookAddr, (void*)my_Keyboard_setLimit, (void**)&orig_Keyboard_setLimit); //[cite: 6]
    LOGI("TouchScreenKeyboard kancasi basariyla atildi!");

    MSHookFunction((void*)tmpHookAddr, (void*)my_TMP_setLimit, (void**)&orig_TMP_setLimit);
    LOGI("TMP_InputField kancasi basariyla atildi!");

    return nullptr;
}

extern "C" {
    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
        pthread_t ptid;
        pthread_create(&ptid, nullptr, hack_thread, nullptr);
        return JNI_VERSION_1_6; //[cite: 6]
    }
}
