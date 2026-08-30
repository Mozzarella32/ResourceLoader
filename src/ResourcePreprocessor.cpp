#include "ResourcePreprocessor.hpp"
#include "MeshData.hpp"
#include "Resource.hpp"
#include "Serialiser.hpp"

#include <Visitor.hpp>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <stdexcept>
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cassert>
#include <chrono>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define stat _stat
#endif

namespace {
auto get_mtime_ms(const std::string &filename) -> uint64_t {
    struct stat result{};
    if (stat(filename.c_str(), &result) != 0) {
        return 0;
    }

#ifdef __APPLE__
    const uint64_t sec = result.st_mtimespec.tv_sec;
    cosnt uint64_t nsec = result.st_mtimespec.tv_nsec;
#elifdef _POSIX_VERSION
#if defined(st_mtim)
    const uint64_t sec = result.st_mtim.tv_sec;
    const uint64_t nsec = result.st_mtim.tv_nsec;
#else
    const uint64_t sec = result.st_mtime;
    const uint64_t nsec = 0;
#endif
#else
    const uint64_t sec = result.st_mtime;
    const uint64_t nsec = 0;
#endif

    static constinit const uint64_t secToMsec = 1000ULL;
    static constinit const uint64_t nsecTomsec = 1000000ULL;
    return (sec * secToMsec) + (nsec / nsecTomsec);
}
} // namespace

void ResourcePreprocessor::info() const {
    std::cout << "Input dir: " << (inputDir.empty() ? "<none>" : inputDir) << "\n";
    std::cout << "Output dir: " << (outputDir.empty() ? "<none>" : outputDir) << "\n";
}
auto ResourcePreprocessor::getInputDir() const -> const std::filesystem::path & { return inputDir; }
auto ResourcePreprocessor::getOutputDir() const -> const std::filesystem::path & {
    return outputDir;
}

namespace {
auto compileSingleShader(const std::string &shaderSource, EShLanguage type, const std::string &name)
    -> std::optional<std::vector<unsigned int>> {
    std::cout << "Compiling " << name << " (";
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

    auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgEnhanced);

    if (!shader.parse(GetDefaultResources(), vulkanGlslVersion, false, messages)) {
        std::cerr << "GLSL parsing failed:\n" << shader.getInfoLog() << '\n';
        return std::nullopt;
    }

    program.addShader(&shader);

    if (!program.link(messages)) {
        std::cerr << "GLSL linking failed:\n" << program.getInfoLog() << '\n';
        return std::nullopt;
    }

    if (program.getIntermediate(type) == nullptr) {
        std::cerr << "Failed to get shader intermediate after parsing." << '\n';
        return std::nullopt;
    }

    std::vector<unsigned int> spirv;
    glslang::SpvOptions spvOptions;

#ifndef NDEBUG
    spvOptions.generateDebugInfo = true;
#endif

    glslang::GlslangToSpv(*program.getIntermediate(type), spirv, nullptr, &spvOptions);
    std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start));
    return spirv;
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

auto ResourcePreprocessor::getShaderSpirV(std::string_view key) -> std::span<const uint32_t> {
    const std::unique_lock uniqueLock{resouceMutex};
    auto valueIter = data.find(std::string{key});
    if (valueIter == data.end()) {
        std::cerr << "failed to find shader: " << key << "\n";
        return {};
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<ShaderData>& shaderData) -> std::span<uint32_t> {
            return shaderData->data;
        },
        [&key]([[maybe_unused]]const auto& other) -> std::span<uint32_t> {
            std::cerr << "getShaderSpirV with non shader resouce: " << key << "\n";
            return {};
        }
    };
    // clang-format on

    return std::visit(v, valueIter->second);
}

auto ResourcePreprocessor::spirVGetter()
    -> std::function<std::span<const uint32_t>(std::string_view)> {
    return [&](std::string_view key) -> std::span<const uint32_t> { return getShaderSpirV(key); };
}

