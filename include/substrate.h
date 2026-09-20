#pragma once

#include <dlfcn.h>

// Substrate-compatible runtime resolver. The mod does not link directly
// against a Substrate library; the hook implementation must be exposed by
// the runtime/injector environment.
typedef void (*MSHookFunction_t)(void* symbol, void* replace, void** result);

inline MSHookFunction_t ResolveMSHookFunction() {
    void* symbol = dlsym(RTLD_DEFAULT, "MSHookFunction");
    return reinterpret_cast<MSHookFunction_t>(symbol);
}
