#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <dlfcn.h>

#include "arm_hook.h"

#define LOG_TAG "CursedHouseMod"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
// IL2CPP LOAD BIAS
// ============================================================

static uintptr_t g_il2cppLoadBias = 0;

static uintptr_t GetModuleBase(const char* moduleName)
{
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp)
        return 0;

    char line[512];

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, moduleName))
        {
            uintptr_t base = 0;

            if (sscanf(line, "%lx-%*lx", &base) == 1)
            {
                fclose(fp);
                return base;
            }
        }
    }

    fclose(fp);
    return 0;
}

static uintptr_t RvaToFunctionAddress(uintptr_t rva)
{
    if (!g_il2cppLoadBias)
        return 0;

    return g_il2cppLoadBias + rva;
}

// ============================================================
// UNITY TYPES
// ============================================================

struct Vector3
{
    float x;
    float y;
    float z;
};

// ============================================================
// IL2CPP / UNITY ADDRESSES
// ============================================================

// playerController::Update()
static constexpr uintptr_t kPlayerController_Update_RVA =
    0x10D24DC;

// PhotonView::get_IsMine()
static constexpr uintptr_t kPhotonView_get_IsMine_RVA =
    0x2AA4604;

// UnityEngine.Component::get_transform()
static constexpr uintptr_t kComponent_get_transform_RVA =
    0x3584684;

// UnityEngine.Transform::get_position()
static constexpr uintptr_t kTransform_get_position_RVA =
    0x359B380;

// playerController field:
// private PhotonView pv; // 0x2A0
static constexpr uintptr_t kPlayerController_PV_Offset =
    0x2A0;

// ============================================================
// FUNCTION TYPES
// ============================================================

using PlayerControllerUpdate_t =
    void (*)(void* instance);

using PhotonViewGetIsMine_t =
    bool (*)(void* instance);

using ComponentGetTransform_t =
    void* (*)(void* instance);

using TransformGetPosition_t =
    Vector3 (*)(void* instance);

// ============================================================
// ORIGINAL FUNCTIONS
// ============================================================

static PlayerControllerUpdate_t
orig_PlayerController_Update = nullptr;

static PhotonViewGetIsMine_t
fn_PhotonView_get_IsMine = nullptr;

static ComponentGetTransform_t
fn_Component_get_transform = nullptr;

static TransformGetPosition_t
fn_Transform_get_position = nullptr;

// ============================================================
// STATE
// ============================================================

static void* g_localPlayerController = nullptr;

static uint64_t g_lastCoordinateDumpNs = 0;

static bool g_menuReady = false;

// ============================================================
// TIME
// ============================================================

static uint64_t GetMonotonicNs()
{
    timespec ts{};

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return
        (uint64_t)ts.tv_sec * 1000000000ULL +
        (uint64_t)ts.tv_nsec;
}

// ============================================================
// COORDINATE DUMP
// ============================================================

static void DumpLocalPlayerPosition(void* playerController)
{
    if (!playerController)
        return;

    if (!fn_Component_get_transform)
        return;

    if (!fn_Transform_get_position)
        return;

    void* transform =
        fn_Component_get_transform(playerController);

    if (!transform)
        return;

    Vector3 pos =
        fn_Transform_get_position(transform);

    LOGI(
        "================================================"
    );

    LOGI(
        "PLAYER COORDINATE"
    );

    LOGI(
        "X = %.4f",
        pos.x
    );

    LOGI(
        "Y = %.4f",
        pos.y
    );

    LOGI(
        "Z = %.4f",
        pos.z
    );

    LOGI(
        "POSITION = { %.4f, %.4f, %.4f }",
        pos.x,
        pos.y,
        pos.z
    );

    LOGI(
        "================================================"
    );
}

// ============================================================
// PLAYERCONTROLLER UPDATE HOOK
// ============================================================

static void Hooked_PlayerController_Update(void* instance)
{
    // Önce oyunun kendi Update'i çalışsın.
    if (orig_PlayerController_Update)
    {
        orig_PlayerController_Update(instance);
    }

    if (!instance)
        return;

    // --------------------------------------------------------
    // Local player'ı bul
    // --------------------------------------------------------

    if (!g_localPlayerController && fn_PhotonView_get_IsMine)
    {
        void* pv =
            *(void**)
            (
                (uintptr_t)instance +
                kPlayerController_PV_Offset
            );

        if (pv)
        {
            bool isMine =
                fn_PhotonView_get_IsMine(pv);

            if (isMine)
            {
                g_localPlayerController = instance;

                LOGI(
                    "[MOD] Local playerController bulundu: %p",
                    instance
                );
            }
        }
    }

    // Henüz kendi oyuncumuz bulunamadıysa çık.
    if (!g_localPlayerController)
        return;

    // Sadece kendi oyuncunun Update'inde işlem yap.
    if (instance != g_localPlayerController)
        return;

    // --------------------------------------------------------
    // 5 saniyede bir koordinat yaz
    // --------------------------------------------------------

    uint64_t now =
        GetMonotonicNs();

    if (g_lastCoordinateDumpNs == 0)
    {
        g_lastCoordinateDumpNs = now;

        LOGI(
            "[MOD] Coordinate logger aktif."
        );

        DumpLocalPlayerPosition(
            g_localPlayerController
        );

        return;
    }

    if (now - g_lastCoordinateDumpNs >= 5000000000ULL)
    {
        g_lastCoordinateDumpNs = now;

        DumpLocalPlayerPosition(
            g_localPlayerController
        );
    }
}

