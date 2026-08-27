#pragma once

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
struct ShaderData_c {
#else
typedef struct {
#endif
    uint64_t timestamp;
    const uint32_t *data;
    uint64_t data_len;
#ifdef __cplusplus
};
#else
} ShaderData_c;
#endif
