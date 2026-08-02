#include "ResourcePreprocessor.hpp"
#include "Resource.hpp"
#include "Visitor.hpp"

#include <cstdint>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <tuple>
#include <vector>
#ifdef _WIN32
#include <io.h>
#define stat _stat
#endif

uint64_t get_mtime_ms(const std::string &filename) {
    struct stat result;
    if (stat(filename.c_str(), &result) != 0) {
        return 0;
    }

#ifdef __APPLE__
    uint64_t sec = result.st_mtimespec.tv_sec;
    uint64_t nsec = result.st_mtimespec.tv_nsec;
#elif defined(_POSIX_VERSION)
#if defined(st_mtim)
    uint64_t sec = result.st_mtim.tv_sec;
    uint64_t nsec = result.st_mtim.tv_nsec;
#else
    uint64_t sec = result.st_mtime;
    uint64_t nsec = 0;
#endif
#else
    uint64_t sec = result.st_mtime;
    uint64_t nsec = 0;
#endif

    uint64_t ms = sec * 1000ULL + nsec / 1000000ULL;
    return ms;
}

void ResourcePreprocessor::info() const {
    std::cout << "Input dir: " << (inputDir.empty() ? "<none>" : inputDir) << "\n";
    std::cout << "Output dir: " << (outputDir.empty() ? "<none>" : outputDir) << "\n";
}

const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationIageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */
    {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }};

