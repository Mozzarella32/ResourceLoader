#pragma once

#include <stb_image.h>

#include <cstdint>

#include "MeshData.hpp"
#include "ShaderData.h"
#include "TextureData.h"

#include <concepts>
#include <unordered_map>
#include <variant>

struct PreprocessorDataHolder {
  private:
    std::unordered_map<std::string, std::variant<ShaderData_c *, TextureData_c *, MeshData *>> data;
    PreprocessorDataHolder() {};
    static PreprocessorDataHolder &Instance() {
        static PreprocessorDataHolder instance;
        return instance;
    }

  public:
    static std::unordered_map<std::string,
                              std::variant<ShaderData_c *, TextureData_c *, MeshData *>> &
    getData() {
        return Instance().data;
    }
    static void setData(
        const std::unordered_map<std::string,
                                 std::variant<ShaderData_c *, TextureData_c *, MeshData *>> &data) {
        Instance().data = data;
    }
};

template <typename T>
concept hasTimestamp = requires(T t) {
    { t.timestamp } -> std::same_as<uint64_t &>;
};

template <hasTimestamp T> void setTimestamp(T &t, const uint64_t &timestamp) {
    t.timestamp = timestamp;
}

template <hasTimestamp T> const uint64_t &getTimestamp(const T &t) { return t.timestamp; }
