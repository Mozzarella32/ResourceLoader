#include "ResourceLoader/private/Utils.hpp"

#include "ResourceLoader/Paths.hpp"
#include "ResourceLoader/private/ResourceType.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>

namespace ResourceLoader {
auto modificationTime(const std::filesystem::path &path) -> std::time_t {
    if (!std::filesystem::exists(path)) {
        return 0;
    }
    auto last_write = std::filesystem::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        last_write - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

auto getOutputPathAndType(const std::filesystem::path &path, const Paths &resourcePaths)
    -> std::optional<std::tuple<std::filesystem::path, ResourceType>> {
    const auto extension = path.extension().string().substr(1);
    const auto basePath =
        (resourcePaths.outputDir / std::filesystem::relative(path, resourcePaths.inputDir))
            .remove_filename();
    const auto stem = path.stem();

    const constexpr std::array shaderExtensions = {
        "frag", "vert", "geom", "comp", "tesc", "tese", "spv",
    };

    const constexpr std::array textureExtensions = {"png", "jpg"};

    const constexpr std::array meshExtensions = {"obj"};

    if (std::ranges::find(shaderExtensions, extension) != shaderExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".c"),
                               ResourceType::Shader);
    }
    if (std::ranges::find(textureExtensions, extension) != textureExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".c"),
                               ResourceType::Texture);
    }
    if (std::ranges::find(meshExtensions, extension) != meshExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".cpp"),
                               ResourceType::Mesh);
    }
    return std::nullopt;
}

auto readFile(const std::filesystem::path &path) -> std::string {
    const std::ifstream file(path);

    if (!file) {
        return "";
    }

    std::string contents;
    if (file) {
        std::ostringstream buff;
        buff << file.rdbuf();
        contents = buff.str();
    }
    return contents;
}
} // namespace ResourceLoader
