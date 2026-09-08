#include <jni.h>
#include <android/log.h>

#define LOG_TAG "NativeLibraryTemplate"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void*) {
    LOGI("Native library loaded (armeabi-v7a)");
    return JNI_VERSION_1_6;
}
