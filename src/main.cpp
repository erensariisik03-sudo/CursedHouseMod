#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "arm_hook.h"

#define LOG_TAG "CursedHouseMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
// SETTINGS
// ============================================================

// false = sadece koordinat/instance tespiti
// true  = local player bulunduğunda mevcut spider GameObject'ini
//         local player'ın bulunduğu yere BİR KEZ kopyalar.
//
// Test için varsayılan olarak KAPALI bırakıldı.
static constexpr bool AUTO_SPAWN_TEST_SPIDER = false;

static constexpr uint64_t COORD_INTERVAL_NS = 5000000000ULL;

// ============================================================
// TYPES
// ============================================================

struct Vector3
{
    float x;
    float y;
    float z;
};

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

// ============================================================
// IL2CPP RVAs - dump_dosyasi.cs
// ============================================================

// playerController::Update()
static constexpr uintptr_t RVA_PlayerController_Update =
    0x10D24DC;

// playerController private PhotonView pv; // 0x2A0
static constexpr uintptr_t OFF_PlayerController_PV =
    0x2A0;

// PhotonView::get_IsMine()
static constexpr uintptr_t RVA_PhotonView_get_IsMine =
    0x2AA4604;

// spiderControlMulti::Update()
static constexpr uintptr_t RVA_SpiderControlMulti_Update =
    0x112571C;

// UnityEngine.Component::get_transform()
static constexpr uintptr_t RVA_Component_get_transform =
    0x3584684;

// UnityEngine.Component::get_gameObject()
static constexpr uintptr_t RVA_Component_get_gameObject =
    0x358485C;

// UnityEngine.Transform::get_position()
static constexpr uintptr_t RVA_Transform_get_position =
    0x359B380;

// UnityEngine.Transform::set_position()
static constexpr uintptr_t RVA_Transform_set_position =
    0x359B4A0;

// UnityEngine.Object::Instantiate(Object, Vector3, Quaternion)
static constexpr uintptr_t RVA_Object_Instantiate_PosRot =
    0x3590508;

// UnityEngine.Object::Destroy(Object)
static constexpr uintptr_t RVA_Object_Destroy =
    0x3591590;

// ============================================================
// STATE
// ============================================================

static uintptr_t g_il2cppLoadBias = 0;

static void* g_localPlayerController = nullptr;
static void* g_spiderInstance = nullptr;

static bool g_localPlayerFoundLogged = false;
static bool g_spiderFoundLogged = false;
static bool g_autoSpawnDone = false;

static uint64_t g_lastCoordinateDumpNs = 0;

// ============================================================
// FUNCTION POINTERS
// ============================================================

using PlayerControllerUpdate_t =
    void (*)(void* instance);

using SpiderControlMultiUpdate_t =
    void (*)(void* instance);

using PhotonViewGetIsMine_t =
    bool (*)(void* instance);

using ComponentGetTransform_t =
    void* (*)(void* instance);

using ComponentGetGameObject_t =
    void* (*)(void* instance);

using TransformGetPosition_t =
    Vector3 (*)(void* instance);

using TransformSetPosition_t =
    void (*)(void* instance, Vector3 position);

using ObjectInstantiatePosRot_t =
    void* (*)(void* original, Vector3 position, Quaternion rotation);

using ObjectDestroy_t =
    void (*)(void* object);

static PlayerControllerUpdate_t orig_PlayerController_Update = nullptr;
static SpiderControlMultiUpdate_t orig_SpiderControlMulti_Update = nullptr;

static PhotonViewGetIsMine_t fn_PhotonView_get_IsMine = nullptr;
static ComponentGetTransform_t fn_Component_get_transform = nullptr;
static ComponentGetGameObject_t fn_Component_get_gameObject = nullptr;
static TransformGetPosition_t fn_Transform_get_position = nullptr;
static TransformSetPosition_t fn_Transform_set_position = nullptr;
static ObjectInstantiatePosRot_t fn_Object_Instantiate_PosRot = nullptr;
static ObjectDestroy_t fn_Object_Destroy = nullptr;

