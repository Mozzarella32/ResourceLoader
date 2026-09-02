#pragma once

#include <stb_image.h>

#include <cstdint>
#include <vector>

struct TextureData {
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    std::vector<stbi_uc> pixels;
    uint64_t pixels_len;
};
