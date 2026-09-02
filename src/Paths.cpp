#include "ResourceLoader/Paths.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace ResourceLoader {
void Paths::info(const Paths &resourcePaths) {
    std::cout << "ResourcePaths: Input dir: "
              << (resourcePaths.inputDir.empty() ? "<none>" : resourcePaths.inputDir) << "\n";
    std::cout << "ResourcePreprocessor: Output dir: "
              << (resourcePaths.outputDir.empty() ? "<none>" : resourcePaths.outputDir) << "\n";
}

auto Paths::getKey(const Paths &resourcePaths, const std::filesystem::path &path) -> std::string {
    const std::filesystem::path relative = std::filesystem::relative(path, resourcePaths.inputDir);
    std::vector<std::string> dirs;
    for (const auto &dir : relative) {
        dirs.push_back(dir.string());
    }
    dirs.pop_back();
    std::string key;
    for (const auto &dir : dirs) {
        key += dir;
        key += "_";
    }
    key += relative.stem().string();
    key += "_";
    const auto extension = relative.extension().string().substr(1);
    key += extension;
    return key;
}
} // namespace ResourceLoader
