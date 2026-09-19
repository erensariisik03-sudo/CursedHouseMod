#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <utility>
#include <inttypes.h>

#include "substrate.h"

#define LOG_TAG "NativeKeyboard"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================================
// Project target
// ============================================================================
// 32-bit ARM / armeabi-v7a IL2CPP build.
// Function address = libil2cpp load bias + RVA, then Thumb bit (+1).
// No -0x10000 adjustment is used.
// ============================================================================

static constexpr uintptr_t kTMP_ActivateInputFieldInternal_RVA = 0x34AE794;
static constexpr uintptr_t kTMP_OnUpdateSelected_RVA            = 0x34B2634;
static constexpr uintptr_t kTMP_DeactivateInputField_RVA        = 0x34ACC18;
static constexpr uintptr_t kTMP_OnDeselect_RVA                  = 0x34B7480;
static constexpr uintptr_t kTMP_OnSubmit_RVA                    = 0x34B74B0;
static constexpr uintptr_t kTMP_UpdateTouchKeyboard_RVA          = 0x34B1A68;
static constexpr uintptr_t kTMP_GetText_RVA                     = 0x34A92A0;
static constexpr uintptr_t kTMP_SetText_RVA                     = 0x34A92A8;

static constexpr uintptr_t kInputField_ActivateInputFieldInternal_RVA = 0x3944DC0;
static constexpr uintptr_t kInputField_OnUpdateSelected_RVA            = 0x3948160;
static constexpr uintptr_t kInputField_DeactivateInputField_RVA        = 0x3943C10;
static constexpr uintptr_t kInputField_OnDeselect_RVA                  = 0x394C440;
static constexpr uintptr_t kInputField_OnSubmit_RVA                    = 0x394C464;
static constexpr uintptr_t kInputField_UpdateTouchKeyboard_RVA          = 0x3947EFC;
static constexpr uintptr_t kInputField_GetText_RVA                     = 0x3941B18;
static constexpr uintptr_t kInputField_SetText_RVA                     = 0x3941B20;
static constexpr uintptr_t kTMP_CharacterLimit_FieldOffset               = 0x114;
static constexpr uintptr_t kInputField_CharacterLimit_FieldOffset       = 0xDC;

using MethodInfoPtr = void*;
struct Il2CppString;

using ActivateInternalFn = void (*)(void* instance, MethodInfoPtr methodInfo);
using UpdateSelectedFn   = void (*)(void* instance, void* eventData, MethodInfoPtr methodInfo);
using VoidInstanceFn     = void (*)(void* instance, MethodInfoPtr methodInfo);
using EventFn            = void (*)(void* instance, void* eventData, MethodInfoPtr methodInfo);
using GetTextFn          = Il2CppString* (*)(void* instance, MethodInfoPtr methodInfo);
using SetTextFn          = void (*)(void* instance, Il2CppString* value, MethodInfoPtr methodInfo);

static JavaVM* g_vm = nullptr;
static MSHookFunction_t g_hook = nullptr;

static ActivateInternalFn g_origTmpActivate = nullptr;
static ActivateInternalFn g_origInputActivate = nullptr;
static UpdateSelectedFn g_origTmpUpdateSelected = nullptr;
static UpdateSelectedFn g_origInputUpdateSelected = nullptr;
static VoidInstanceFn g_origTmpDeactivate = nullptr;
static VoidInstanceFn g_origTmpUpdateKeyboard = nullptr;
static VoidInstanceFn g_origInputDeactivate = nullptr;
static VoidInstanceFn g_origInputUpdateKeyboard = nullptr;
static EventFn g_origTmpDeselect = nullptr;
static EventFn g_origInputDeselect = nullptr;
static EventFn g_origTmpSubmit = nullptr;
static EventFn g_origInputSubmit = nullptr;

static GetTextFn g_tmpGetText = nullptr;
static SetTextFn g_tmpSetText = nullptr;
static GetTextFn g_inputGetText = nullptr;
static SetTextFn g_inputSetText = nullptr;

static std::mutex g_stateMutex;
static void* g_activeField = nullptr;
static bool g_activeIsTmp = false;
static std::string g_pendingText;
static std::string g_lastAppliedText;
static bool g_hasPendingText = false;
static std::atomic<bool> g_bridgeRunning{false};
static std::atomic<bool> g_threadRunning{false};
static std::mutex g_jniUiMutex;
static jobject g_editTextGlobal = nullptr;

