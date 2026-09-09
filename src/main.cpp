#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "dobby.h" // Substrate yerine Dobby kullanıyoruz

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Fonksiyon İşaretçileri
typedef void (*TMP_set_characterLimit_t)(void* instance, int value);
TMP_set_characterLimit_t TMP_set_characterLimit = nullptr;

typedef void (*TMP_OnEnable_t)(void* instance);
TMP_OnEnable_t orig_TMP_OnEnable = nullptr;

// Kancaladığımız OnEnable Fonksiyonu
void my_TMP_OnEnable(void* instance) {
    if (orig_TMP_OnEnable) {
        orig_TMP_OnEnable(instance);
    }

    if (instance != nullptr && TMP_set_characterLimit != nullptr) {
        TMP_set_characterLimit(instance, 0);
        LOGI("TMP_InputField aktif oldu, m_CharacterLimit RAM üzerinde 0 yapıldı!");
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

void *hack_thread(void *) {
    LOGI("Hack thread baslatildi, libil2cpp.so bekleniyor...");

    uintptr_t il2cppBase = 0;
    while (il2cppBase == 0) {
        il2cppBase = GetBaseAddress("libil2cpp.so");
        sleep(1);
    }

    LOGI("libil2cpp.so bulundu! Base Address: 0x%" PRIxPTR, il2cppBase);

    // 1. set_characterLimit adresini bağla (0x34AA4D0)
    TMP_set_characterLimit = (TMP_set_characterLimit_t)(il2cppBase + 0x34AA4D0);

    // 2. OnEnable fonksiyonunu kancala (0x34AB4CC)
    // Dobby, ARM32 Thumb modunu (+1) kendisi otomatik algılar.
    uintptr_t onEnableAddr = il2cppBase + 0x34AB4CC;
    
    DobbyHook((void*)onEnableAddr, (void*)my_TMP_OnEnable, (void**)&orig_TMP_OnEnable);

    LOGI("TMP_InputField::OnEnable Dobby ile basariyla kancalandi!");

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
