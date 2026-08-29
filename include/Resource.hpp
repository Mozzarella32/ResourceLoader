#pragma once

#include "MeshData.hpp"
#include "ShaderData.h"
#include "TextureData.h"

#include <stb_image.h>

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>

struct PreprocessorDataHolder {
  private:
    struct Data {
        std::span<const std::tuple<std::string_view, ShaderData_c *>> shaders;
        std::span<const std::tuple<std::string_view, TextureData_c *>> textures;
        std::span<const std::tuple<std::string_view, MeshData *>> meshes;
    };
    std::optional<Data> data;

    PreprocessorDataHolder() = default;
    static auto Instance() -> PreprocessorDataHolder & {
        static PreprocessorDataHolder instance;
        return instance;
    }

  public:
    static auto getData() -> const Data & {
        if (auto &dataOpt = Instance().data; dataOpt) {
            return dataOpt.value();
        }
        throw std::logic_error(
            "ResourceLoader: Data has not been set, check if Resource.cpp was compiled");
    }
    static void setData(const Data &data) noexcept { Instance().data = data; }
};

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
