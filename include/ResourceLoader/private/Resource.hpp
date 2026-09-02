#pragma once

#include "MeshData.hpp"
#include "ShaderData.h"
#include "TextureData.h"

#include <stb_image.h>

#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <tuple>

struct PreprocessorData {
    std::span<const std::tuple<std::string_view, ShaderData_c *>> shaders;
    std::span<const std::tuple<std::string_view, TextureData_c *>> textures;
    std::span<const std::tuple<std::string_view, MeshData *>> meshes;
};

extern auto getPreprocessorData() -> PreprocessorData;

template <typename T>
concept hasTimestamp = requires(T value) {
    { value.timestamp } -> std::same_as<uint64_t &>;
};

template <hasTimestamp T> void setTimestamp(T &value, const uint64_t &timestamp) {
    value.timestamp = timestamp;
}

template <hasTimestamp T> auto getTimestamp(const T &value) -> const uint64_t & {
    return value.timestamp;
}
