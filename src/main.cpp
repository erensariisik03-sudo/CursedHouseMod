#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <substrate.h> // Substrate kütüphanesi

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Fonksiyon İşaretçileri (Function Pointers)
typedef void (*TMP_set_characterLimit_t)(void* instance, int value);
TMP_set_characterLimit_t TMP_set_characterLimit = nullptr;

typedef void (*TMP_OnEnable_t)(void* instance);
TMP_OnEnable_t orig_TMP_OnEnable = nullptr;

// Kancaladığımız (Hooked) OnEnable Fonksiyonu
void my_TMP_OnEnable(void* instance) {
    // 1. Orijinal OnEnable fonksiyonunu çalıştır
    if (orig_TMP_OnEnable) {
        orig_TMP_OnEnable(instance);
    }

    // 2. Ekranda aktif olan TMP_InputField nesnesinin karakter limitini canlı olarak 0 yap!
    if (instance != nullptr && TMP_set_characterLimit != nullptr) {
        TMP_set_characterLimit(instance, 0);
        LOGI("TMP_InputField canlı bellek adresi yakalandı, limit 0 yapıldı!");
    }
}

// Bellekte kütüphane adresini bulan fonksiyon
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

// Arka plan hack thread'i
void *hack_thread(void *) {
    LOGI("Hack thread baslatildi, libil2cpp.so bekleniyor...");

    uintptr_t il2cppBase = 0;
    while (il2cppBase == 0) {
        il2cppBase = GetBaseAddress("libil2cpp.so");
        sleep(1);
    }

    LOGI("libil2cpp.so bulundu! Base Address: 0x%" PRIxPTR, il2cppBase);

    // 1. set_characterLimit fonksiyonunun adresini bagla (0x34AA4D0)
    TMP_set_characterLimit = (TMP_set_characterLimit_t)(il2cppBase + 0x34AA4D0);

    // 2. OnEnable fonksiyonunu kancala (0x34AB4CC + 1 Thumb Modu)
    uintptr_t onEnableAddr = il2cppBase + 0x34AB4CC + 1;
    
    MSHookFunction((void*)onEnableAddr, (void*)my_TMP_OnEnable, (void**)&orig_TMP_OnEnable);

    LOGI("TMP_InputField::OnEnable basariyla kancalandi!");

    return nullptr;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);

    return JNI_VERSION_1_6;
}