auto ResourcePreprocessor::getTextureData(std::string_view key) -> const TextureData & {
    static TextureData empty;

    const std::unique_lock uniqueLock{resouceMutex};
    auto iter = data.find(std::string{key});
    if (iter == data.end()) {
        std::cerr << "failed to find textureData: " << key << "\n";
        return empty;
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<TextureData>& textureData) -> const auto& {
            return *textureData;
        },
        [&key]([[maybe_unused]]const auto& other) -> const auto& {
            std::cerr << "getShaderSpirV with non shader resouce: " << key << "\n";
            return empty;
       }
    };
    // clang-format on

    return std::visit(v, iter->second);
}

auto ResourcePreprocessor::textureGetter() -> std::function<
    std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>(std::string_view)> {
    return [&](std::string_view key)
               -> std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>> {
        const TextureData &data = getTextureData(key);
        return {{data.width, data.height}, std::as_bytes(std::span{data.pixels})};
    };
}
auto ResourcePreprocessor::getKey(const std::filesystem::path &path) -> std::string {
    const std::filesystem::path relative = std::filesystem::relative(path, inputDir);
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

namespace {
auto parseShader(const std::filesystem::path &path, const std::string &key)
    -> std::optional<ResourcePreprocessor::resource_ptr> {

    std::optional<std::vector<uint32_t>> vecOpt;

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

    const auto *iter = std::ranges::find_if(
        extensionToEShLanguage, [&](const auto pair) { return pair.first == extension; });

    if (iter == extensionToEShLanguage.end()) {
        throw std::logic_error(
            std::format("ResourcePreprocessor: parseShader on file with unsupported extension: {}",
                        path.string()));
    }

    vecOpt = compileSingleShader(readFile(path), iter->second, key);

    if (!vecOpt) {
        return std::nullopt;
    }

    auto data = std::make_unique<ShaderData>();
    data->data_len = vecOpt.value().size();
    data->data = std::move(vecOpt.value());
    return data;
}

using uniqueStbPixels =
    std::unique_ptr<stbi_uc, decltype([](auto *pixels) -> void { stbi_image_free(pixels); })>;

auto parseTexture(const std::filesystem::path &path, const std::string &name)
    -> std::optional<ResourcePreprocessor::resource_ptr> {
    std::unique_ptr<TextureData> resource = std::make_unique<TextureData>();

    int width = 0;
    int height = 0;
    int channels = 0;
    uniqueStbPixels pixels;

    std::cout << "Decoding " << name << " (";
    auto start = std::chrono::steady_clock::now();

    pixels.reset(stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha));
    std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start));

    if (pixels.get() == nullptr) {
        return std::nullopt;
    }

    resource->width = width;
    resource->height = height;

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    resource->pixels.assign_range(std::span(pixels.get(), pixelCount));
    resource->pixels_len = resource->pixels.size();
    return resource;
}

auto parseMesh(const std::filesystem::path &path, const std::string &name)
    -> std::optional<ResourcePreprocessor::resource_ptr> {
    std::unique_ptr<MeshData> resouce = std::make_unique<MeshData>();

    std::string warn;
    std::string err;

    std::cout << "Constructing " << name << " (";
    auto start = std::chrono::steady_clock::now();

    if (!tinyobj::LoadObj(&resouce->attrib, &resouce->shapes, &resouce->materials, &warn, &err,
                          path.string().c_str(), path.parent_path().string().c_str())) {
        std::cerr << "\nError: " << err << "\n";
        return std::nullopt;
    }
    if (!warn.empty()) {
        std::cout << "\nWarning: " << warn << "\n";
    }

    std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start));

    return resouce;
}

