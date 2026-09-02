#pragma once

#include <stb_image.h>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
struct TextureData_c {
#else
typedef struct {
#endif
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    const stbi_uc *pixels;
    uint64_t pixels_len;
#ifdef __cplusplus
};
#else
} TextureData_c;
#endif
