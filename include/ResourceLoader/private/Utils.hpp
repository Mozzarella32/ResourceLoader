#pragma once

#include "ResourceLoader/Paths.hpp"
#include "ResourceLoader/private/ResourceType.hpp"

#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>

namespace ResourceLoader {
auto modificationTime(const std::filesystem::path &path) -> std::time_t;

auto getOutputPathAndType(const std::filesystem::path &path, const Paths &resourcePaths)
    -> std::optional<std::tuple<std::filesystem::path, ResourceType>>;

auto readFile(const std::filesystem::path &path) -> std::string;
} // namespace ResourceLoader
