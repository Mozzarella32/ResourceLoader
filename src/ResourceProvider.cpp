#include "ResourceProvider.hpp"
#include "MeshData.hpp"
#include "Parsers.hpp"
#include "Resource.hpp"
#include "ResourcePtr.hpp"
#include "ResourceType.hpp"
#include "ShaderData.hpp"
#include "TextureData.hpp"
#include "Utils.hpp"

#include <Visitor.hpp>

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

auto ResourceProvider::getShaderSpirV(std::string_view key) -> std::span<const uint32_t> {
    const std::scoped_lock lock{resouceMutex};
    auto valueIter = data.find(std::string{key});
    if (valueIter == data.end()) {
        std::cerr << "ResourcePreprocessor: Failed to find shader: " << key << "\n";
        return {};
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<ShaderData>& shaderData) -> std::span<uint32_t> {
            return shaderData->data;
        },
        [&key]([[maybe_unused]]const auto& other) -> std::span<uint32_t> {
            std::cerr << "ResourcePreprocessor: getShaderSpirV with non shader resouce: " << key << "\n";
            return {};
        }
    };
    // clang-format on

    return std::visit(v, valueIter->second);
}

auto ResourceProvider::spirVGetter() -> std::function<std::span<const uint32_t>(std::string_view)> {
    return [&](std::string_view key) { return getShaderSpirV(key); };
}

auto ResourceProvider::getTextureData(std::string_view key)
    -> std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>> {
    const std::scoped_lock lock{resouceMutex};
    auto iter = data.find(std::string{key});
    if (iter == data.end()) {
        std::cerr << "ResourcePreprocessor: failed to find texture: " << key << "\n";
        return {};
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<TextureData>& textureData) ->std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>> {
        return {{textureData->width, textureData->height}, std::as_bytes(std::span{textureData->pixels})};
        },
        [&key]([[maybe_unused]]const auto& other) -> std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>{
            std::cerr << "ResourcePreprocessor: getShaderSpirV with non texture resouce: " << key << "\n";
            return {};
       }
    };
    // clang-format on

    return std::visit(v, iter->second);
}

auto ResourceProvider::textureGetter() -> std::function<
    std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>(std::string_view)> {
    return [&](std::string_view key) { return getTextureData(key); };
}

void ResourceProvider::updateResources() {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(resourcePaths.inputDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto &path = entry.path();
        const auto key = ResourcePaths::getKey(resourcePaths, path);

        if (!data.contains(key)) {
            continue;
        }

        std::uint64_t srcModificationTime = modificationTime(path.string());

        auto &someData = data.at(key);

        std::uint64_t dest_mtime = std::visit(
            Visitor{[](const auto &ptr) -> const uint64_t & { return getTimestamp(*ptr); }},
            someData);

        if (srcModificationTime <= dest_mtime) {
            continue;
        }

        auto outputPathTypeOpt = getOutputPathAndType(path, resourcePaths);
        if (!outputPathTypeOpt) {
            throw std::logic_error("ResourcePreprocessor: " + path.string() +
                                   " is no file with a "
                                   "known extension despite " +
                                   key + " beeing tracked");
        }
        const auto &[outputPath, type] = outputPathTypeOpt.value();

        std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>> resourceTimeOpt;
        switch (type) {
        case ResourceType::Shader:
            resourceTimeOpt = parseShader(path, key);
            shadersNeedUpdating = true;
            break;
        case ResourceType::Texture:
            resourceTimeOpt = parseTexture(path, key);
            texturesNeedUpdating = true;
            break;
        case ResourceType::Mesh:
            resourceTimeOpt = parseMesh(path, key);
            meshesNeedUpdate = true;
            break;
        }

        if (!resourceTimeOpt) {
            std::cerr << "ResourcePreprocessor: failed to parse " << path.string() << "\n";
            std::visit(Visitor{[&srcModificationTime](const auto &ptr) -> void {
                           setTimestamp(*ptr, srcModificationTime);
                       }},
                       someData);
            continue;
        }

        auto &[resource, parseTime] = resourceTimeOpt.value();

        Visitor v{[&srcModificationTime](const auto &ptr) -> void {
            setTimestamp(*ptr, srcModificationTime);
        }};
        std::visit(v, resource);

        data[key] = std::move(resource);

        std::cout << key << ": (Parsing: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(parseTime).count()
                  << "ms)\n";
    }
}

void ResourceProvider::work() {

    if (!std::filesystem::exists(resourcePaths.inputDir)) {
        std::cerr << "ResourcePreprocessor: No Hot shader reloading without "
                  << resourcePaths.inputDir << "\n";
        return;
    }

    while (!terminate) {
        std::this_thread::sleep_for(refreshTime);
        try {
            updateResources();
        } catch (std::filesystem::filesystem_error &err) {
            std::cerr << "ResourcePreprocessor: " << err.what() << "\n";
        }
    }
}

void ResourceProvider::startUpdater(std::chrono::milliseconds suppliedRefreshTime) {
    assert(!running);
    refreshTime = suppliedRefreshTime;

    const auto &preprocessorData = getPreprocessorData();
    for (const auto &[key, shaderData] : preprocessorData.shaders) {
        const auto shaderDataSpan = std::span(shaderData->data, shaderData->data_len);
        data[std::string(key)] = std::make_unique<ShaderData>(
            shaderData->timestamp,
            std::vector<uint32_t>{shaderDataSpan.begin(), shaderDataSpan.end()},
            shaderData->data_len);
    }
    for (const auto &[key, textureData] : preprocessorData.textures) {
        const auto textureDataSpan = std::span(textureData->pixels, textureData->pixels_len);
        data[std::string(key)] = std::make_unique<TextureData>(
            textureData->timestamp, textureData->width, textureData->height,
            std::vector<stbi_uc>{textureDataSpan.begin(), textureDataSpan.end()});
    }
    for (const auto &[key, meshData] : preprocessorData.meshes) {
        data[std::string(key)], std::make_unique<MeshData>(meshData->timestamp, meshData->attrib,
                                                           meshData->shapes, meshData->materials);
    };

    running = true;
    terminate = false;
    worker = std::thread([this]() -> void { this->work(); });
}

void ResourceProvider::stopUpdater() {
    assert(running);

    terminate = true;
    worker.join();
    running = false;
}
auto ResourceProvider::shadersNeedUpdatingClear() -> bool {
    return std::exchange(shadersNeedUpdating, false);
}
auto ResourceProvider::texturesNeedUpdatingClear() -> bool {
    return std::exchange(texturesNeedUpdating, false);
}
auto ResourceProvider::meshesNeedUpdateClear() -> bool {
    return std::exchange(meshesNeedUpdate, false);
}

ResourceProvider::~ResourceProvider() {
    if (running) {
        stopUpdater();
    }
}
