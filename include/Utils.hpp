#pragma once

#include "ResourcePaths.hpp"
#include "ResourceType.hpp"

#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>

auto modificationTime(const std::filesystem::path &path) -> std::time_t;

auto getOutputPathAndType(const std::filesystem::path &path, const ResourcePaths &resourcePaths)
    -> std::optional<std::tuple<std::filesystem::path, ResourceType>>;

auto readFile(const std::filesystem::path &path) -> std::string;