// ============================================================
// HELPERS
// ============================================================

static uintptr_t GetModuleBase(const char* moduleName)
{
    FILE* fp = fopen("/proc/self/maps", "r");

    if (!fp)
        return 0;

    char line[512];

    while (fgets(line, sizeof(line), fp))
    {
        if (!strstr(line, moduleName))
            continue;

        uintptr_t base = 0;
        uintptr_t end = 0;

        if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR, &base, &end) == 2)
        {
            fclose(fp);
            return base;
        }
    }

    fclose(fp);
    return 0;
}

static uintptr_t RvaToAddress(uintptr_t rva)
{
    if (!g_il2cppLoadBias)
        return 0;

    return g_il2cppLoadBias + rva;
}

static uint64_t GetMonotonicNs()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return
        static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
        static_cast<uint64_t>(ts.tv_nsec);
}

static bool IsValidPtr(void* p)
{
    return p != nullptr;
}

// ============================================================
// COORDINATE DUMP
// ============================================================

static bool GetWorldPosition(void* component, Vector3& outPosition)
{
    if (!component)
        return false;

    if (!fn_Component_get_transform)
        return false;

    if (!fn_Transform_get_position)
        return false;

    void* transform =
        fn_Component_get_transform(component);

    if (!transform)
        return false;

    outPosition =
        fn_Transform_get_position(transform);

    return true;
}

static void DumpLocalPlayerPosition()
{
    if (!g_localPlayerController)
        return;

    Vector3 pos{};

    if (!GetWorldPosition(g_localPlayerController, pos))
    {
        LOGE("[COORD] local player Transform/position okunamadi");
        return;
    }

    LOGI("----------------------------------------");
    LOGI("[COORD] LOCAL PLAYER");
    LOGI("[COORD] X = %.4f", pos.x);
    LOGI("[COORD] Y = %.4f", pos.y);
    LOGI("[COORD] Z = %.4f", pos.z);
    LOGI("[COORD] { %.4f, %.4f, %.4f }", pos.x, pos.y, pos.z);
    LOGI("----------------------------------------");
}

static void DumpSpiderPosition()
{
    if (!g_spiderInstance)
        return;

    Vector3 pos{};

    if (!GetWorldPosition(g_spiderInstance, pos))
    {
        LOGE("[SPIDER] spider Transform/position okunamadi");
        return;
    }

    LOGI("[SPIDER] INSTANCE=%p", g_spiderInstance);
    LOGI("[SPIDER] POSITION { %.4f, %.4f, %.4f }",
         pos.x, pos.y, pos.z);
}

// ============================================================
// TEST SPIDER CLONE
// ============================================================

static void SpawnTestSpiderAt(Vector3 position)
{
    if (!g_spiderInstance)
    {
        LOGE("[SPAWN] spiderInstance henuz bulunmadi");
        return;
    }

    if (!fn_Component_get_gameObject)
    {
        LOGE("[SPAWN] Component.get_gameObject null");
        return;
    }

    if (!fn_Object_Instantiate_PosRot)
    {
        LOGE("[SPAWN] Object.Instantiate null");
        return;
    }

    void* spiderGameObject =
        fn_Component_get_gameObject(g_spiderInstance);

    if (!spiderGameObject)
    {
        LOGE("[SPAWN] spider GameObject alinamadi");
        return;
    }

    // Identity rotation.
    Quaternion rotation{};
    rotation.x = 0.0f;
    rotation.y = 0.0f;
    rotation.z = 0.0f;
    rotation.w = 1.0f;

    void* clone =
        fn_Object_Instantiate_PosRot(
            spiderGameObject,
            position,
            rotation
        );

    if (clone)
    {
        LOGI("[SPAWN] Test spider clone OLUSTU: %p", clone);
        LOGI("[SPAWN] POS { %.4f, %.4f, %.4f }",
             position.x,
             position.y,
             position.z);
    }
    else
    {
        LOGE("[SPAWN] Object.Instantiate NULL dondurdu");
    }
}

