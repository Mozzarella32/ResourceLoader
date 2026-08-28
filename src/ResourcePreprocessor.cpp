#include "ResourcePreprocessor.hpp"
#include "MeshData.hpp"
#include "Resource.hpp"
#include "ShaderData.h"
#include "TextureData.h"

#include <Visitor.hpp>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <tiny_obj_loader.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
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

    const auto extension = path.extension().string().substr(1);
    if (extension == "frag") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangFragment, key);
    } else if (extension == "vert") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangVertex, key);
    } else if (extension == "geom") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangGeometry, key);
    } else if (extension == "comp") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangCompute, key);
    } else if (extension == "tesc") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangTessControl, key);
    } else if (extension == "tese") {
        vecOpt = compileSingleShader(readFile(path), EShLanguage::EShLangTessEvaluation, key);
    } else if (extension == "spv") {
        std::cerr << "Currently not supported, would just need to read bytes";
        return std::nullopt;
    }
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
                const auto extension = path.extension().string().substr(1);
                auto basePath = outputDir / std::filesystem::relative(path, inputDir);
                basePath.remove_filename();

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

                std::optional<resource_ptr> resource;
                if (std::ranges::find(shaderExtensions, extension) != shaderExtensions.end()) {
                    resource = parseShader(path, key);
                    shadersNeedUpdating = true;
                } else if (std::ranges::find(textureExtensions, extension) !=
                           textureExtensions.end()) {
                    resource = parseTexture(path, key);
                    texturesNeedUpdating = true;
                } else if (std::ranges::find(meshExtensions, extension) != meshExtensions.end()) {
                    resource = parseMesh(path, key);
                    meshesNeedUpdate = true;
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

    auto &preprocessorData = PreprocessorDataHolder::getData();

    for (const auto &[key, value] : preprocessorData) {
        Visitor v{[](const ShaderData_c *shader_data) -> resource_ptr {
                      return std::make_unique<ShaderData>(
                          shader_data->timestamp,
                          std::vector{std::from_range,
                                      std::span(shader_data->data, shader_data->data_len)},
                          shader_data->data_len);
                  },
                  [](const TextureData_c *texture_data) -> resource_ptr {
                      return std::make_unique<TextureData>(
                          texture_data->timestamp, texture_data->width, texture_data->height,
                          std::vector{std::from_range,
                                      std::span(texture_data->pixels, texture_data->pixels_len)});
                  },
                  [](const MeshData *mesh_data) -> resource_ptr {
                      return std::make_unique<MeshData>(mesh_data->timestamp, mesh_data->attrib,
                                                        mesh_data->shapes, mesh_data->materials);
                  }};
        data[key] = std::visit(v, value);
    }

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

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define Decl(name)                                                                                 \
    ostream << (name) << " {";                                                                     \
    ostream << "\n" << std::string(indent, '\t');

#define Decl_c(name)                                                                               \
    ostream << " {";                                                                               \
    ostream << "\n" << std::string(indent, '\t');

#define Decl_noAggregat(name)                                                                      \
    ostream << " []() {";                                                                          \
    ostream << "\n" << std::string(indent, '\t');                                                  \
    ostream << (name) << " data;";                                                                 \
    ostream << "\n" << std::string(indent, '\t');

#define Member(mem)                                                                                \
    ostream << "." << #mem << " = ";                                                               \
    write(data.mem, ostream, indent + 1, key);                                                     \
    ostream << ",\n" << std::string(indent, '\t');

#define Member_noAggregat(mem)                                                                     \
    ostream << "data." << #mem << " = ";                                                           \
    write(data.mem, ostream, indent + 1, key);                                                     \
    ostream << ";\n" << std::string(indent, '\t');

#define Exp(mem, exp)                                                                              \
    ostream << "." << #mem << " = ";                                                               \
    write(exp, ostream, indent + 1, key);                                                          \
    ostream << ",\n" << std::string(indent, '\t');

#define Exp_noAggregat(mem, exp)                                                                   \
    ostream << "data." << #mem << " = ";                                                           \
    write(exp, ostream, indent + 1, key);                                                          \
    ostream << ";\n" << std::string(indent, '\t');

#define Member_last(mem)                                                                           \
    ostream << "." << #mem << " = ";                                                               \
    write(data.mem, ostream, indent + 1, key);                                                     \
    ostream << ",\n" << std::string(indent - 1, '\t');                                             \
    ostream << "}";

#define Member_last_noAggregat(mem)                                                                \
    ostream << "data." << #mem << " = ";                                                           \
    write(data.mem, ostream, indent + 1, key);                                                     \
    ostream << ";\n" << std::string(indent, '\t');                                                 \
    ostream << "return data";                                                                      \
    ostream << ";\n" << std::string(indent - 1, '\t');                                             \
    ostream << "}()";

#define Exp_last(mem, exp)                                                                         \
    ostream << "." << #mem << " = ";                                                               \
    write(exp, ostream, indent + 1, key);                                                          \
    ostream << ",\n" << std::string(indent - 1, '\t');                                             \
    ostream << "}";

#define Exp_last_noAggregat(mem, exp)                                                              \
    ostream << "data." << #mem << " = ";                                                           \
    write(exp, ostream, indent + 1, key);                                                          \
    ostream << ";\n" << std::string(indent, '\t');                                                 \
    ostream << "return data";                                                                      \
    ostream << ";\n" << std::string(indent - 1, '\t');                                             \
    ostream << "}()";

// NOLINTEND(cppcoreguidelines-macro-usage)

namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
void write(const std::string &str, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << "\"" << str << "\"";
}

class plain_string : public std::string {};

void write(const plain_string &str, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << str;
}

void write(const bool &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    if (value) {
        ostream << "true";
    } else {
        ostream << "false";
    }
}

void write(const float &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << std::setfill(' ') << std::setw(9) << value;
}

void write(const int &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    int num = value;
    int width = 9;
    if (value < 0) {
        ostream << '-';
        num = -value;
        width -= 1;
    }
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(width) << num;
}

void write(const std::uint64_t &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(8) << value << "u";
}

void write(const unsigned int &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(8) << value << "u";
}

void write(const stbi_uc &value, std::ostream &ostream, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(2) << static_cast<short>(value);
}

void write(const tinyobj::texture_type_t &texture, std::ostream &ostream,
           [[maybe_unused]] size_t indent, [[maybe_unused]] const std::string &key) {
    ostream << std::hex << std::setfill('0');
    ostream << "tinyobj::texture_type_t(0x" << std::setw(3) << static_cast<size_t>(texture) << ")";
}

template <typename T>
void write(const std::vector<T> &vec, std::ostream &ostream, size_t indent, const std::string &key);

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
template <typename T, size_t N>
void write(const T (&arr)[N], std::ostream &ostream, size_t indent, const std::string &key);
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

template <typename K, typename V>
void write(const std::map<K, V> &map, std::ostream &ostream, size_t indent, const std::string &key);

void write(const tinyobj::skin_weight_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::skin_weight_t");
    Member(vertex_id);
    Member_last(weightValues);
}

void write(const tinyobj::joint_and_weight_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::joint_and_weight_t");
    Member(joint_id);
    Member_last(weight);
}

void write(const ShaderData &data, std::ostream &ostream, size_t indent, const std::string &key) {
    Decl_c("ShaderData_c");
    Member(timestamp);
    Exp(data, plain_string(std::format("{}_data_spv", key)));
    Member_last(data_len);
}

void write(const TextureData &data, std::ostream &ostream, size_t indent, const std::string &key) {
    Decl_c("TextureData_c");
    Member(timestamp);
    Member(width);
    Member(height);
    Exp(pixels, plain_string(std::format("{}_data_pixels", key)));
    Member_last(pixels_len);
}

void write(const tinyobj::attrib_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl_noAggregat("tinyobj::attrib_t");
    Member_noAggregat(vertices);
    Member_noAggregat(vertex_weights);
    Member_noAggregat(normals);
    Member_noAggregat(texcoords);
    Member_noAggregat(texcoord_ws);
    Member_noAggregat(colors);
    Member_last_noAggregat(skin_weights);
}

void write(const tinyobj::mesh_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::mesh_t");
    Member(indices);
    Member(num_face_vertices);
    Member(material_ids);
    Member(smoothing_group_ids);
    Member_last(tags);
}

void write(const tinyobj::tag_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::tag_t");
    Member(name);
    Member(intValues);
    Member(floatValues);
    Member_last(stringValues);
}

void write(const tinyobj::lines_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::lines_t");
    Member(indices);
    Member_last(num_line_vertices);
}

void write(const tinyobj::points_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::points_t");
    Member_last(indices);
}

void write(const tinyobj::index_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::index_t");
    Member(vertex_index);
    Member(normal_index);
    Member_last(texcoord_index);
}

void write(const tinyobj::shape_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::shape_t");
    Member(name);
    Member(mesh);
    Member(lines);
    Member_last(points);
}

void write(const tinyobj::texture_option_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::texture_option_t");
    Member(type);
    Member(sharpness);
    Member(brightness);
    Member(contrast);
    Member(origin_offset);
    Member(scale);
    Member(turbulence);
    Member(texture_resolution);
    Member(clamp);
    Member(imfchan);
    Member(blendu);
    Member(blendv);
    Member(bump_multiplier);
    Member_last(colorspace);
}

void write(const tinyobj::material_t &data, std::ostream &ostream, size_t indent,
           const std::string &key) {
    Decl("tinyobj::material_t");
    Member(name);
    Member(ambient);
    Member(diffuse);
    Member(specular);
    Member(transmittance);
    Member(emission);
    Member(shininess);
    Member(ior);
    Member(dissolve);
    Member(illum);
    Member(dummy);
    Member(ambient_texname);
    Member(diffuse_texname);
    Member(specular_texname);
    Member(specular_highlight_texname);
    Member(bump_texname);
    Member(displacement_texname);
    Member(alpha_texname);
    Member(reflection_texname);
    Member(ambient_texopt);
    Member(diffuse_texopt);
    Member(specular_texopt);
    Member(specular_highlight_texopt);
    Member(bump_texopt);
    Member(displacement_texopt);
    Member(alpha_texopt);
    Member(reflection_texopt);
    Member(roughness);
    Member(metallic);
    Member(sheen);
    Member(clearcoat_thickness);
    Member(clearcoat_roughness);
    Member(anisotropy);
    Member(anisotropy_rotation);
    Member(pad0);
    Member(roughness_texname);
    Member(metallic_texname);
    Member(sheen_texname);
    Member(emissive_texname);
    Member(normal_texname);
    Member(roughness_texopt);
    Member(metallic_texopt);
    Member(emissive_texopt);
    Member(normal_texopt);
    Member(pad2);
    Member_last(unknown_parameter);
}

void write(const MeshData &data, std::ostream &ostream, size_t indent, const std::string &key) {
    Decl("MeshData");
    Member(timestamp);
    Member(attrib);
    Member(shapes);
    Member_last(materials);
}

template <typename T>
void write(const std::vector<T> &vec, std::ostream &ostream, size_t indent,
           const std::string &key) {
    int perline = 1;
    if constexpr (std::same_as<T, unsigned int> || std::same_as<T, int> || std::same_as<T, float>) {
        perline = 8;
    } else if constexpr (std::same_as<T, stbi_uc>) {
        perline = 16;
    } else if constexpr (std::same_as<T, std::string> || std::same_as<T, tinyobj::skin_weight_t> ||
                         std::same_as<T, tinyobj::joint_and_weight_t> ||
                         std::same_as<T, tinyobj::index_t> || std::same_as<T, tinyobj::tag_t> ||
                         std::same_as<T, tinyobj::shape_t> ||
                         std::same_as<T, tinyobj::material_t>) {
        perline = 1;
    } else {
        static_assert(false);
    }

    int newline = 0;

    if (vec.empty()) {
        ostream << "{}";
        return;
    }

    ostream << "{\n";
    for (const auto &element : vec) {
        if (newline == 0) {
            ostream << std::string(indent, '\t');
        }
        write(element, ostream, indent + 1, key);
        ostream << ", ";
        newline = (newline + 1) % perline;
        if (newline == 0) {
            ostream << "\n";
        }
    }
    ostream << "\n";
    ostream << std::string(indent - 1, '\t') << "}";
}

template <typename K, typename V>
void write(const std::map<K, V> &map, std::ostream &ostream, size_t indent,
           const std::string &key) {
    if (map.empty()) {
        ostream << "{}";
        return;
    }
    ostream << "{\n";
    for (const auto &[k, v] : map) {
        ostream << std::string(indent, '\t');
        ostream << "{";
        write(k, ostream, indent + 1, key);
        ostream << ", ";
        write(v, ostream, indent + 1, key);
        ostream << ", ";
        ostream << "\n";
    }
    ostream << "\n";
    ostream << std::string(indent - 1, '\t') << "}";
}

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
template <typename T, size_t N>
void write(const T (&arr)[N], std::ostream &ostream, size_t indent, const std::string &key) {
    int perline = 1;
    if constexpr (std::same_as<T, unsigned int> || std::same_as<T, int> || std::same_as<T, int> ||
                  std::same_as<T, float>) {
        perline = 8;
    } else if constexpr (std::same_as<T, stbi_uc>) {
        perline = 16;
    } else if constexpr (std::same_as<T, std::string> || std::same_as<T, tinyobj::skin_weight_t> ||
                         std::same_as<T, tinyobj::joint_and_weight_t> ||
                         std::same_as<T, tinyobj::index_t> || std::same_as<T, tinyobj::tag_t> ||
                         std::same_as<T, tinyobj::shape_t> ||
                         std::same_as<T, tinyobj::material_t>) {
        perline = 1;
    } else {
        static_assert(false);
    }

    if constexpr (N == 0) {
        ostream << "{}";
        return;
    }

    int newline = 0;

    ostream << "{\n";
    for (size_t i = 0; i < N; ++i) {
        if (newline == 0) {
            ostream << std::string(indent, '\t');
        }
        write(arr[i], ostream, indent + 1, key);
        if (i != N - 1)
            ostream << ", ";
        newline = (newline + 1) % perline;
        if (newline == 0 && i != N - 1) {
            ostream << "\n";
        }
    }
    ostream << "\n";
    ostream << std::string(indent - 1, '\t') << "}";
}
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

void writeData(const std::string &key, ResourcePreprocessor::resource_ptr &resource,
               const std::vector<std::filesystem::path> &artefacts) {

    std::vector<std::stringstream> streams;
    streams.resize(artefacts.size());

    Visitor v{[&](const std::unique_ptr<ShaderData> &shaderData) -> void {
                  streams.at(0) << "#include \"ShaderData.h\"\n"
                                << "#include <stdint.h>\n\n";

                  streams.at(0) << "const uint32_t " << key << "_data_spv[] = ";
                  write(shaderData->data, streams.at(0), 1, key);
                  streams.at(0) << ";\n\n";

                  streams.at(0) << "ShaderData_c " << key << "_data = ";
                  write(*shaderData, streams.at(0), 1, key);
                  streams.at(0) << ";";
              },
              [&](const std::unique_ptr<TextureData> &textureData) -> void {
                  streams.at(0) << "#include \"TextureData.h\"\n";
                  streams.at(0) << "#include <stb_image.h>\n\n";

                  streams.at(0) << "const stbi_uc " << key << "_data_pixels[] = ";
                  write(textureData->pixels, streams.at(0), 1, key);
                  streams.at(0) << ";\n\n";

                  streams.at(0) << "TextureData_c " << key << "_data = ";
                  write(*textureData, streams.at(0), 1, key);
                  streams.at(0) << ";";
              },
              [&]([[maybe_unused]] const std::unique_ptr<MeshData> &meshData) -> void {
                  streams.at(0) << "#include \"MeshData.hpp\"\n\nMeshData " << key << "_data = ";
                  write(*meshData, streams.at(0), 1, key);
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
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

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
    const auto key = resourcePreprocessor.getKey(path);
    const auto extension = path.extension().string().substr(1);
    const auto basePath = (resourcePreprocessor.getOutputDir() /
                           std::filesystem::relative(path, resourcePreprocessor.getInputDir()))
                              .remove_filename();
    const auto stem = path.stem();

    if (std::ranges::find(ResourcePreprocessor::shaderExtensions, extension) !=
        ResourcePreprocessor::shaderExtensions.end()) {
        artefacts.push_back(basePath / (stem.string() + "_" + extension + ".c"));
        keys.shaders.push_back(key);
    } else if (std::ranges::find(ResourcePreprocessor::textureExtensions, extension) !=
               ResourcePreprocessor::textureExtensions.end()) {
        artefacts.push_back(basePath / (stem.string() + "_" + extension + ".c"));
        keys.textures.push_back(key);
    } else if (std::ranges::find(ResourcePreprocessor::meshExtensions, extension) !=
               ResourcePreprocessor::meshExtensions.end()) {
        artefacts.push_back(basePath / (stem.string() + "_" + extension + ".cpp"));
        keys.meshes.push_back(key);
    } else {
        return;
    }

    std::uint64_t dest_mtime = 0;
    for (const auto &artefact : artefacts) {
        dest_mtime = std::max(dest_mtime, get_mtime_ms(artefact.string()));
    }

    std::uint64_t src_mtime = get_mtime_ms(path.string());

    if (src_mtime < dest_mtime) {
        return;
    }

    std::optional<ResourcePreprocessor::resource_ptr> resource;
    if (std::ranges::find(ResourcePreprocessor::shaderExtensions, extension) !=
        ResourcePreprocessor::shaderExtensions.end()) {
        resource = parseShader(path, key);
    } else if (std::ranges::find(ResourcePreprocessor::textureExtensions, extension) !=
               ResourcePreprocessor::textureExtensions.end()) {
        resource = parseTexture(path, key);
    } else if (std::ranges::find(ResourcePreprocessor::meshExtensions, extension) !=
               ResourcePreprocessor::meshExtensions.end()) {
        resource = parseMesh(path, key);
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

    std::stringstream str;

    const std::filesystem::path resourceCpp = resourcePreprocessor.getOutputDir() / "Resource.cpp";

    str << "#include \"Resource.hpp\"\n\n";
    str << "extern \"C\" {\n";
    for (const auto &key : keys.shaders) {
        str << "\textern ShaderData_c " << key << "_data;\n";
    }
    for (const auto &key : keys.textures) {
        str << "\textern TextureData_c " << key << "_data;\n";
    }
    for (const auto &key : keys.meshes) {
        str << "\textern MeshData " << key << "_data;\n";
    }
    str << "}\n";

    str << "std::unordered_map<std::string, std::variant<ShaderData_c *, "
           "TextureData_c *, "
           "MeshData *>> preprocessor_data = {\n";
    for (const auto &key : keys.shaders) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    for (const auto &key : keys.textures) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    for (const auto &key : keys.meshes) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    str << "};\n\n";

    str << "auto init_data = [](){ "
           "PreprocessorDataHolder::setData(preprocessor_data); return "
           "1; }();\n";

    if (str.str() != readFile(resourceCpp)) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Writing " << resourceCpp << " (";
        std::ofstream ostream(resourceCpp);
        ostream << str.str();
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
