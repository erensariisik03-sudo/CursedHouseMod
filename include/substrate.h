#pragma once

#include <dlfcn.h>
#include <stddef.h>

// Substrate-compatible API resolved at runtime. No hard link to Substrate.
typedef void (*MSHookFunction_t)(void* symbol, void* replace, void** result);

// Dobby API (optional fallback when a Dobby-based loader already has it loaded).
typedef int (*DobbyHook_t)(void* address, void* replace_func, void** out_origin_func);

struct HookBackend {
    enum Kind { NONE = 0, SUBSTRATE = 1, DOBBY = 2 } kind = NONE;
    MSHookFunction_t substrate = nullptr;
    DobbyHook_t dobby = nullptr;
    void* ownerHandle = nullptr;
    const char* ownerName = nullptr;

    bool valid() const {
        return substrate != nullptr || dobby != nullptr;
    }
};

inline MSHookFunction_t ResolveMSHookFunctionDefault() {
    return reinterpret_cast<MSHookFunction_t>(dlsym(RTLD_DEFAULT, "MSHookFunction"));
}

inline DobbyHook_t ResolveDobbyHookDefault() {
    return reinterpret_cast<DobbyHook_t>(dlsym(RTLD_DEFAULT, "DobbyHook"));
}
