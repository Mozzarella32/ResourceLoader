#pragma once

#include <filesystem>
#include <string>

namespace ResourceLoader {
struct Paths {
    std::filesystem::path inputDir;
    std::filesystem::path outputDir;

    static void info(const Paths &resourcePaths);

    static auto getKey(const Paths &resourcePaths, const std::filesystem::path &path)
        -> std::string;
};
}
