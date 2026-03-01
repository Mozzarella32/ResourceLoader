#pragma once

#include "Resource.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

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
    ResourcePreprocessor(const std::filesystem::path &inputDir,
                         const std::filesystem::path &outputDir)
        : inputDir(inputDir), outputDir(outputDir) {}

    ~ResourcePreprocessor();

  public:
    // key is in format [name]_[type]
    std::span<const uint32_t> getShaderSpirV(const std::string &key);

    std::function<std::span<const uint32_t>(const std::string &)> spirVGetter();

    const TextureData &getTextureData(const std::string &key);

    std::function<std::tuple<std::pair<uint32_t, uint32_t>, std::span<const unsigned char>>(const std::string &)>
    textureGetter();

    std::string getKey(const std::filesystem::path &path);

  public:
    void preprocess();

    using resource_ptr = std::variant<std::unique_ptr<ShaderData>, std::unique_ptr<TextureData>,
                                      std::unique_ptr<MeshData>>;

  private:
    static const constexpr std::array shaderExtensions = {
        "frag", "vert", "geom", "comp", "tesc", "tese", "spv",
    };

    static const constexpr std::array textureExtensions = {"png", "jpg"};

    static const constexpr std::array meshExtensions = {"obj"};

    std::unordered_map<std::string, resource_ptr> data;

    std::mutex resouceMutex;
    std::thread worker;

    bool terminate : 1 = false;
    bool running : 1 = false;

    void work();

  public:
    void startUpdater();
    void stopUpdater();

    bool shadersNeedUpdating = false;
    bool texturesNeedUpdating = false;
    bool meshesNeedUpdate = false;
};