// Bu fonksiyon ileride menu butonuna baglanacak.
static void RequestTestSpiderSpawn()
{
    if (!g_localPlayerController)
    {
        LOGE("[SPAWN] local player henuz bulunmadi");
        return;
    }

    Vector3 playerPosition{};

    if (!GetWorldPosition(g_localPlayerController, playerPosition))
    {
        LOGE("[SPAWN] local player koordinati okunamadi");
        return;
    }

    SpawnTestSpiderAt(playerPosition);
}

// ============================================================
// PLAYER UPDATE HOOK
// ============================================================

static void Hooked_PlayerController_Update(void* instance)
{
    if (instance)
    {
        // playerController::pv @ 0x2A0
        void* pv =
            *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(instance) +
                OFF_PlayerController_PV
            );

        if (pv && fn_PhotonView_get_IsMine)
        {
            const bool isMine =
                fn_PhotonView_get_IsMine(pv);

            if (isMine)
            {
                if (g_localPlayerController != instance)
                {
                    g_localPlayerController = instance;

                    if (!g_localPlayerFoundLogged)
                    {
                        g_localPlayerFoundLogged = true;

                        LOGI(
                            "[PLAYER] LOCAL playerController bulundu: %p",
                            instance
                        );
                    }

                    // Sadece test amaçlı; varsayılan FALSE.
                    if (AUTO_SPAWN_TEST_SPIDER &&
                        !g_autoSpawnDone &&
                        g_spiderInstance)
                    {
                        g_autoSpawnDone = true;
                        RequestTestSpiderSpawn();
                    }
                }
            }
        }
    }

    // Oyunun kendi Update'i.
    if (orig_PlayerController_Update)
        orig_PlayerController_Update(instance);

    if (!g_localPlayerController)
        return;

    if (instance != g_localPlayerController)
        return;

    const uint64_t now = GetMonotonicNs();

    if (g_lastCoordinateDumpNs == 0 ||
        now - g_lastCoordinateDumpNs >= COORD_INTERVAL_NS)
    {
        g_lastCoordinateDumpNs = now;

        DumpLocalPlayerPosition();
        DumpSpiderPosition();

        // Spawn test'i spider daha sonra bulunduysa da çalıştır.
        if (AUTO_SPAWN_TEST_SPIDER &&
            !g_autoSpawnDone &&
            g_spiderInstance)
        {
            g_autoSpawnDone = true;
            RequestTestSpiderSpawn();
        }
    }
}

// ============================================================
// SPIDER UPDATE HOOK
// ============================================================

static void Hooked_SpiderControlMulti_Update(void* instance)
{
    if (instance)
    {
        g_spiderInstance = instance;

        if (!g_spiderFoundLogged)
        {
            g_spiderFoundLogged = true;

            LOGI(
                "[SPIDER] spiderControlMulti bulundu: %p",
                instance
            );

            LOGI(
                "[SPIDER] Update RVA = 0x%lx",
                static_cast<unsigned long>(RVA_SpiderControlMulti_Update)
            );
        }
    }

    if (orig_SpiderControlMulti_Update)
        orig_SpiderControlMulti_Update(instance);
}

// ============================================================
// INSTALL
// ============================================================