// ============================================================================
// Small JNI helpers
// ============================================================================

static bool ClearJavaException(JNIEnv* env, const char* where) {
    if (!env || !env->ExceptionCheck()) {
        return false;
    }
    LOGE("JNI exception at %s", where);
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
}

static JNIEnv* GetEnv(bool* attached) {
    *attached = false;
    if (!g_vm) {
        return nullptr;
    }

    JNIEnv* env = nullptr;
    const jint getEnv = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (getEnv == JNI_OK) {
        return env;
    }

    if (getEnv == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            *attached = true;
            return env;
        }
    }
    return nullptr;
}

static void DetachIfNeeded(bool attached) {
    if (attached && g_vm) {
        g_vm->DetachCurrentThread();
    }
}

static uintptr_t GetModuleLoadBias(const char* moduleName) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        return 0;
    }

    uintptr_t fallback = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, moduleName)) {
            continue;
        }

        uintptr_t start = 0;
        uintptr_t end = 0;
        uintptr_t fileOffset = 0;
        char perms[8] = {};
        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s %" SCNxPTR,
                   &start, &end, perms, &fileOffset) != 4) {
            continue;
        }

        if (fileOffset == 0) {
            fclose(f);
            return start;
        }
        if (fallback == 0 && start >= fileOffset) {
            fallback = start - fileOffset;
        }
    }

    fclose(f);
    return fallback;
}

static uintptr_t MakeThumb(uintptr_t address) {
#if defined(__arm__)
    return address | static_cast<uintptr_t>(1);
#else
    return address;
#endif
}

static uintptr_t ResolveRva(uintptr_t bias, uintptr_t rva) {
    return MakeThumb(bias + rva);
}

// ============================================================================
// IL2CPP string helpers
// ============================================================================

static std::string Utf16ToUtf8(const uint16_t* data, size_t length) {
    std::string out;
    out.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        uint32_t cp = data[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < length) {
            const uint32_t low = data[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10u) + (low - 0xDC00u);
                ++i;
            }
        }

        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

static std::string Il2CppStringToUtf8(Il2CppString* str) {
    if (!str) {
        return {};
    }

    // 32-bit IL2CPP System.String layout:
    // object header 0x00..0x07, length at 0x08, UTF-16 chars at 0x0C.
    const uintptr_t p = reinterpret_cast<uintptr_t>(str);
    const int32_t length = *reinterpret_cast<const int32_t*>(p + 0x8);
    if (length <= 0 || length > 1024 * 1024) {
        return {};
    }

    const auto* chars = reinterpret_cast<const uint16_t*>(p + 0xC);
    return Utf16ToUtf8(chars, static_cast<size_t>(length));
}

using Il2CppStringNewFn = Il2CppString* (*)(const char*);
static Il2CppStringNewFn g_il2cppStringNew = nullptr;

static void ResolveIl2CppStringApi(uintptr_t bias) {
    (void)bias;
    void* sym = dlsym(RTLD_DEFAULT, "il2cpp_string_new");
    if (!sym) {
        void* il2cpp = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (il2cpp) {
            sym = dlsym(il2cpp, "il2cpp_string_new");
        }
    }
    g_il2cppStringNew = reinterpret_cast<Il2CppStringNewFn>(sym);
    LOGI("il2cpp_string_new: %s", g_il2cppStringNew ? "resolved" : "NOT FOUND");
}

// ============================================================================
// Native Android EditText bridge
// ============================================================================

static jobject GetCurrentActivity(JNIEnv* env) {
    jclass unityPlayer = env->FindClass("com/unity3d/player/UnityPlayer");
    if (!unityPlayer) {
        ClearJavaException(env, "FindClass(UnityPlayer)");
        return nullptr;
    }

    jfieldID currentActivity = env->GetStaticFieldID(
        unityPlayer, "currentActivity", "Landroid/app/Activity;");
    if (!currentActivity) {
        ClearJavaException(env, "GetStaticFieldID(currentActivity)");
        env->DeleteLocalRef(unityPlayer);
        return nullptr;
    }

    jobject activity = env->GetStaticObjectField(unityPlayer, currentActivity);
    env->DeleteLocalRef(unityPlayer);
    return activity;
}

