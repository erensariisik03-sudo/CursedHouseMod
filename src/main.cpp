#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <inttypes.h>

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Bellekte çalışan kütüphanenin (.so) başlangıç adresini (Base Address) bulan fonksiyon
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

// Belirli bir bellek adresine doğrudan Byte yazan yama fonksiyonu
void PatchMemory(uintptr_t address, const uint8_t* patch_data, size_t size) {
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    uintptr_t page_start = address & ~(page_size - 1);
    
    // Bellek korumasını kaldır (Okuma/Yazma/Çalıştırma izni ver)
    if (mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy((void*)address, patch_data, size);
        LOGI("Bellek basariyla yamatildi: 0x%" PRIxPTR, address);
    } else {
        LOGI("mprotect basarisiz oldu: 0x%" PRIxPTR, address);
    }
}

// JNI üzerinden Toast mesajı oluşturan fonksiyon
void ShowToast(JNIEnv* env, const char* text) {
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (!activityThreadClass) return;

    jmethodID currentApplicationMethod = env->GetStaticMethodID(activityThreadClass, "currentApplication", "()Landroid/app/Application;");
    jobject context = env->CallStaticObjectMethod(activityThreadClass, currentApplicationMethod);

    if (context != nullptr) {
        jstring jText = env->NewStringUTF(text);
        jclass toastClass = env->FindClass("android/widget/Toast");
        jmethodID makeTextMethod = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
        jobject toastObject = env->CallStaticObjectMethod(toastClass, makeTextMethod, context, jText, 1);
        jmethodID showMethod = env->GetMethodID(toastClass, "show", "()V");
        
        env->CallVoidMethod(toastObject, showMethod);
        env->DeleteLocalRef(jText);
        env->DeleteLocalRef(toastClass);
    }
    env->DeleteLocalRef(activityThreadClass);
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

    // ARM32 (Thumb) "return 0;" -> MOVS R0, #0 ; BX LR
    uint8_t ret0_patch[4] = {0x00, 0x20, 0x70, 0x47};

    // ARM32 (Thumb) "return;" -> NOP ; BX LR
    uint8_t void_ret_patch[4] = {0x00, 0xBF, 0x70, 0x47};

    // --- GETTER YAMALARI (Her zaman 0/Sınırsız döndürür) ---
    LOGI("Getter fonksiyonlari yamalaniyor...");
    PatchMemory(il2cppBase + 0x34AA4C8, ret0_patch, 4); // TMP_InputField::get_characterLimit
    PatchMemory(il2cppBase + 0x3942DDC, ret0_patch, 4); // InputField::get_characterLimit

    // --- SETTER YAMALARI (Karakter limiti atanmasını engeller) ---
    LOGI("Setter fonksiyonlari yamalaniyor...");
    PatchMemory(il2cppBase + 0x3598790, void_ret_patch, 4); // TouchScreenKeyboard::set_characterLimit
    PatchMemory(il2cppBase + 0x34AA4D0, void_ret_patch, 4); // TMP_InputField::set_characterLimit
    PatchMemory(il2cppBase + 0x3942DE4, void_ret_patch, 4); // InputField::set_characterLimit

    LOGI("Karakter limiti yama islemi tamamlandi!");

    return nullptr;
}

// .so dosyası yüklendiğinde çalışacak JNI giriş noktası
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    ShowToast(env, "Mod Aktif! (Murat Muradow)");

    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);

    return JNI_VERSION_1_6;
}
