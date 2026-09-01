#include "ResourcePaths.hpp"

#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

void ResourcePaths::info(const ResourcePaths &resourcePaths) {
    std::cout << "ResourcePaths: Input dir: "
              << (resourcePaths.inputDir.empty() ? "<none>" : resourcePaths.inputDir) << "\n";
    std::cout << "ResourcePreprocessor: Output dir: "
              << (resourcePaths.outputDir.empty() ? "<none>" : resourcePaths.outputDir) << "\n";
}

auto ResourcePaths::getKey(const ResourcePaths &resourcePaths, const std::filesystem::path &path)
    -> std::string {
    const std::filesystem::path relative = std::filesystem::relative(path, resourcePaths.inputDir);
    std::vector<std::string> dirs =
        relative | std::views::transform([](const auto &dir) { return dir.string(); }) |
        std::ranges::to<std::vector>();
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