static jstring JavaStringFromUtf8(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}

static std::string Utf8FromJavaString(JNIEnv* env, jstring str) {
    if (!str) {
        return {};
    }

    const jsize len = env->GetStringLength(str);
    const jchar* chars = env->GetStringChars(str, nullptr);
    if (!chars) {
        return {};
    }

    const std::string result = Utf16ToUtf8(
        reinterpret_cast<const uint16_t*>(chars), static_cast<size_t>(len));
    env->ReleaseStringChars(str, chars);
    return result;
}

static bool EnsureNativeEditText(JNIEnv* env, const std::string& initialText) {
    std::lock_guard<std::mutex> lock(g_jniUiMutex);
    if (g_editTextGlobal) {
        return true;
    }

    jobject activity = GetCurrentActivity(env);
    if (!activity) {
        LOGE("UnityPlayer.currentActivity bulunamadi.");
        return false;
    }

    jclass editTextCls = env->FindClass("android/widget/EditText");
    jclass viewGroupParamsCls = env->FindClass("android/view/ViewGroup$LayoutParams");
    jclass inputTypeCls = env->FindClass("android/text/InputType");
    if (!editTextCls || !viewGroupParamsCls || !inputTypeCls) {
        ClearJavaException(env, "FindClass(EditText/LayoutParams/InputType)");
        env->DeleteLocalRef(activity);
        return false;
    }

    jmethodID editCtor = env->GetMethodID(
        editTextCls, "<init>", "(Landroid/content/Context;)V");
    jmethodID setSingleLine = env->GetMethodID(editTextCls, "setSingleLine", "(Z)V");
    jmethodID setMaxLines = env->GetMethodID(editTextCls, "setMaxLines", "(I)V");
    jmethodID setInputType = env->GetMethodID(editTextCls, "setInputType", "(I)V");
    jmethodID setText = env->GetMethodID(editTextCls, "setText", "(Ljava/lang/CharSequence;)V");
    jmethodID setFocusableInTouchMode = env->GetMethodID(editTextCls, "setFocusableInTouchMode", "(Z)V");
    jmethodID requestFocus = env->GetMethodID(editTextCls, "requestFocus", "()Z");
    jmethodID setBackgroundColor = env->GetMethodID(editTextCls, "setBackgroundColor", "(I)V");
    jmethodID setTextColor = env->GetMethodID(editTextCls, "setTextColor", "(I)V");
    jmethodID setCursorVisible = env->GetMethodID(editTextCls, "setCursorVisible", "(Z)V");
    jmethodID setAlpha = env->GetMethodID(editTextCls, "setAlpha", "(F)V");
    jmethodID getText = env->GetMethodID(editTextCls, "getText", "()Landroid/text/Editable;");

    jmethodID lpCtor = env->GetMethodID(viewGroupParamsCls, "<init>", "(II)V");
    jmethodID addContentView = env->GetMethodID(
        env->GetObjectClass(activity), "addContentView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V");

    if (!editCtor || !setSingleLine || !setMaxLines || !setInputType || !setText ||
        !setFocusableInTouchMode || !requestFocus || !setBackgroundColor ||
        !setTextColor || !setCursorVisible || !setAlpha || !getText ||
        !lpCtor || !addContentView) {
        ClearJavaException(env, "GetMethodID(EditText)");
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(editTextCls);
        env->DeleteLocalRef(viewGroupParamsCls);
        env->DeleteLocalRef(inputTypeCls);
        return false;
    }

    // InputType constants. We only use stable constants from InputType.
    const jint TYPE_CLASS_TEXT = 0x00000001;
    const jint TYPE_TEXT_FLAG_MULTI_LINE = 0x00020000;
    const jint TYPE_TEXT_FLAG_CAP_SENTENCES = 0x00004000;
    const jint inputType = TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_MULTI_LINE | TYPE_TEXT_FLAG_CAP_SENTENCES;

    jobject editText = env->NewObject(editTextCls, editCtor, activity);
    if (!editText || ClearJavaException(env, "NewObject(EditText)")) {
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(editTextCls);
        env->DeleteLocalRef(viewGroupParamsCls);
        env->DeleteLocalRef(inputTypeCls);
        return false;
    }

    jstring initial = JavaStringFromUtf8(env, initialText);
    env->CallVoidMethod(editText, setText, initial);
    env->DeleteLocalRef(initial);

    env->CallVoidMethod(editText, setSingleLine, JNI_FALSE);
    env->CallVoidMethod(editText, setMaxLines, 0x7FFFFFFF);
    env->CallVoidMethod(editText, setInputType, inputType);
    env->CallVoidMethod(editText, setFocusableInTouchMode, JNI_TRUE);
    env->CallVoidMethod(editText, setBackgroundColor, static_cast<jint>(0x00000000));
    env->CallVoidMethod(editText, setTextColor, static_cast<jint>(0x00000000));
    env->CallVoidMethod(editText, setCursorVisible, JNI_FALSE);
    env->CallVoidMethod(editText, setAlpha, 0.0f);

    jobject lp = env->NewObject(viewGroupParamsCls, lpCtor, 1, 1);
    env->CallVoidMethod(activity, addContentView, editText, lp);
    if (ClearJavaException(env, "addContentView(EditText)")) {
        env->DeleteLocalRef(lp);
        env->DeleteLocalRef(editText);
        env->DeleteLocalRef(activity);
        env->DeleteLocalRef(editTextCls);
        env->DeleteLocalRef(viewGroupParamsCls);
        env->DeleteLocalRef(inputTypeCls);
        return false;
    }

    env->CallBooleanMethod(editText, requestFocus);

    jclass contextCls = env->FindClass("android/content/Context");
    jobject imm = nullptr;
    if (contextCls) {
        jmethodID getSystemService = env->GetMethodID(
            contextCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        jfieldID inputMethodService = env->GetStaticFieldID(
            contextCls, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
        if (getSystemService && inputMethodService) {
            jstring serviceName = static_cast<jstring>(
                env->GetStaticObjectField(contextCls, inputMethodService));
            imm = env->CallObjectMethod(activity, getSystemService, serviceName);
            env->DeleteLocalRef(serviceName);
        }
    }

    if (imm) {
        jclass immCls = env->FindClass("android/view/inputmethod/InputMethodManager");
        if (immCls) {
            jmethodID showSoftInput = env->GetMethodID(
                immCls, "showSoftInput", "(Landroid/view/View;I)Z");
            if (showSoftInput) {
                env->CallBooleanMethod(editText, requestFocus);
                env->CallBooleanMethod(imm, showSoftInput, editText, 0);
            }
            env->DeleteLocalRef(immCls);
        }
        env->DeleteLocalRef(imm);
    }

    g_editTextGlobal = env->NewGlobalRef(editText);
    g_bridgeRunning.store(true);

    if (contextCls) env->DeleteLocalRef(contextCls);
    env->DeleteLocalRef(lp);
    env->DeleteLocalRef(editText);
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(editTextCls);
    env->DeleteLocalRef(viewGroupParamsCls);
    env->DeleteLocalRef(inputTypeCls);
    return g_editTextGlobal != nullptr;
}

static void RemoveNativeEditText(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(g_jniUiMutex);
    if (!g_editTextGlobal) {
        g_bridgeRunning.store(false);
        return;
    }

    jclass viewCls = env->GetObjectClass(g_editTextGlobal);
    jmethodID getWindowToken = env->GetMethodID(
        viewCls, "getWindowToken", "()Landroid/os/IBinder;");

    jobject token = nullptr;
    if (getWindowToken) {
        token = env->CallObjectMethod(g_editTextGlobal, getWindowToken);
    }

    jobject activity = GetCurrentActivity(env);
    if (activity) {
        jclass contextCls = env->FindClass("android/content/Context");
        if (contextCls) {
            jmethodID getSystemService = env->GetMethodID(
                contextCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
            jfieldID inputMethodService = env->GetStaticFieldID(
                contextCls, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
            if (getSystemService && inputMethodService) {
                jstring serviceName = static_cast<jstring>(
                    env->GetStaticObjectField(contextCls, inputMethodService));
                jobject imm = env->CallObjectMethod(activity, getSystemService, serviceName);
                env->DeleteLocalRef(serviceName);
                if (imm && token) {
                    jclass immCls = env->FindClass("android/view/inputmethod/InputMethodManager");
                    if (immCls) {
                        jmethodID hideSoftInputFromWindow = env->GetMethodID(
                            immCls, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
                        if (hideSoftInputFromWindow) {
                            env->CallBooleanMethod(imm, hideSoftInputFromWindow, token, 0);
                        }
                        env->DeleteLocalRef(immCls);
                    }
                }
                if (imm) env->DeleteLocalRef(imm);
            }
            env->DeleteLocalRef(contextCls);
        }
        env->DeleteLocalRef(activity);
    }

    // Remove the view from its parent if there is one.
    jclass viewCls2 = env->GetObjectClass(g_editTextGlobal);
    jmethodID getParent = env->GetMethodID(viewCls2, "getParent", "()Landroid/view/ViewParent;");
    jobject parent = getParent ? env->CallObjectMethod(g_editTextGlobal, getParent) : nullptr;
    if (parent) {
        jclass vgCls = env->FindClass("android/view/ViewGroup");
        if (vgCls) {
            jmethodID removeView = env->GetMethodID(
                vgCls, "removeView", "(Landroid/view/View;)V");
            if (removeView) {
                env->CallVoidMethod(parent, removeView, g_editTextGlobal);
            }
            env->DeleteLocalRef(vgCls);
        }
        env->DeleteLocalRef(parent);
    }

    if (token) env->DeleteLocalRef(token);
    env->DeleteLocalRef(viewCls2);
    env->DeleteLocalRef(viewCls);
    env->DeleteGlobalRef(g_editTextGlobal);
    g_editTextGlobal = nullptr;
    g_bridgeRunning.store(false);
}

static void KeyboardPollThread() {
    g_threadRunning.store(true);
    bool attached = false;
    JNIEnv* env = GetEnv(&attached);
    if (!env) {
        g_threadRunning.store(false);
        return;
    }

    while (g_threadRunning.load()) {
        if (!g_bridgeRunning.load() || !g_editTextGlobal) {
            usleep(50000);
            continue;
        }

        jobject editTextRef = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_jniUiMutex);
            if (g_editTextGlobal) {
                editTextRef = env->NewLocalRef(g_editTextGlobal);
            }
        }
        if (!editTextRef) {
            usleep(30000);
            continue;
        }

        jclass editTextCls = env->GetObjectClass(editTextRef);
        jmethodID getText = env->GetMethodID(
            editTextCls, "getText", "()Landroid/text/Editable;");
        jmethodID toString = nullptr;
        jobject editable = nullptr;
        jstring asString = nullptr;

        if (getText) {
            editable = env->CallObjectMethod(editTextRef, getText);
            if (editable) {
                jclass editableCls = env->GetObjectClass(editable);
                toString = env->GetMethodID(editableCls, "toString", "()Ljava/lang/String;");
                if (toString) {
                    asString = static_cast<jstring>(env->CallObjectMethod(editable, toString));
                }
                env->DeleteLocalRef(editableCls);
            }
        }

        if (!ClearJavaException(env, "poll EditText text") && asString) {
            const std::string text = Utf8FromJavaString(env, asString);
            std::lock_guard<std::mutex> lock(g_stateMutex);
            if (text != g_pendingText || !g_hasPendingText) {
                g_pendingText = text;
                g_hasPendingText = true;
            }
        }

        if (asString) env->DeleteLocalRef(asString);
        if (editable) env->DeleteLocalRef(editable);
        if (editTextCls) env->DeleteLocalRef(editTextCls);
        env->DeleteLocalRef(editTextRef);

        usleep(30000);
    }

    DetachIfNeeded(attached);
    g_threadRunning.store(false);
}

static void StartPollThread() {
    static std::once_flag once;
    std::call_once(once, []() {
        std::thread(KeyboardPollThread).detach();
    });
}

static void OpenNativeKeyboardForField(void* field, bool isTmp) {
    if (!field) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_activeField = field;
        g_activeIsTmp = isTmp;
        g_hasPendingText = false;
        g_pendingText.clear();
        g_lastAppliedText.clear();
    }

    bool attached = false;
    JNIEnv* env = GetEnv(&attached);
    if (!env) {
        LOGE("JNI env alinamadi; native keyboard acilamadi.");
        return;
    }

    std::string initialText;
    if (isTmp && g_tmpGetText) {
        initialText = Il2CppStringToUtf8(g_tmpGetText(field, nullptr));
    } else if (!isTmp && g_inputGetText) {
        initialText = Il2CppStringToUtf8(g_inputGetText(field, nullptr));
    }

    if (!EnsureNativeEditText(env, initialText)) {
        LOGE("Native EditText olusturulamadi.");
        DetachIfNeeded(attached);
        return;
    }

    StartPollThread();
    LOGI("Native Android keyboard acildi. field=%p type=%s initialLen=%zu",
         field, isTmp ? "TMP_InputField" : "InputField", initialText.size());
    DetachIfNeeded(attached);
}

static void CloseNativeKeyboard() {
    bool attached = false;
    JNIEnv* env = GetEnv(&attached);
    if (env) {
        RemoveNativeEditText(env);
    }
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_activeField = nullptr;
        g_activeIsTmp = false;
        g_pendingText.clear();
        g_hasPendingText = false;
        g_lastAppliedText.clear();
    }
    DetachIfNeeded(attached);
}

// Force the Unity-side input field to unlimited before every native-to-Unity
// transfer. This matters because InputField/TMP_InputField can clamp text in
// their managed set_text path even though the Android EditText is unlimited.
static inline void ForceUnlimitedField(void* instance, bool isTmp) {
    if (!instance) {
        return;
    }
    const uintptr_t offset = isTmp
        ? kTMP_CharacterLimit_FieldOffset
        : kInputField_CharacterLimit_FieldOffset;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(instance) + offset) = 0;
}

