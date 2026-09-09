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

// Orijinal get_characterLimit fonksiyon işaretçisi
int (*orig_get_characterLimit)(void* instance) = nullptr;

// Kancalanan get_characterLimit (Sınırsız için 0 döndürüyoruz)
int my_get_characterLimit(void* instance) {
    return 0; 
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

    // Ghidra Image Base (0x10000) çıkartılmış GERÇEK offset adresi
    uintptr_t targetOffset = 0x34AA4C8 - 0x10000; // 0x349A4C8

    // Mimariye göre Thumb Modu (+1) kontrolü (armeabi-v7a için __arm__ bloğu çalışır)
#if defined(__arm__)
    uintptr_t hookAddress = il2cppBase + targetOffset + 1; // ARM32 (Thumb)
#else
    uintptr_t hookAddress = il2cppBase + targetOffset;     // ARM64 (AArch64)
#endif

    MSHookFunction_t hookFunction = ResolveMSHookFunction();
    if (hookFunction != nullptr) {
        hookFunction((void*)hookAddress, (void*)my_get_characterLimit, (void**)&orig_get_characterLimit);
        LOGI("get_characterLimit basariyla kancalandi! Gercek Offset: 0x%" PRIxPTR, targetOffset);
    } else {
        LOGI("MSHookFunction baglantisi saglanamadi!");
    }

    return nullptr;
}

// JNI üzerinden Toast bildirimi gösteren fonksiyon
void ShowToast(JNIEnv* env, const char* text) {
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (!activityThreadClass) return;

    jmethodID currentApplicationMethod = env->GetStaticMethodID(activityThreadClass, "currentApplication", "()Landroid/app/Application;");
    if (!currentApplicationMethod) return;

    jobject context = env->CallStaticObjectMethod(activityThreadClass, currentApplicationMethod);
    if (!context) return;

    jclass toastClass = env->FindClass("android/widget/Toast");
    if (!toastClass) return;

    jstring javaString = env->NewStringUTF(text);
    jmethodID makeTextMethod = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");
    if (!makeTextMethod) return;

    // 1 = Toast.LENGTH_LONG
    jobject toastObject = env->CallStaticObjectMethod(toastClass, makeTextMethod, context, javaString, 1);
    if (!toastObject) return;

    jmethodID showMethod = env->GetMethodID(toastClass, "show", "()V");
    if (showMethod) {
        env->CallVoidMethod(toastObject, showMethod);
    }

    env->DeleteLocalRef(javaString);
    env->DeleteLocalRef(toastClass);
    env->DeleteLocalRef(activityThreadClass);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env != nullptr) {
        // .so yüklendiğinde görünecek mesaj
        ShowToast(env, "Sohbet Limiti Başarıyla Kaldırıldı!");
    }

    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);
    return JNI_VERSION_1_6;
}