std::optional<std::vector<unsigned int>>
compileSingleShader(const std::string &shaderSource, EShLanguage type, const std::string &name) {
    std::cout << "Compiling " << name << " (";
    auto start = std::chrono::steady_clock::now();
    glslang::TProgram program;
    glslang::TShader shader(type);
    const char *c_str = shaderSource.c_str();
    shader.setStrings(&c_str, 1);
    shader.setOverrideVersion(0);

    shader.setEnvInput(glslang::EShSourceGlsl, type, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    EShMessages messages =
        static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgEnhanced);

    if (!shader.parse(&DefaultTBuiltInResource, 100, false, messages)) {
        std::cerr << "GLSL parsing failed:\n" << shader.getInfoLog() << std::endl;
        return std::nullopt;
    }

    program.addShader(&shader);

    if (!program.link(messages)) {
        std::cerr << "GLSL linking failed:\n" << program.getInfoLog() << std::endl;
        return std::nullopt;
    }

    if (!program.getIntermediate(type)) {
        std::cerr << "Failed to get shader intermediate after parsing." << std::endl;
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

std::string readFile(const std::filesystem::path &path) {
    std::ifstream file(path);

    if (!file) {
        return "";
    }

    std::string contents;
    if (file) {
        std::ostringstream ss;
        ss << file.rdbuf();
        contents = ss.str();
    }
    return contents;
}

std::span<const uint32_t> ResourcePreprocessor::getShaderSpirV(const std::string &key) {
    static std::vector<uint32_t> empty;

    std::unique_lock ul{resouceMutex};
    auto it = data.find(key);
    if (it == data.end()) {
        throw std::runtime_error{"failed to find shader: " + key};
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<ShaderData>& shaderData) -> std::span<uint32_t> {
            return shaderData->data;
        },
        [&key]([[maybe_unused]]const auto& other) -> std::span<uint32_t> {
            std::cerr << "getShaderSpirV with non shader resouce: " << key << "\n";
            return empty;
        }
    };
    // clang-format on

    return std::visit(v, it->second);
}

std::function<std::span<const uint32_t>(const std::string &)> ResourcePreprocessor::spirVGetter() {
    return [&](const std::string &key) -> std::span<const uint32_t> { return getShaderSpirV(key); };
}

const TextureData &ResourcePreprocessor::getTextureData(const std::string &key) {
    static TextureData empty;

    std::unique_lock ul{resouceMutex};
    auto it = data.find(key);
    if (it == data.end()) {
        std::cerr << "failed to find textureData: " << key << "\n";
        return empty;
    }

    // clang-format off
    Visitor v{
        [](const std::unique_ptr<TextureData>& textureData) -> const auto& {
            return *textureData.get();
        },
        [&key]([[maybe_unused]]const auto& other) -> const auto& {
            std::cerr << "getShaderSpirV with non shader resouce: " << key << "\n";
            return empty;
       }
    };
    // clang-format on

    return std::visit(v, it->second);
}

std::function<
    std::tuple<std::pair<uint32_t, uint32_t>, std::span<const unsigned char>>(const std::string &)>
ResourcePreprocessor::textureGetter() {
    return [&](const std::string &key)
               -> std::tuple<std::pair<uint32_t, uint32_t>, std::span<const unsigned char>> {
        const TextureData &data = getTextureData(key);
        std::span<const stbi_uc> span(data.pixels.data(), data.pixels.size());
        return {{data.width, data.height}, span};
    };
}
std::string ResourcePreprocessor::getKey(const std::filesystem::path &path) {
    const std::filesystem::path relative = std::filesystem::relative(path, inputDir);
    std::vector<std::string> dirs;
    for (const auto &d : relative) {
        dirs.push_back(d.string());
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

std::optional<ResourcePreprocessor::resource_ptr> parseShader(const std::filesystem::path &path,
                                                              const std::string &key) {

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
    std::unique_ptr<stbi_uc, decltype([](auto *pixels) { stbi_image_free(pixels); })>;

std::optional<ResourcePreprocessor::resource_ptr> parseTexture(const std::filesystem::path &path,
                                                               const std::string &name) {
    std::unique_ptr<TextureData> resouce = std::make_unique<TextureData>();

    int width, height, channels;
    uniqueStbPixels pixels;

    std::cout << "Decoding " << name << " (";
    auto start = std::chrono::steady_clock::now();

    pixels.reset(stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha));
    std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start));

    if (pixels.get() == nullptr) {
        return std::nullopt;
    }

    resouce->width = width;
    resouce->height = height;
    resouce->pixels = std::vector(pixels.get(), pixels.get() + width * height * 4);
    resouce->pixels_len = resouce->pixels.size();
    return resouce;
}

std::optional<ResourcePreprocessor::resource_ptr> parseMesh(const std::filesystem::path &path,
                                                            const std::string &name) {
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

void ResourcePreprocessor::work() {

    if (!std::filesystem::exists(outputDir)) {
        std::cout << "No Shader Reloading without " << outputDir << "\n";
        return;
    }

    while (!terminate) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

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
                const auto stem = path.stem();

                if (!data.contains(key)) {
                    continue;
                }

                std::uint64_t src_mtime = get_mtime_ms(path.string());

                auto &someData = data.at(key);

                std::uint64_t dest_mtime = std::visit(
                    Visitor{[](const auto &ptr) { return getTimestamp(*ptr); }}, someData);

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
                    std::visit(
                        Visitor{[&src_mtime](const auto &ptr) { setTimestamp(*ptr, src_mtime); }},
                        someData);
                    continue;
                }

                Visitor v{[&src_mtime](const auto &ptr) { setTimestamp(*ptr, src_mtime); }};
                std::visit(v, resource.value());

                data[key] = std::move(resource.value());
            }
        } catch (std::filesystem::filesystem_error &err) {
            std::cerr << err.what() << "\n";
        }
    }
}

void ResourcePreprocessor::startUpdater() {
    assert(!running);

    auto &preprocessorData = PreprocessorDataHolder::getData();

    for (const auto &it : preprocessorData) {
        Visitor v{[](const ShaderData_c *shader_data) -> resource_ptr {
                      return std::make_unique<ShaderData>(
                          shader_data->timestamp,
                          std::vector<uint32_t>{shader_data->data,
                                                shader_data->data + shader_data->data_len},
                          shader_data->data_len);
                  },
                  [](const TextureData_c *texture_data) -> resource_ptr {
                      return std::make_unique<TextureData>(
                          texture_data->timestamp, texture_data->width, texture_data->height,
                          std::vector<stbi_uc>{texture_data->pixels,
                                               texture_data->pixels + texture_data->pixels_len});
                  },
                  [](const MeshData *mesh_data) -> resource_ptr {
                      return std::make_unique<MeshData>(
                          mesh_data->timestamp, std::move(mesh_data->attrib),
                          std::move(mesh_data->shapes), std::move(mesh_data->materials));
                  }};
        data[it.first] = std::visit(v, it.second);
    }

    running = true;
    terminate = false;
    worker = std::thread([this]() { this->work(); });
}

