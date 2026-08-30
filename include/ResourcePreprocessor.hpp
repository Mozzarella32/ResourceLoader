#pragma once
#include "MeshData.hpp"

#include <stb_image.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct ShaderData {
    uint64_t timestamp;
    std::vector<uint32_t> data;
    uint64_t data_len;
};

struct TextureData {
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    std::vector<stbi_uc> pixels;
    uint64_t pixels_len;
};

class ResourcePreprocessor {
  public:
    void info() const;

  private:
    std::filesystem::path inputDir;
    std::filesystem::path outputDir;

  public:
    enum class ResourceType : std::uint8_t { Shader, Texture, Mesh };

    [[nodiscard]] auto getInputDir() const -> const std::filesystem::path &;
    [[nodiscard]] auto getOutputDir() const -> const std::filesystem::path &;

    ResourcePreprocessor(std::filesystem::path inputDir, std::filesystem::path outputDir)
        : inputDir(std::move(inputDir)), outputDir(std::move(outputDir)) {}

    ResourcePreprocessor(const ResourcePreprocessor &) noexcept = delete;
    ResourcePreprocessor(ResourcePreprocessor &&) noexcept = delete;
    auto operator=(const ResourcePreprocessor &) noexcept = delete;
    auto operator=(ResourcePreprocessor &&) noexcept = delete;

    ~ResourcePreprocessor();

    // key is in format [name]_[type]
    auto getShaderSpirV(std::string_view key) -> std::span<const uint32_t>;

    auto spirVGetter() -> std::function<std::span<const uint32_t>(std::string_view)>;

    auto getTextureData(std::string_view key) -> const TextureData &;

    auto textureGetter() -> std::function<
        std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>(std::string_view)>;

    auto getKey(const std::filesystem::path &path) -> std::string;

    void preprocess();

    using resource_ptr = std::variant<std::unique_ptr<ShaderData>, std::unique_ptr<TextureData>,
                                      std::unique_ptr<MeshData>>;

  private:
    std::unordered_map<std::string, resource_ptr> data;

    std::mutex resouceMutex;
    std::thread worker;

    bool terminate : 1 = false;
    bool running : 1 = false;

    bool shadersNeedUpdating = false;
    bool texturesNeedUpdating = false;
    bool meshesNeedUpdate = false;

    std::chrono::milliseconds refreshTime = defaultRefreshTime;

    void work();

  public:
    static const constexpr auto defaultRefreshTime = std::chrono::milliseconds(16);

    void startUpdater(std::chrono::milliseconds refreshTime = defaultRefreshTime);
    void stopUpdater();

    auto shadersNeedUpdatingClear() -> bool;
    auto texturesNeedUpdatingClear() -> bool;
    auto meshesNeedUpdateClear() -> bool;
};
