#include "ResourceLoader/private/Parsers.hpp"

#include "ResourceLoader/private/MeshData.hpp"
#include "ResourceLoader/private/ResourcePtr.hpp"
#include "ResourceLoader/private/ShaderData.hpp"
#include "ResourceLoader/private/TextureData.hpp"

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <stb_image.h>

#include <tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {
auto compileSingleShader(const std::string &shaderSource, EShLanguage type, std::string_view name)
    -> std::optional<std::tuple<std::vector<unsigned int>, std::chrono::steady_clock::duration>> {
    auto start = std::chrono::steady_clock::now();
    glslang::TProgram program;
    glslang::TShader shader(type);
    const char *c_str = shaderSource.c_str();
    shader.setStrings(&c_str, 1);
    shader.setOverrideVersion(0);

    static constinit const int vulkanGlslVersion = 100;
    shader.setEnvInput(glslang::EShSourceGlsl, type, glslang::EShClientVulkan, vulkanGlslVersion);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    // NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgEnhanced);
    // NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)

    if (!shader.parse(GetDefaultResources(), vulkanGlslVersion, false, messages)) {
        std::cerr << name << ": GLSL parsing failed:\n" << shader.getInfoLog() << '\n';
        return std::nullopt;
    }

    program.addShader(&shader);

    if (!program.link(messages)) {
        std::cerr << name << ": GLSL linking failed:\n" << program.getInfoLog() << '\n';
        return std::nullopt;
    }

    if (program.getIntermediate(type) == nullptr) {
        std::cerr << name << ": Failed to get shader intermediate after parsing" << '\n';
        return std::nullopt;
    }

    std::vector<unsigned int> spirv;
    glslang::SpvOptions spvOptions;

#ifndef NDEBUG
    spvOptions.generateDebugInfo = true;
#endif

    glslang::GlslangToSpv(*program.getIntermediate(type), spirv, nullptr, &spvOptions);
    return std::make_tuple(spirv, std::chrono::steady_clock::now() - start);
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

} // namespace

namespace ResourceLoader {
auto parseShader(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>> {

    constinit static const auto extensionToEShLanguage =
        std::to_array<std::pair<std::string_view, EShLanguage>>({
            {"frag", EShLanguage::EShLangFragment},
            {"vert", EShLanguage::EShLangVertex},
            {"geom", EShLanguage::EShLangGeometry},
            {"comp", EShLanguage::EShLangCompute},
            {"tesc", EShLanguage::EShLangTessControl},
            {"tese", EShLanguage::EShLangTessEvaluation},
        });

    const auto extension = path.extension().string().substr(1);
    if (extension == "spv") {
        throw std::logic_error(std::format(
            "ResourcePreprocessor: Currently not supported, would just need to read bytes: {}",
            path.string()));
    }

    // msvc gets a iterator not a pointer leading to deduction failiure
    // NOLINTBEGIN(readability-qualified-auto)
    const auto iter = std::ranges::find_if(
        extensionToEShLanguage, [&](const auto pair) { return pair.first == extension; });
    // NOLINTEND(readability-qualified-auto)

    if (iter == extensionToEShLanguage.end()) {
        throw std::logic_error(
            std::format("ResourcePreprocessor: parseShader on file with unsupported extension: {}",
                        path.string()));
    }

    auto vecTimeOpt = compileSingleShader(readFile(path), iter->second, name);
    if (!vecTimeOpt) {
        return std::nullopt;
    }
    auto &[vec, time] = vecTimeOpt.value();

    auto data = std::make_unique<ShaderData>();
    data->data_len = vec.size();
    data->data = std::move(vec);
    return std::make_tuple(std::move(data), time);
}

using uniqueStbPixels =
    std::unique_ptr<stbi_uc, decltype([](auto *pixels) -> void { stbi_image_free(pixels); })>;

auto parseTexture(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>> {
    std::unique_ptr<TextureData> resource = std::make_unique<TextureData>();

    int width = 0;
    int height = 0;
    int channels = 0;
    uniqueStbPixels pixels;

    auto start = std::chrono::steady_clock::now();

    pixels.reset(stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha));

    if (pixels.get() == nullptr) {
        std::cerr << name << ": parseTexture failed with stbi_load\n";
        return std::nullopt;
    }

    resource->width = width;
    resource->height = height;

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    const auto pixelsSpan = std::span(pixels.get(), pixelCount);
    resource->pixels.assign(pixelsSpan.begin(), pixelsSpan.end());
    resource->pixels_len = resource->pixels.size();
    auto time = std::chrono::steady_clock::now() - start;

    return std::make_tuple(std::move(resource), time);
}

auto parseMesh(const std::filesystem::path &path, std::string_view name)
    -> std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>> {
    std::unique_ptr<MeshData> resouce = std::make_unique<MeshData>();

    std::string warn;
    std::string err;

    auto start = std::chrono::steady_clock::now();

    if (!tinyobj::LoadObj(&resouce->attrib, &resouce->shapes, &resouce->materials, &warn, &err,
                          path.string().c_str(), path.parent_path().string().c_str())) {
        std::cerr << name << ": parsingMesh error: " << err << "\n";
        return std::nullopt;
    }
    if (!warn.empty()) {
        std::cerr << name << ": parsingMesh warning: " << warn << "\n";
    }

    return std::make_tuple(std::move(resouce), std::chrono::steady_clock::now() - start);
}
} // namespace ResourceLoader
