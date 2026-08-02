#pragma once

#include <stb_image.h>
#include <stdint.h>

typedef struct {
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    const stbi_uc *pixels;
    uint64_t pixels_len;
} TextureData_c;
