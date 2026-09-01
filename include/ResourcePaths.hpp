#pragma once

#include <filesystem>
#include <string>

struct ResourcePaths {
    std::filesystem::path inputDir;
    std::filesystem::path outputDir;

    static void info(const ResourcePaths &resourcePaths);

    static auto getKey(const ResourcePaths &resourcePaths, const std::filesystem::path &path)
        -> std::string;
};
