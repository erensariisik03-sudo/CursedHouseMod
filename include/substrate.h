#pragma once
// Compatibility declarations retained for the existing source layout.
#include <stddef.h>

typedef int (*DobbyHook_t)(void* address, void* replace_func, void** out_origin_func);

struct HookBackend {
    enum Kind { NONE = 0, SUBSTRATE = 1, DOBBY = 2 } kind = NONE;
    DobbyHook_t dobby = nullptr;
    void* ownerHandle = nullptr;
    const char* ownerName = nullptr;

    bool valid() const { return dobby != nullptr; }
};