// ============================================================================
// Unity text synchronization
// ============================================================================

static void ApplyPendingText(void* instance, bool isTmp, MethodInfoPtr methodInfo) {
    std::string pending;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (!g_hasPendingText || instance != g_activeField) {
            return;
        }
        pending = g_pendingText;
    }

    ForceUnlimitedField(instance, isTmp);

    if (!g_il2cppStringNew) {
        return;
    }

    if (pending == g_lastAppliedText) {
        return;
    }

    Il2CppString* managed = g_il2cppStringNew(pending.c_str());
    if (!managed) {
        return;
    }

    if (isTmp) {
        if (g_tmpSetText) {
            g_tmpSetText(instance, managed, nullptr);
        }
    } else {
        if (g_inputSetText) {
            g_inputSetText(instance, managed, nullptr);
        }
    }

    g_lastAppliedText = std::move(pending);
}

static bool IsActiveField(void* instance, bool* isTmpOut = nullptr) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (instance != g_activeField) {
        return false;
    }
    if (isTmpOut) {
        *isTmpOut = g_activeIsTmp;
    }
    return true;
}

// ============================================================================
// Hooks: activation / frame update / close
// ============================================================================

static void my_TMP_Activate(void* instance, MethodInfoPtr methodInfo) {
    LOGI("TMP_InputField activation intercepted: %p", instance);
    ForceUnlimitedField(instance, true);

    // Keep Unity's normal selection/focus state so OnUpdateSelected and the
    // existing chat/send UI continue to work. We then steal the Android IME
    // focus with our native EditText.
    if (g_origTmpActivate) {
        g_origTmpActivate(instance, methodInfo);
    }

    OpenNativeKeyboardForField(instance, true);
}

