#pragma once

#include <dlfcn.h>

// Minimal runtime resolver for a Substrate-compatible MSHookFunction export.
// The project does not need to link against a Substrate library at build time.
typedef void (*MSHookFunction_t)(void* symbol, void* replace, void** result);

inline MSHookFunction_t ResolveMSHookFunction() {
    void* symbol = dlsym(RTLD_DEFAULT, "MSHookFunction");
    return reinterpret_cast<MSHookFunction_t>(symbol);
}