auto getArtefactAndType(const std::filesystem::path &path,
                        ResourcePreprocessor &resourcePreprocessor)
    -> std::optional<std::tuple<std::filesystem::path, ResourcePreprocessor::ResourceType>> {
    const auto extension = path.extension().string().substr(1);
    const auto basePath = (resourcePreprocessor.getOutputDir() /
                           std::filesystem::relative(path, resourcePreprocessor.getInputDir()))
                              .remove_filename();
    const auto stem = path.stem();

    const constexpr std::array shaderExtensions = {
        "frag", "vert", "geom", "comp", "tesc", "tese", "spv",
    };

    const constexpr std::array textureExtensions = {"png", "jpg"};

    const constexpr std::array meshExtensions = {"obj"};

    if (std::ranges::find(shaderExtensions, extension) != shaderExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".c"),
                               ResourcePreprocessor::ResourceType::Shader);
    }
    if (std::ranges::find(textureExtensions, extension) != textureExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".c"),
                               ResourcePreprocessor::ResourceType::Texture);
    }
    if (std::ranges::find(meshExtensions, extension) != meshExtensions.end()) {
        return std::make_tuple(basePath / (stem.string() + "_" + extension + ".cpp"),
                               ResourcePreprocessor::ResourceType::Mesh);
    }
    return std::nullopt;
}
} // namespace

void ResourcePreprocessor::work() {

    if (!std::filesystem::exists(outputDir)) {
        std::cout << "No Shader Reloading without " << outputDir << "\n";
        return;
    }

    while (!terminate) {
        std::this_thread::sleep_for(refreshTime);

        try {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(inputDir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto &path = entry.path();
                const auto key = getKey(path);

                if (!data.contains(key)) {
                    continue;
                }

                std::uint64_t src_mtime = get_mtime_ms(path.string());

                auto &someData = data.at(key);

                std::uint64_t dest_mtime = std::visit(
                    Visitor{[](const auto &ptr) -> const uint64_t & { return getTimestamp(*ptr); }},
                    someData);

                if (src_mtime <= dest_mtime) {
                    continue;
                }

                auto artefactTypeOpt = getArtefactAndType(path, *this);
                if (!artefactTypeOpt) {
                    throw std::logic_error(std::format("ResourcePreprocessor: {} is no file with a "
                                                       "known extension despite {} beeing tracked",
                                                       path.string(), key));
                }
                const auto &[_, type] = artefactTypeOpt.value();

                std::optional<resource_ptr> resource;
                switch (type) {
                case ResourceType::Shader:
                    resource = parseShader(path, key);
                    shadersNeedUpdating = true;
                    break;
                case ResourceType::Texture:
                    resource = parseTexture(path, key);
                    texturesNeedUpdating = true;
                    break;
                case ResourceType::Mesh:
                    resource = parseMesh(path, key);
                    meshesNeedUpdate = true;
                    break;
                }

                if (!resource) {
                    std::cout << "failed to parse: " << path << "\n";
                    std::visit(Visitor{[&src_mtime](const auto &ptr) -> void {
                                   setTimestamp(*ptr, src_mtime);
                               }},
                               someData);
                    continue;
                }

                Visitor v{[&src_mtime](const auto &ptr) -> void { setTimestamp(*ptr, src_mtime); }};
                std::visit(v, resource.value());

                data[key] = std::move(resource.value());
            }
        } catch (std::filesystem::filesystem_error &err) {
            std::cerr << err.what() << "\n";
        }
    }
}

void ResourcePreprocessor::startUpdater(std::chrono::milliseconds suppliedRefreshTime) {
    assert(!running);
    refreshTime = suppliedRefreshTime;

    const auto &preprocessorData = PreprocessorDataHolder::getData();

    data.insert_range(
        preprocessorData.shaders | std::views::transform([](const auto &tuple) {
            const auto &[key, data] = tuple;
            return std::make_tuple(
                std::string{key},
                std::make_unique<ShaderData>(
                    data->timestamp,
                    std::vector{std::from_range, std::span(data->data, data->data_len)},
                    data->data_len));
        }));
    data.insert_range(
        preprocessorData.textures | std::views::transform([](const auto &tuple) {
            const auto &[key, data] = tuple;
            return std::make_tuple(
                std::string{key},
                std::make_unique<TextureData>(
                    data->timestamp, data->width, data->height,
                    std::vector{std::from_range, std::span(data->pixels, data->pixels_len)}));
        }));
    data.insert_range(preprocessorData.meshes | std::views::transform([](const auto &tuple) {
                          const auto &[key, data] = tuple;
                          return std::make_tuple(
                              std::string{key},
                              std::make_unique<MeshData>(data->timestamp, data->attrib,
                                                         data->shapes, data->materials));
                      }));

    running = true;
    terminate = false;
    worker = std::thread([this]() -> void { this->work(); });
}