static void my_Input_Activate(void* instance, MethodInfoPtr methodInfo) {
    LOGI("InputField activation intercepted: %p", instance);
    ForceUnlimitedField(instance, false);

    if (g_origInputActivate) {
        g_origInputActivate(instance, methodInfo);
    }

    OpenNativeKeyboardForField(instance, false);
}

static void my_TMP_OnUpdateSelected(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    if (g_origTmpUpdateSelected) {
        g_origTmpUpdateSelected(instance, eventData, methodInfo);
    }
    ApplyPendingText(instance, true, methodInfo);
}

static void my_Input_OnUpdateSelected(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    if (g_origInputUpdateSelected) {
        g_origInputUpdateSelected(instance, eventData, methodInfo);
    }
    ApplyPendingText(instance, false, methodInfo);
}

static void my_TMP_UpdateTouchKeyboard(void* instance, MethodInfoPtr methodInfo) {
    if (IsActiveField(instance)) {
        // Native EditText owns the Android IME now. Do not push Unity's
        // character-limited TouchScreenKeyboard state back to Android.
        ForceUnlimitedField(instance, true);
        return;
    }
    if (g_origTmpUpdateKeyboard) {
        g_origTmpUpdateKeyboard(instance, methodInfo);
    }
}

static void my_Input_UpdateTouchKeyboard(void* instance, MethodInfoPtr methodInfo) {
    if (IsActiveField(instance)) {
        ForceUnlimitedField(instance, false);
        return;
    }
    if (g_origInputUpdateKeyboard) {
        g_origInputUpdateKeyboard(instance, methodInfo);
    }
}