// ============================================================
// SIMPLE MENU PLACEHOLDER
// ============================================================
//
// İlk aşamada gerçek UI kütüphanesi eklemiyoruz.
// Burayı ileride ImGui / Unity UI menüsüne bağlayacağız.
//
// Şimdilik mod başlarken hazır olduğunu log'a bildiriyor.
//

static void PrepareMenu()
{
    if (g_menuReady)
        return;

    g_menuReady = true;

    LOGI(
        "[MENU] Cursed House Mod menu yapisi hazir."
    );

    LOGI(
        "[MENU] Sonraki asamada buraya:"
    );

    LOGI(
        "[MENU]  - Coordinate Capture"
    );

    LOGI(
        "[MENU]  - Save Position"
    );

    LOGI(
        "[MENU]  - Spawn"
    );

    LOGI(
        "[MENU]  - Character Select"
    );

    LOGI(
        "[MENU] butonlari eklenecek."
    );
}

// ============================================================
// HOOK INSTALL
// ============================================================

static bool InstallHooks()
{
    if (!g_il2cppLoadBias)
        return false;

    uintptr_t updateAddr =
        RvaToFunctionAddress(
            kPlayerController_Update_RVA
        );

    uintptr_t isMineAddr =
        RvaToFunctionAddress(
            kPhotonView_get_IsMine_RVA
        );

    uintptr_t getTransformAddr =
        RvaToFunctionAddress(
            kComponent_get_transform_RVA
        );

    uintptr_t getPositionAddr =
        RvaToFunctionAddress(
            kTransform_get_position_RVA
        );

    if (!updateAddr)
    {
        LOGE("[MOD] playerController.Update adresi bulunamadi!");
        return false;
    }

    if (!isMineAddr)
    {
        LOGE("[MOD] PhotonView.get_IsMine adresi bulunamadi!");
        return false;
    }

    if (!getTransformAddr)
    {
        LOGE("[MOD] Component.get_transform adresi bulunamadi!");
        return false;
    }

    if (!getPositionAddr)
    {
        LOGE("[MOD] Transform.get_position adresi bulunamadi!");
        return false;
    }

    // --------------------------------------------------------
    // Function pointers
    // --------------------------------------------------------

    fn_PhotonView_get_IsMine =
        reinterpret_cast<PhotonViewGetIsMine_t>(
            isMineAddr
        );

    fn_Component_get_transform =
        reinterpret_cast<ComponentGetTransform_t>(
            getTransformAddr
        );

    fn_Transform_get_position =
        reinterpret_cast<TransformGetPosition_t>(
            getPositionAddr
        );

    // --------------------------------------------------------
    // Hook Update
    // --------------------------------------------------------

    if (!ArmHook(
            (void*)updateAddr,
            (void*)Hooked_PlayerController_Update,
            (void**)&orig_PlayerController_Update))
    {
        LOGE(
            "[MOD] playerController.Update hook BASARISIZ!"
        );

        return false;
    }

    LOGI(
        "[MOD] playerController.Update hook BASARILI."
    );

    PrepareMenu();

    return true;
}

// ============================================================
// HACK THREAD
// ============================================================

static void* hack_thread(void*)
{
    LOGI(
        "[MOD] Hack thread basladi."
    );

    // --------------------------------------------------------
    // libil2cpp.so bekle
    // --------------------------------------------------------

    while (!g_il2cppLoadBias)
    {
        g_il2cppLoadBias =
            GetModuleBase("libil2cpp.so");

        if (!g_il2cppLoadBias)
        {
            sleep(1);
            continue;
        }

        LOGI(
            "[MOD] libil2cpp.so bulundu: 0x%lx",
            g_il2cppLoadBias
        );
    }

    // Küçük güvenlik beklemesi
    sleep(1);

    // --------------------------------------------------------
    // Hooklar
    // --------------------------------------------------------

    if (!InstallHooks())
    {
        LOGE(
            "[MOD] Hook kurulumu BASARISIZ."
        );

        return nullptr;
    }

    LOGI(
        "[MOD] ================================"
    );

    LOGI(
        "[MOD] Cursed House coordinate logger AKTIF"
    );

    LOGI(
        "[MOD] ================================"
    );

    return nullptr;
}

// ============================================================
// JNI ON LOAD
// ============================================================

JNIEXPORT jint JNICALL
JNI_OnLoad(
    JavaVM* vm,
    void*
)
{
    pthread_t thread;

    if (pthread_create(
            &thread,
            nullptr,
            hack_thread,
            nullptr) == 0)
    {
        pthread_detach(thread);
    }
    else
    {
        LOGE(
            "[MOD] hack_thread olusturulamadi."
        );
    }

    return JNI_VERSION_1_6;
}