void ResourcePreprocessor::stopUpdater() {
    assert(running);

    terminate = true;
    worker.join();
    running = false;
}
auto ResourcePreprocessor::shadersNeedUpdatingClear() -> bool {
    return std::exchange(shadersNeedUpdating, false);
}
auto ResourcePreprocessor::texturesNeedUpdatingClear() -> bool {
    return std::exchange(texturesNeedUpdating, false);
}
auto ResourcePreprocessor::meshesNeedUpdateClear() -> bool {
    return std::exchange(meshesNeedUpdate, false);
}

ResourcePreprocessor::~ResourcePreprocessor() {
    if (running) {
        stopUpdater();
    }
}

namespace {
void writeData(const std::string &key, ResourcePreprocessor::resource_ptr &resource,
               const std::vector<std::filesystem::path> &artefacts) {

    std::vector<std::stringstream> streams;
    streams.resize(artefacts.size());

    size_t indent = 1;
    Serializer seralizer(streams.at(0), indent);

    Visitor v{[&](const std::unique_ptr<ShaderData> &shaderData) -> void {
                  streams.at(0) << "#include \"ShaderData.h\"\n"
                                << "#include <stdint.h>\n\n";

                  streams.at(0) << "const uint32_t " << key << "_data_spv[] = ";
                  seralizer.write(shaderData->data);
                  streams.at(0) << ";\n\n";

                  streams.at(0) << "const ShaderData_c " << key << "_data = ";
                  seralizer.write(*shaderData, key);
                  streams.at(0) << ";";
              },
              [&](const std::unique_ptr<TextureData> &textureData) -> void {
                  streams.at(0) << "#include \"TextureData.h\"\n";
                  streams.at(0) << "#include <stb_image.h>\n\n";

                  streams.at(0) << "const stbi_uc " << key << "_data_pixels[] = ";
                  seralizer.write(textureData->pixels);
                  streams.at(0) << ";\n\n";

                  streams.at(0) << "const TextureData_c " << key << "_data = ";
                  seralizer.write(*textureData, key);
                  streams.at(0) << ";";
              },
              [&]([[maybe_unused]] const std::unique_ptr<MeshData> &meshData) -> void {
                  streams.at(0) << "#include \"MeshData.hpp\"\n\nconst MeshData " << key
                                << "_data = ";
                  seralizer.write(*meshData);
                  streams.at(0) << ";";
              }};

    std::visit(v, resource);

    for (const auto &[artefact, stream] : std::ranges::zip_view(artefacts, streams)) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Writing " << artefact << " (";
        std::filesystem::create_directories(artefact.parent_path());
        std::ofstream ostream(artefact);
        ostream << stream.str();
        ostream.close();
        std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now() - start));
    }
}

struct Keys {
    std::vector<std::string> shaders;
    std::vector<std::string> textures;
    std::vector<std::string> meshes;
};