static void my_TMP_Deactivate(void* instance, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    if (g_origTmpDeactivate) {
        g_origTmpDeactivate(instance, methodInfo);
    }
    if (active) {
        LOGI("TMP_InputField deactivate -> native keyboard close");
        CloseNativeKeyboard();
    }
}

static void my_Input_Deactivate(void* instance, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    if (g_origInputDeactivate) {
        g_origInputDeactivate(instance, methodInfo);
    }
    if (active) {
        LOGI("InputField deactivate -> native keyboard close");
        CloseNativeKeyboard();
    }
}

static void my_TMP_OnDeselect(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    if (g_origTmpDeselect) {
        g_origTmpDeselect(instance, eventData, methodInfo);
    }
    if (active) {
        LOGI("TMP_InputField deselect -> native keyboard close");
        CloseNativeKeyboard();
    }
}

static void my_Input_OnDeselect(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    if (g_origInputDeselect) {
        g_origInputDeselect(instance, eventData, methodInfo);
    }
    if (active) {
        LOGI("InputField deselect -> native keyboard close");
        CloseNativeKeyboard();
    }
}

static void my_TMP_OnSubmit(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    ApplyPendingText(instance, true, methodInfo);
    if (g_origTmpSubmit) {
        g_origTmpSubmit(instance, eventData, methodInfo);
    }
    if (active) {
        LOGI("TMP_InputField submit -> native keyboard close");
        CloseNativeKeyboard();
    }
}

