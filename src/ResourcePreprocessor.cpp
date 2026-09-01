#include "ResourcePreprocessor.hpp"
#include "ResourcePaths.hpp"
#include "ResourcePtr.hpp"
#include "ResourceType.hpp"
#include "Utils.hpp"

#include "MeshData.hpp"
#include "Parsers.hpp"
#include "Resource.hpp"
#include "Serialiser.hpp"
#include "ShaderData.hpp"
#include "TextureData.hpp"

#include <Visitor.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace {
void writeData(const std::string &key, const std::chrono::steady_clock::duration &parseTime,
               const ResourcePtr &resource, const std::filesystem::path &outputPath,
               const std::filesystem::path &baseOutputPath) {

    std::stringstream buffer;

    size_t indent = 1;
    Serializer seralizer(buffer, indent);

    Visitor v{[&](const std::unique_ptr<ShaderData> &shaderData) -> void {
                  buffer << "#include \"ShaderData.h\"\n"
                         << "#include <stdint.h>\n\n";

                  buffer << "const uint32_t " << key << "_data_spv[] = ";
                  seralizer.write(shaderData->data);
                  buffer << ";\n\n";

                  buffer << "const ShaderData_c " << key << "_data = ";
                  seralizer.write(*shaderData, key);
                  buffer << ";";
              },
              [&](const std::unique_ptr<TextureData> &textureData) -> void {
                  buffer << "#include \"TextureData.h\"\n";
                  buffer << "#include <stb_image.h>\n\n";

                  buffer << "const stbi_uc " << key << "_data_pixels[] = ";
                  seralizer.write(textureData->pixels);
                  buffer << ";\n\n";

                  buffer << "const TextureData_c " << key << "_data = ";
                  seralizer.write(*textureData, key);
                  buffer << ";";
              },
              [&]([[maybe_unused]] const std::unique_ptr<MeshData> &meshData) -> void {
                  buffer << "#include \"MeshData.hpp\"\n\nconst MeshData " << key << "_data = ";
                  seralizer.write(*meshData);
                  buffer << ";";
              }};

    std::visit(v, resource);

    auto start = std::chrono::steady_clock::now();
    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream ostream(outputPath);
    ostream << buffer.str();
    ostream.close();
    const auto writeTime = std::chrono::steady_clock::now() - start;
    std::cout << std::filesystem::relative(outputPath, baseOutputPath).string() << ": (Parsing: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(parseTime).count()
              << "ms, Writing: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(writeTime).count()
              << "ms)\n";
}

struct Keys {
    std::vector<std::string> shaders;
    std::vector<std::string> textures;
    std::vector<std::string> meshes;
};

void processFiles(const std::filesystem::directory_entry &entry, const ResourcePaths &resourcePaths,
                  Keys &keys) {
    if (!entry.is_regular_file())
        return;

    const auto &path = entry.path();
    auto artefactTypeOpt = getOutputPathAndType(path, resourcePaths);
    if (!artefactTypeOpt) {
        return;
    }
    const auto &[outputPath, type] = artefactTypeOpt.value();

    std::time_t destinationModificationTime =
        std::max(std::time_t{0}, modificationTime(outputPath));
    std::time_t srcModificationTime = modificationTime(path.string());

    if (srcModificationTime < destinationModificationTime) {
        return;
    }

    const auto key = ResourcePaths::getKey(resourcePaths, path);

    std::optional<std::tuple<ResourcePtr, std::chrono::steady_clock::duration>> resourceTime;
    switch (type) {
    case ResourceType::Shader:
        keys.shaders.push_back(key);
        resourceTime = parseShader(path, key);
        break;
    case ResourceType::Texture:
        keys.textures.push_back(key);
        resourceTime = parseTexture(path, key);
        break;
    case ResourceType::Mesh:
        keys.meshes.push_back(key);
        resourceTime = parseMesh(path, key);
        break;
    }

    if (!resourceTime) {
        std::cerr << "failed to parse: " << path << "\n";
        return;
    }

    const auto &[resource, parseTime] = resourceTime.value();
    Visitor v{[&srcModificationTime](const auto &ptr) -> void {
        setTimestamp(*ptr, srcModificationTime);
    }};
    std::visit(v, resource);

    writeData(key, parseTime, resource, outputPath, resourcePaths.outputDir);
};

void writeResources(Keys &keys, const ResourcePaths &resourcePaths) {

    std::stringstream buff;

    const std::filesystem::path resourceCpp = resourcePaths.outputDir / "Resource.cpp";

    buff << "#include \"Resource.hpp\"\n";
    buff << "#include \"ShaderData.h\"\n";
    buff << "#include \"TextureData.h\"\n";
    buff << "#include \"MeshData.hpp\"\n";
    buff << "\n";
    buff << "#include <array>\n";
    buff << "#include <tuple>\n";
    buff << "#include <string_view>\n";
    buff << "#include <variant>\n";
    buff << "\n";
    buff << "extern \"C\" {\n";
    for (const auto &key : keys.shaders) {
        buff << "\textern ShaderData_c " << key << "_data;\n";
    }
    for (const auto &key : keys.textures) {
        buff << "\textern TextureData_c " << key << "_data;\n";
    }
    for (const auto &key : keys.meshes) {
        buff << "\textern MeshData " << key << "_data;\n";
    }
    buff << "}\n";
    buff << "namespace {\n";

    auto writeArray = [&](std::string_view name, const std::vector<std::string> &keys,
                          std::string_view dataType) {
        if (keys.empty()) {
            buff << "const std::array<std::tuple<std::string_view, " << dataType << "*>, 0> "
                 << name << " = {};\n";
            return;
        }
        buff << "const auto " << name << " = std::to_array<std::tuple<std::string_view, "
             << dataType << "*>>({\n";
        for (const auto &key : keys) {
            buff << "\t{\"" << key << "\", &" << key << "_data},\n";
        }

        buff << "});\n";
    };

    writeArray("shaders", keys.shaders, "ShaderData_c");
    writeArray("textures", keys.textures, "TextureData_c");
    writeArray("meshes", keys.meshes, "MeshData");

    buff << "[[maybe_unused]] const auto init_data = []() noexcept { "
            "PreprocessorDataHolder::setData({.shaders=shaders, .textures=textures, "
            ".meshes=meshes}); return "
            "std::monostate{}; }();\n";

    buff << "}}";

    if (buff.str() != readFile(resourceCpp)) {
        auto start = std::chrono::high_resolution_clock::now();
        std::ofstream ostream(resourceCpp);
        ostream << buff.str();
        ostream.close();
        const auto writeTime = std::chrono::high_resolution_clock::now() - start;
        std::cout << std::filesystem::relative(resourceCpp, resourcePaths.outputDir).string()
                  << ": (Writing: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(writeTime) << "ms)\n";
    }
}
} // namespace

void resourcePreprocessor(const ResourcePaths &resourcePaths) {
    if (!std::filesystem::exists(resourcePaths.outputDir)) {
        std::filesystem::create_directories(resourcePaths.outputDir);
    }

    Keys keys;

    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(resourcePaths.inputDir)) {
        processFiles(entry, resourcePaths, keys);
    }

    writeResources(keys, resourcePaths);
}