void processFiles(const std::filesystem::directory_entry &entry,
                  ResourcePreprocessor &resourcePreprocessor, Keys &keys) {
    if (!entry.is_regular_file())
        return;

    const auto &path = entry.path();
    std::vector<std::filesystem::path> artefacts;
    auto artefactTypeOpt = getArtefactAndType(path, resourcePreprocessor);
    if (!artefactTypeOpt) {
        return;
    }
    const auto &[artefact, type] = artefactTypeOpt.value();
    artefacts.push_back(artefact);

    std::uint64_t dest_mtime = 0;
    for (const auto &artefact : artefacts) {
        dest_mtime = std::max(dest_mtime, get_mtime_ms(artefact.string()));
    }

    std::uint64_t src_mtime = get_mtime_ms(path.string());

    if (src_mtime < dest_mtime) {
        return;
    }

    const auto key = resourcePreprocessor.getKey(path);

    std::optional<ResourcePreprocessor::resource_ptr> resource;
    switch (type) {
    case ResourcePreprocessor::ResourceType::Shader:
        keys.shaders.push_back(key);
        resource = parseShader(path, key);
        break;
    case ResourcePreprocessor::ResourceType::Texture:
        keys.textures.push_back(key);
        resource = parseTexture(path, key);
        break;
    case ResourcePreprocessor::ResourceType::Mesh:
        keys.meshes.push_back(key);
        resource = parseMesh(path, key);
        break;
    }

    if (!resource) {
        std::cout << "failed to parse: " << path << "\n";
        return;
    }

    Visitor v{[&src_mtime](const auto &ptr) -> void { setTimestamp(*ptr, src_mtime); }};
    std::visit(v, resource.value());

    writeData(key, resource.value(), artefacts);
};

void writeResources(Keys &keys, ResourcePreprocessor &resourcePreprocessor) {

    std::stringstream buff;

    const std::filesystem::path resourceCpp = resourcePreprocessor.getOutputDir() / "Resource.cpp";

    std::println(buff, "#include \"Resource.hpp\"");
    std::println(buff, "#include \"ShaderData.h\"");
    std::println(buff, "#include \"TextureData.h\"");
    std::println(buff, "#include \"MeshData.hpp\"");
    std::println(buff);
    std::println(buff, "#include <array>");
    std::println(buff, "#include <tuple>");
    std::println(buff, "#include <string_view>");
    std::println(buff, "#include <variant>");
    std::println(buff);
    std::println(buff, "extern \"C\" {{");
    for (const auto &key : keys.shaders) {
        std::println(buff, "\textern ShaderData_c {}_data;", key);
    }
    for (const auto &key : keys.textures) {
        std::println(buff, "\textern TextureData_c {}_data;", key);
    }
    for (const auto &key : keys.meshes) {
        std::println(buff, "\textern MeshData {}_data;", key);
    }
    std::println(buff, "}}");
    std::println(buff, "namespace {{");

    auto writeArray = [&](std::string_view name, const std::vector<std::string> &keys,
                          std::string_view dataType) {
        if (keys.empty()) {
            std::println(buff, "const std::array<std::tuple<std::string_view, {}*>, 0> {} = {{}};",
                         dataType, name);
            return;
        }
        std::println(buff, "const auto {} = std::to_array<std::tuple<std::string_view, {}*>>({{\n",
                     name, dataType);
        for (const auto &key : keys) {
            std::println(buff, "\t{{\"{}\", &{}_data}},", key, key);
        }

        std::println(buff, "}});");
    };

    writeArray("shaders", keys.shaders, "ShaderData_c");
    writeArray("textures", keys.textures, "TextureData_c");
    writeArray("meshes", keys.meshes, "MeshData");

    buff << "[[maybe_unused]] const auto init_data = []() noexcept { "
            "PreprocessorDataHolder::setData({.shaders=shaders, .textures=textures, "
            ".meshes=meshes}); return "
            "std::monostate{}; }();\n";

    std::println(buff, "}}");

    if (buff.str() != readFile(resourceCpp)) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Writing " << resourceCpp << " (";
        std::ofstream ostream(resourceCpp);
        ostream << buff.str();
        ostream.close();
        std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now() - start));
    }
}

} // namespace

void ResourcePreprocessor::preprocess() {
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    Keys keys;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(inputDir)) {
        processFiles(entry, *this, keys);
    }

    writeResources(keys, *this);
}