static void my_Input_OnSubmit(void* instance, void* eventData, MethodInfoPtr methodInfo) {
    const bool active = IsActiveField(instance);
    ApplyPendingText(instance, false, methodInfo);
    if (g_origInputSubmit) {
        g_origInputSubmit(instance, eventData, methodInfo);
    }
    if (active) {
        LOGI("InputField submit -> native keyboard close");
        CloseNativeKeyboard();
    }
}

// ============================================================================
// Hook installer
// ============================================================================

static void InstallHook(uintptr_t target, void* replacement, void** original, const char* label) {
    if (!g_hook) {
        LOGE("MSHookFunction yok; %s kurulamaz.", label);
        return;
    }
    LOGI("Hook %s @ 0x%" PRIxPTR, label, target);
    g_hook(reinterpret_cast<void*>(target), replacement, original);
    LOGI("Hook %s original=%p", label, original ? *original : nullptr);
}

static void* HackThread(void*) {
    LOGI("NativeKeyboard mod thread baslatildi...");

    uintptr_t bias = 0;
    while ((bias = GetModuleLoadBias("libil2cpp.so")) == 0) {
        sleep(1);
    }

    LOGI("libil2cpp.so load bias = 0x%" PRIxPTR, bias);
    LOGI("Adresleme: loadBias + RVA + Thumb(+1), -0x10000 YOK.");

    g_hook = ResolveMSHookFunction();
    if (!g_hook) {
        LOGE("MSHookFunction bulunamadi.");
        return nullptr;
    }

    ResolveIl2CppStringApi(bias);

    g_tmpGetText = reinterpret_cast<GetTextFn>(ResolveRva(bias, kTMP_GetText_RVA));
    g_tmpSetText = reinterpret_cast<SetTextFn>(ResolveRva(bias, kTMP_SetText_RVA));
    g_inputGetText = reinterpret_cast<GetTextFn>(ResolveRva(bias, kInputField_GetText_RVA));
    g_inputSetText = reinterpret_cast<SetTextFn>(ResolveRva(bias, kInputField_SetText_RVA));

    InstallHook(ResolveRva(bias, kTMP_ActivateInputFieldInternal_RVA),
                 reinterpret_cast<void*>(my_TMP_Activate),
                 reinterpret_cast<void**>(&g_origTmpActivate),
                 "TMP_InputField.ActivateInputFieldInternal");

    InstallHook(ResolveRva(bias, kInputField_ActivateInputFieldInternal_RVA),
                 reinterpret_cast<void*>(my_Input_Activate),
                 reinterpret_cast<void**>(&g_origInputActivate),
                 "InputField.ActivateInputFieldInternal");

    InstallHook(ResolveRva(bias, kTMP_OnUpdateSelected_RVA),
                 reinterpret_cast<void*>(my_TMP_OnUpdateSelected),
                 reinterpret_cast<void**>(&g_origTmpUpdateSelected),
                 "TMP_InputField.OnUpdateSelected");

    InstallHook(ResolveRva(bias, kInputField_OnUpdateSelected_RVA),
                 reinterpret_cast<void*>(my_Input_OnUpdateSelected),
                 reinterpret_cast<void**>(&g_origInputUpdateSelected),
                 "InputField.OnUpdateSelected");

    InstallHook(ResolveRva(bias, kTMP_UpdateTouchKeyboard_RVA),
                 reinterpret_cast<void*>(my_TMP_UpdateTouchKeyboard),
                 reinterpret_cast<void**>(&g_origTmpUpdateKeyboard),
                 "TMP_InputField.UpdateTouchKeyboardFromEditChanges");

    InstallHook(ResolveRva(bias, kInputField_UpdateTouchKeyboard_RVA),
                 reinterpret_cast<void*>(my_Input_UpdateTouchKeyboard),
                 reinterpret_cast<void**>(&g_origInputUpdateKeyboard),
                 "InputField.UpdateTouchKeyboardFromEditChanges");

    InstallHook(ResolveRva(bias, kTMP_DeactivateInputField_RVA),
                 reinterpret_cast<void*>(my_TMP_Deactivate),
                 reinterpret_cast<void**>(&g_origTmpDeactivate),
                 "TMP_InputField.DeactivateInputField");

    InstallHook(ResolveRva(bias, kInputField_DeactivateInputField_RVA),
                 reinterpret_cast<void*>(my_Input_Deactivate),
                 reinterpret_cast<void**>(&g_origInputDeactivate),
                 "InputField.DeactivateInputField");

    InstallHook(ResolveRva(bias, kTMP_OnDeselect_RVA),
                 reinterpret_cast<void*>(my_TMP_OnDeselect),
                 reinterpret_cast<void**>(&g_origTmpDeselect),
                 "TMP_InputField.OnDeselect");

    InstallHook(ResolveRva(bias, kInputField_OnDeselect_RVA),
                 reinterpret_cast<void*>(my_Input_OnDeselect),
                 reinterpret_cast<void**>(&g_origInputDeselect),
                 "InputField.OnDeselect");

    InstallHook(ResolveRva(bias, kTMP_OnSubmit_RVA),
                 reinterpret_cast<void*>(my_TMP_OnSubmit),
                 reinterpret_cast<void**>(&g_origTmpSubmit),
                 "TMP_InputField.OnSubmit");

    InstallHook(ResolveRva(bias, kInputField_OnSubmit_RVA),
                 reinterpret_cast<void*>(my_Input_OnSubmit),
                 reinterpret_cast<void**>(&g_origInputSubmit),
                 "InputField.OnSubmit");

    LOGI("Native keyboard bridge hazir.");
    LOGI("TouchScreenKeyboard hook'lanmiyor; oyun klavyesi tamamen bypass ediliyor.");
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;

    pthread_t thread;
    const int rc = pthread_create(&thread, nullptr, HackThread, nullptr);
    if (rc != 0) {
        LOGE("HackThread olusturulamadi: %d", rc);
    } else {
        pthread_detach(thread);
    }

    return JNI_VERSION_1_6;
}