void ResourcePreprocessor::stopUpdater() {
    assert(running);

    terminate = true;
    worker.join();
    running = false;
}

ResourcePreprocessor::~ResourcePreprocessor() {
    if (running) {
        stopUpdater();
    }
}

#define Decl(name)                                                                                 \
    of << name << " {";                                                                            \
    of << "\n" << std::string(indent, '\t');

#define Decl_c(name)                                                                               \
    of << " {";                                                                                    \
    of << "\n" << std::string(indent, '\t');

#define Decl_noAggregat(name)                                                                      \
    of << " []() {";                                                                               \
    of << "\n" << std::string(indent, '\t');                                                       \
    of << name << " data;";                                                                        \
    of << "\n" << std::string(indent, '\t');

#define Member(mem)                                                                                \
    of << "." << #mem << " = ";                                                                    \
    write(data.mem, of, indent + 1, key);                                                          \
    of << ",\n" << std::string(indent, '\t');

#define Member_noAggregat(mem)                                                                     \
    of << "data." << #mem << " = ";                                                                \
    write(data.mem, of, indent + 1, key);                                                          \
    of << ";\n" << std::string(indent, '\t');

#define Exp(mem, exp)                                                                              \
    of << "." << #mem << " = ";                                                                    \
    write(exp, of, indent + 1, key);                                                               \
    of << ",\n" << std::string(indent, '\t');

#define Exp_noAggregat(mem, exp)                                                                   \
    of << "data." << #mem << " = ";                                                                \
    write(exp, of, indent + 1, key);                                                               \
    of << ";\n" << std::string(indent, '\t');

#define Member_last(mem)                                                                           \
    of << "." << #mem << " = ";                                                                    \
    write(data.mem, of, indent + 1, key);                                                          \
    of << ",\n" << std::string(indent - 1, '\t');                                                  \
    of << "}";

#define Member_last_noAggregat(mem)                                                                \
    of << "data." << #mem << " = ";                                                                \
    write(data.mem, of, indent + 1, key);                                                          \
    of << ";\n" << std::string(indent, '\t');                                                      \
    of << "return data";                                                                           \
    of << ";\n" << std::string(indent - 1, '\t');                                                  \
    of << "}()";

#define Exp_last(mem, exp)                                                                         \
    of << "." << #mem << " = ";                                                                    \
    write(exp, of, indent + 1, key);                                                               \
    of << ",\n" << std::string(indent - 1, '\t');                                                  \
    of << "}";

#define Exp_last_noAggregat(mem, exp)                                                              \
    of << "data." << #mem << " = ";                                                                \
    write(exp, of, indent + 1, key);                                                               \
    of << ";\n" << std::string(indent, '\t');                                                      \
    of << "return data";                                                                           \
    of << ";\n" << std::string(indent - 1, '\t');                                                  \
    of << "}()";

void write(const std::string &str, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << "\"" << str << "\"";
}

class plain_string : public std::string {};

void write(const plain_string &str, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << str;
}

void write(const bool &b, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    if (b) {
        of << "true";
    } else {
        of << "false";
    }
}

void write(const float &f, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << std::setfill(' ') << std::setw(9) << f;
}

void write(const int &i, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    int num = i;
    int width = 9;
    if (i < 0) {
        of << '-';
        num = -i;
        width -= 1;
    }
    of << std::hex << std::setfill('0');
    of << "0x" << std::setw(width) << num;
}

