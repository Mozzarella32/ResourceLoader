#pragma once

#include "ResourceLoader/private/ResourcePtr.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <tuple>

namespace ResourceLoader {
auto parseShader(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>>;

auto parseTexture(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>>;

auto parseMesh(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>>;
} // namespace ResourceLoader
