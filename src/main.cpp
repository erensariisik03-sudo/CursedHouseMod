#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Bellekte çalışan bir kütüphanenin (.so) başlangıç adresini (Base Address) bulan fonksiyon
uintptr_t GetBaseAddress(const char* name) {
    uintptr_t base = 0;
    char line[512];
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            sscanf(line, "%lx", &base);
            break;
        }
    }
    fclose(f);
    return base;
}

// Belirli bir bellek adresine doğrudan Byte yazmamızı sağlayan yama fonksiyonu
void PatchMemory(uintptr_t address, const uint8_t* patch_data, size_t size) {
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    uintptr_t page_start = address & ~(page_size - 1);
    
    // Bellek korumasını kaldır (Okuma/Yazma/Çalıştırma izni ver)
    if (mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        // Yeni byte'ları adrese yaz
        memcpy((void*)address, patch_data, size);
        LOGI("Bellek basariyla yamatildi: %lx", address);
    } else {
        LOGI("mprotect basarisiz oldu: %lx", address);
    }
}

// JNI üzerinden Toast mesajı oluşturan fonksiyon[cite: 2]
void ShowToast(JNIEnv* env, const char* text) {
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");[cite: 2]
    jmethodID currentApplicationMethod = env->GetStaticMethodID(activityThreadClass, "currentApplication", "()Landroid/app/Application;");[cite: 2]
    jobject context = env->CallStaticObjectMethod(activityThreadClass, currentApplicationMethod);[cite: 2]

    if (context != nullptr) {[cite: 2]
        jstring jText = env->NewStringUTF(text);[cite: 2]
        jclass toastClass = env->FindClass("android/widget/Toast");[cite: 2]
        jmethodID makeTextMethod = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");[cite: 2]
        jobject toastObject = env->CallStaticObjectMethod(toastClass, makeTextMethod, context, jText, 1);[cite: 2]
        jmethodID showMethod = env->GetMethodID(toastClass, "show", "()V");[cite: 2]
        
        env->CallVoidMethod(toastObject, showMethod);[cite: 2]
        env->DeleteLocalRef(jText);[cite: 2]
        env->DeleteLocalRef(activityThreadClass);[cite: 2]
        env->DeleteLocalRef(toastClass);[cite: 2]
    }
}

// Ana Hack Thread'imiz[cite: 2]
void *hack_thread(void *) {
    LOGI("Hack thread başlatıldı, libil2cpp.so bekleniyor...");

    // Oyun motorunun (libil2cpp.so) belleğe yüklenmesini bekle
    uintptr_t il2cppBase = 0;
    while (il2cppBase == 0) {
        il2cppBase = GetBaseAddress("libil2cpp.so");
        sleep(1); // CPU'yu yormamak için 1 saniye bekle
    }

    LOGI("libil2cpp.so bulundu! Base Address: %lx", il2cppBase);

    // ARM32 (Thumb) "return 0;" işlemi byte dizilimi: 
    // 00 20 -> MOVS R0, #0
    // 70 47 -> BX LR
    uint8_t ret0_patch[4] = {0x00, 0x20, 0x70, 0x47};

    // 1. TMP_InputField::get_characterLimit (0x34AA4C8)
    LOGI("TMP_InputField yamalaniyor...");
    PatchMemory(il2cppBase + 0x34AA4C8, ret0_patch, 4);

    // 2. InputField::get_characterLimit (0x3942DDC)
    LOGI("Standart InputField yamalaniyor...");
    PatchMemory(il2cppBase + 0x3942DDC, ret0_patch, 4);

    LOGI("Karakter limitleri basariyla kaldirildi!");

    return nullptr;[cite: 2]
}

// Kütüphane (.so) oyuna enjekte edildiği an Android tarafından çağrılan ana giriş noktası[cite: 2]
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {[cite: 2]
    JNIEnv* env;[cite: 2]
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {[cite: 2]
        return JNI_ERR;[cite: 2]
    }

    ShowToast(env, "Mod Aktif! (Murat Muradow)");[cite: 2]

    pthread_t ptid;[cite: 2]
    pthread_create(&ptid, nullptr, hack_thread, nullptr);[cite: 2]

    return JNI_VERSION_1_6;[cite: 2]
}