void write(const std::uint64_t &ui, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << std::hex << std::setfill('0');
    of << "0x" << std::setw(8) << ui << "u";
}

void write(const unsigned int &ui, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << std::hex << std::setfill('0');
    of << "0x" << std::setw(8) << ui << "u";
}

void write(const stbi_uc &uc, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << std::hex << std::setfill('0');
    of << "0x" << std::setw(2) << (short)uc;
}

void write(const tinyobj::texture_type_t &e, std::ostream &of, [[maybe_unused]] size_t indent,
           [[maybe_unused]] const std::string &key) {
    of << std::hex << std::setfill('0');
    of << "tinyobj::texture_type_t(0x" << std::setw(3) << (size_t)e << ")";
}

template <typename T>
void write(const std::vector<T> &vec, std::ostream &of, size_t indent, const std::string &key);

template <typename T, size_t N>
void write(const T (&arr)[N], std::ostream &of, size_t indent, const std::string &key);

template <typename K, typename V>
void write(const std::map<K, V> &map, std::ostream &of, size_t indent, const std::string &key);

void write(const tinyobj::skin_weight_t &data, std::ostream &of, size_t indent,
           const std::string &key) {
    Decl("tinyobj::skin_weight_t");
    Member(vertex_id);
    Member_last(weightValues);
}

void write(const tinyobj::joint_and_weight_t &data, std::ostream &of, size_t indent,
           const std::string &key) {
    Decl("tinyobj::joint_and_weight_t");
    Member(joint_id);
    Member_last(weight);
}

void write(const ShaderData &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl_c("ShaderData_c");
    Member(timestamp);
    Exp(data, plain_string(std::format("{}_data_spv", key)));
    Member_last(data_len);
}

void write(const TextureData &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl_c("TextureData_c");
    Member(timestamp);
    Member(width);
    Member(height);
    Exp(pixels, plain_string(std::format("{}_data_pixels", key)));
    Member_last(pixels_len);
}

void write(const tinyobj::attrib_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl_noAggregat("tinyobj::attrib_t");
    Member_noAggregat(vertices);
    Member_noAggregat(vertex_weights);
    Member_noAggregat(normals);
    Member_noAggregat(texcoords);
    Member_noAggregat(texcoord_ws);
    Member_noAggregat(colors);
    Member_last_noAggregat(skin_weights);
}

void write(const tinyobj::mesh_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::mesh_t");
    Member(indices);
    Member(num_face_vertices);
    Member(material_ids);
    Member(smoothing_group_ids);
    Member_last(tags);
}

void write(const tinyobj::tag_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::tag_t");
    Member(name);
    Member(intValues);
    Member(floatValues);
    Member_last(stringValues);
}

void write(const tinyobj::lines_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::lines_t");
    Member(indices);
    Member_last(num_line_vertices);
}

void write(const tinyobj::points_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::points_t");
    Member_last(indices);
}

void write(const tinyobj::index_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::index_t");
    Member(vertex_index);
    Member(normal_index);
    Member_last(texcoord_index);
}

void write(const tinyobj::shape_t &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("tinyobj::shape_t");
    Member(name);
    Member(mesh);
    Member(lines);
    Member_last(points);
}