static bool InstallHooks()
{
    if (!g_il2cppLoadBias)
        return false;

    const uintptr_t addrPlayerUpdate =
        RvaToAddress(RVA_PlayerController_Update);

    const uintptr_t addrIsMine =
        RvaToAddress(RVA_PhotonView_get_IsMine);

    const uintptr_t addrSpiderUpdate =
        RvaToAddress(RVA_SpiderControlMulti_Update);

    const uintptr_t addrGetTransform =
        RvaToAddress(RVA_Component_get_transform);

    const uintptr_t addrGetGameObject =
        RvaToAddress(RVA_Component_get_gameObject);

    const uintptr_t addrGetPosition =
        RvaToAddress(RVA_Transform_get_position);

    const uintptr_t addrSetPosition =
        RvaToAddress(RVA_Transform_set_position);

    const uintptr_t addrInstantiate =
        RvaToAddress(RVA_Object_Instantiate_PosRot);

    const uintptr_t addrDestroy =
        RvaToAddress(RVA_Object_Destroy);

    if (!addrPlayerUpdate ||
        !addrIsMine ||
        !addrSpiderUpdate ||
        !addrGetTransform ||
        !addrGetGameObject ||
        !addrGetPosition ||
        !addrInstantiate)
    {
        LOGE("[HOOK] Gerekli RVA adreslerinden biri 0 dondu");
        return false;
    }

    fn_PhotonView_get_IsMine =
        reinterpret_cast<PhotonViewGetIsMine_t>(addrIsMine);

    fn_Component_get_transform =
        reinterpret_cast<ComponentGetTransform_t>(addrGetTransform);

    fn_Component_get_gameObject =
        reinterpret_cast<ComponentGetGameObject_t>(addrGetGameObject);

    fn_Transform_get_position =
        reinterpret_cast<TransformGetPosition_t>(addrGetPosition);

    fn_Transform_set_position =
        reinterpret_cast<TransformSetPosition_t>(addrSetPosition);

    fn_Object_Instantiate_PosRot =
        reinterpret_cast<ObjectInstantiatePosRot_t>(addrInstantiate);

    if (addrDestroy)
    {
        fn_Object_Destroy =
            reinterpret_cast<ObjectDestroy_t>(addrDestroy);
    }

    if (!ArmHook(
            reinterpret_cast<void*>(addrPlayerUpdate),
            reinterpret_cast<void*>(Hooked_PlayerController_Update),
            reinterpret_cast<void**>(&orig_PlayerController_Update)))
    {
        LOGE("[HOOK] playerController.Update BASARISIZ");
        return false;
    }

    if (!ArmHook(
            reinterpret_cast<void*>(addrSpiderUpdate),
            reinterpret_cast<void*>(Hooked_SpiderControlMulti_Update),
            reinterpret_cast<void**>(&orig_SpiderControlMulti_Update)))
    {
        LOGE("[HOOK] spiderControlMulti.Update BASARISIZ");
        return false;
    }

    LOGI("[HOOK] playerController.Update = 0x%" PRIxPTR, addrPlayerUpdate);
    LOGI("[HOOK] spiderControlMulti.Update = 0x%" PRIxPTR, addrSpiderUpdate);
    LOGI("[HOOK] Component.get_transform = 0x%" PRIxPTR, addrGetTransform);
    LOGI("[HOOK] Component.get_gameObject = 0x%" PRIxPTR, addrGetGameObject);
    LOGI("[HOOK] Transform.get_position = 0x%" PRIxPTR, addrGetPosition);
    LOGI("[HOOK] Object.Instantiate = 0x%" PRIxPTR, addrInstantiate);

    if (addrDestroy)
        LOGI("[HOOK] Object.Destroy = 0x%" PRIxPTR, addrDestroy);

    LOGI("[MOD] Coordinate logger + spider test hazir");
    LOGI("[MOD] AUTO_SPAWN_TEST_SPIDER = %s",
         AUTO_SPAWN_TEST_SPIDER ? "TRUE" : "FALSE");

    return true;
}

// ============================================================
// THREAD
// ============================================================

static void* hack_thread(void*)
{
    LOGI("[MOD] hack_thread basladi");

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
            "[MOD] libil2cpp.so base = 0x%" PRIxPTR,
            g_il2cppLoadBias
        );
    }

    sleep(1);

    if (!InstallHooks())
    {
        LOGE("[MOD] Hook kurulumu BASARISIZ");
        return nullptr;
    }

    return nullptr;
}

// ============================================================
// JNI
// ============================================================

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*)
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
        LOGE("[MOD] hack_thread olusturulamadi");
    }

    return JNI_VERSION_1_6;
}
