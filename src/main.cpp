#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_TAG "ModMenu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// JNI üzerinden Toast mesajı oluşturan fonksiyon
void ShowToast(JNIEnv* env, const char* text) {
    // 1. Android uygulamasının mevcut Context'ini (ActivityThread üzerinden) al
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    jmethodID currentApplicationMethod = env->GetStaticMethodID(activityThreadClass, "currentApplication", "()Landroid/app/Application;");
    jobject context = env->CallStaticObjectMethod(activityThreadClass, currentApplicationMethod);

    if (context != nullptr) {
        // 2. Gösterilecek metni Java String'e çevir
        jstring jText = env->NewStringUTF(text);

        // 3. android.widget.Toast sınıfını ve makeText metodunu bul
        jclass toastClass = env->FindClass("android/widget/Toast");
        jmethodID makeTextMethod = env->GetStaticMethodID(toastClass, "makeText", "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;");

        // 4. Toast.makeText(context, text, 1) -> (1 = Toast.LENGTH_LONG)
        jobject toastObject = env->CallStaticObjectMethod(toastClass, makeTextMethod, context, jText, 1);

        // 5. toast.show() metodunu bul ve tetikle
        jmethodID showMethod = env->GetMethodID(toastClass, "show", "()V");
        env->CallVoidMethod(toastObject, showMethod);

        // Çöpleri temizle (Memory leak önleme)
        env->DeleteLocalRef(jText);
        env->DeleteLocalRef(activityThreadClass);
        env->DeleteLocalRef(toastClass);
    }
}

// Ana Hack Thread'imiz
void *hack_thread(void *) {
    LOGI("Hack thread başlatıldı, kancalar (hook) atılıyor...");
    // --- Hook işlemleriniz (önceki kodlar) buraya gelecek ---
    return nullptr;
}

// Kütüphane (.so) oyuna enjekte edildiği an Android tarafından çağrılan ana giriş noktası
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // .so yüklendiği gibi ekranda bildirim göster
    ShowToast(env, "Mod Aktif! (Murat Muradow)");

    // Hile/Menü döngüsünü oyunun ana akışını dondurmamak için arka planda (thread) başlatıyoruz
    pthread_t ptid;
    pthread_create(&ptid, nullptr, hack_thread, nullptr);

    return JNI_VERSION_1_6;
}