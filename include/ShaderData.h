#pragma once

#include <stdint.h>

typedef struct {
    uint64_t timestamp;
    const uint32_t *data;
    uint64_t data_len;
} ShaderData_c;
