#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MSHookFunction_t)(
    void* symbol,
    void* replace,
    void** result
);

MSHookFunction_t ResolveMSHookFunction(void);

#ifdef __cplusplus
}
#endif