void write(const tinyobj::texture_option_t &data, std::ostream &of, size_t indent,
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

void write(const tinyobj::material_t &data, std::ostream &of, size_t indent,
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

void write(const MeshData &data, std::ostream &of, size_t indent, const std::string &key) {
    Decl("MeshData");
    Member(timestamp);
    Member(attrib);
    Member(shapes);
    Member_last(materials);
}

template <typename T>
void write(const std::vector<T> &vec, std::ostream &of, size_t indent, const std::string &key) {
    int perline;
    if constexpr (std::same_as<T, unsigned int>) {
        perline = 8;
    } else if constexpr (std::same_as<T, int>) {
        perline = 8;
    } else if constexpr (std::same_as<T, stbi_uc>) {
        perline = 16;
    } else if constexpr (std::same_as<T, float>) {
        perline = 8;
    } else if constexpr (std::same_as<T, std::string>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::skin_weight_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::joint_and_weight_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::index_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::tag_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::shape_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::material_t>) {
        perline = 1;
    } else {
        static_assert(false);
    }

    int newline = 0;

    if (vec.empty()) {
        of << "{}";
        return;
    }

    of << "{\n";
    for (size_t i = 0; i < vec.size(); i++) {
        if (newline == 0) {
            of << std::string(indent, '\t');
        }
        write(vec[i], of, indent + 1, key);
        of << ", ";
        newline = (newline + 1) % perline;
        if (newline == 0) {
            of << "\n";
        }
    }
    of << "\n";
    of << std::string(indent - 1, '\t') << "}";
}

template <typename K, typename V>
void write(const std::map<K, V> &map, std::ostream &of, size_t indent, const std::string &key) {
    if (map.empty()) {
        of << "{}";
        return;
    }
    of << "{\n";
    for (const auto &[k, v] : map) {
        of << std::string(indent, '\t');
        of << "{";
        write(k, of, indent + 1, key);
        of << ", ";
        write(v, of, indent + 1, key);
        of << ", ";
        of << "\n";
    }
    of << "\n";
    of << std::string(indent - 1, '\t') << "}";
}

template <typename T, size_t N>
void write(const T (&arr)[N], std::ostream &of, size_t indent, const std::string &key) {
    int perline;
    if constexpr (std::same_as<T, unsigned int>) {
        perline = 8;
    } else if constexpr (std::same_as<T, int>) {
        perline = 8;
    } else if constexpr (std::same_as<T, stbi_uc>) {
        perline = 16;
    } else if constexpr (std::same_as<T, float>) {
        perline = 8;
    } else if constexpr (std::same_as<T, std::string>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::skin_weight_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::joint_and_weight_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::index_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::tag_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::shape_t>) {
        perline = 1;
    } else if constexpr (std::same_as<T, tinyobj::material_t>) {
        perline = 1;
    } else {
        static_assert(false);
    }

    if constexpr (N == 0) {
        of << "{}";
        return;
    }

    int newline = 0;

    of << "{\n";
    for (size_t i = 0; i < N; ++i) {
        if (newline == 0) {
            of << std::string(indent, '\t');
        }
        write(arr[i], of, indent + 1, key);
        if (i != N - 1)
            of << ", ";
        newline = (newline + 1) % perline;
        if (newline == 0 && i != N - 1) {
            of << "\n";
        }
    }
    of << "\n";
    of << std::string(indent - 1, '\t') << "}";
}

void writeData(const std::string &key, ResourcePreprocessor::resource_ptr &resource,
               const std::vector<std::filesystem::path> artefacts) {

    std::vector<std::stringstream> s;
    s.resize(artefacts.size());

    Visitor v{[&](const std::unique_ptr<ShaderData> &shaderData) {
                  s[0] << "#include \"ShaderData.h\"\n"
                       << "#include <stdint.h>\n\n";

                  s[0] << "const uint32_t " << key << "_data_spv[] = ";
                  write(shaderData->data, s[0], 1, key);
                  s[0] << ";\n\n";

                  s[0] << "ShaderData_c " << key << "_data = ";
                  write(*shaderData, s[0], 1, key);
                  s[0] << ";";
              },
              [&](const std::unique_ptr<TextureData> &textureData) {
                  s[0] << "#include \"TextureData.h\"\n";
                  s[0] << "#include <stb_image.h>\n\n";

                  s[0] << "const stbi_uc " << key << "_data_pixels[] = ";
                  write(textureData->pixels, s[0], 1, key);
                  s[0] << ";\n\n";

                  s[0] << "TextureData_c " << key << "_data = ";
                  write(*textureData, s[0], 1, key);
                  s[0] << ";";
              },
              [&]([[maybe_unused]] const std::unique_ptr<MeshData> &meshData) {
                  s[0] << "#include \"MeshData.hpp\"\n\nMeshData " << key << "_data = ";
                  write(*meshData, s[0], 1, key);
                  s[0] << ";";
              }};

    std::visit(v, resource);

    for (size_t i = 0; i < artefacts.size(); i++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Writing " << artefacts[i] << " (";
        std::filesystem::create_directories(artefacts[i].parent_path());
        std::ofstream of(artefacts[i]);
        of << s[i].str();
        of.close();
        std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now() - start));
    }
}

void ResourcePreprocessor::preprocess() {
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }

    std::vector<std::string> shader_keys;
    std::vector<std::string> texture_keys;
    std::vector<std::string> mesh_keys;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file())
            continue;

        const auto &path = entry.path();
        const auto key = getKey(path);
        const auto extension = path.extension().string().substr(1);
        auto basePath = outputDir / std::filesystem::relative(path, inputDir);
        basePath.remove_filename();
        const auto stem = path.stem();

        std::vector<std::filesystem::path> artefacts;

        if (std::ranges::find(shaderExtensions, extension) != shaderExtensions.end()) {
            artefacts.push_back(basePath / (stem.string() + "_" + extension + ".c"));
            shader_keys.push_back(key);
        } else if (std::ranges::find(textureExtensions, extension) != textureExtensions.end()) {
            artefacts.push_back(basePath / (stem.string() + "_" + extension + ".c"));
            texture_keys.push_back(key);
        } else if (std::ranges::find(meshExtensions, extension) != meshExtensions.end()) {
            artefacts.push_back(basePath / (stem.string() + "_" + extension + ".cpp"));
            mesh_keys.push_back(key);
        } else
            continue;

        std::uint64_t dest_mtime = 0;
        for (const auto &artefact : artefacts) {
            dest_mtime = std::max(dest_mtime, get_mtime_ms(artefact.string()));
        }

        std::uint64_t src_mtime = get_mtime_ms(path.string());

        if (src_mtime < dest_mtime) {
            continue;
        }

        std::optional<resource_ptr> resource;
        if (std::ranges::find(shaderExtensions, extension) != shaderExtensions.end()) {
            resource = parseShader(path, key);
        } else if (std::ranges::find(textureExtensions, extension) != textureExtensions.end()) {
            resource = parseTexture(path, key);
        } else if (std::ranges::find(meshExtensions, extension) != meshExtensions.end()) {
            resource = parseMesh(path, key);
        }

        if (!resource) {
            std::cout << "failed to parse: " << path << "\n";
            return;
        }

        Visitor v{[&src_mtime](const auto &ptr) { setTimestamp(*ptr, src_mtime); }};
        std::visit(v, resource.value());

        writeData(key, resource.value(), artefacts);
    }

    std::stringstream str;

    std::filesystem::path resourceCpp = outputDir / "Resource.cpp";

    str << "#include \"Resource.hpp\"\n\n";
    str << "extern \"C\" {\n";
    for (const auto &key : shader_keys) {
        str << "\textern ShaderData_c " << key << "_data;\n";
    }
    for (const auto &key : texture_keys) {
        str << "\textern TextureData_c " << key << "_data;\n";
    }
    for (const auto &key : mesh_keys) {
        str << "\textern MeshData " << key << "_data;\n";
    }
    str << "}\n";

    str << "std::unordered_map<std::string, std::variant<ShaderData_c *, "
           "TextureData_c *, "
           "MeshData *>> preprocessor_data = {\n";
    for (const auto &key : shader_keys) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    for (const auto &key : texture_keys) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    for (const auto &key : mesh_keys) {
        str << "\t{\"" << key << "\", &" << key << "_data},\n";
    }
    str << "};\n\n";

    str << "auto init_data = [](){ "
           "PreprocessorDataHolder::setData(preprocessor_data); return "
           "1; }();\n";

    if (str.str() != readFile(resourceCpp)) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Writing " << resourceCpp << " (";
        std::ofstream of(resourceCpp);
        of << str.str();
        of.close();
        std::println("{})", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now() - start));
    }
}
