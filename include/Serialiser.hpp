#pragma once

#include "ResourcePreprocessor.hpp"

#include <stb_image.h>

#include <tiny_obj_loader.h>

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

class Serializer {
  private:
    std::reference_wrapper<std::ostream> ostreamRef;
    std::reference_wrapper<size_t> indent;

    auto getOstream() -> std::ostream &;
    auto getIndent() -> std::string;

  public:
    Serializer(std::ostream &ostream, size_t &indent);

    void decl(std::string_view name);
    void declC();
    void declNoAggregat(std::string_view name);
    void member(std::string_view memberName, const auto &memberValue, bool last = false);
    void memberNoAggregat(std::string_view memberName, const auto &memberValue, bool last = false);
    void exp(std::string_view memberName, const auto &expresionValue, bool last = false);
    void expNoAggregat(std::string_view memberName, const auto &expresionValue, bool last = false);

    void write(const std::string &str);

    class plain_string : public std::string {};

    void write(const plain_string &str);
    void write(const bool &value);
    void write(const float &value);
    void write(const int &value);
    void write(const std::uint64_t &value);
    void write(const unsigned int &value);
    void write(const stbi_uc &value);

    void write(const tinyobj::texture_type_t &texture);
    void write(const tinyobj::skin_weight_t &data);
    void write(const tinyobj::joint_and_weight_t &data);
    void write(const tinyobj::attrib_t &data);
    void write(const tinyobj::mesh_t &data);
    void write(const tinyobj::tag_t &data);
    void write(const tinyobj::lines_t &data);
    void write(const tinyobj::points_t &data);
    void write(const tinyobj::index_t &data);
    void write(const tinyobj::shape_t &data);
    void write(const tinyobj::texture_option_t &data);
    void write(const tinyobj::material_t &data);

    void write(const ShaderData &data, std::string_view key);
    void write(const TextureData &data, std::string_view key);
    void write(const MeshData &data);

    // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    template <typename T> void write(const std::vector<T> &vec) {
        auto &ostream = getOstream();
        int perline = 1;
        if constexpr (std::same_as<T, unsigned int> || std::same_as<T, int> ||
                      std::same_as<T, float>) {
            perline = 8;
        } else if constexpr (std::same_as<T, stbi_uc>) {
            perline = 16;
        } else if constexpr (std::same_as<T, std::string> ||
                             std::same_as<T, tinyobj::skin_weight_t> ||
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
                ostream << getIndent();
            }
            indent++;
            write(element);
            indent--;
            ostream << ", ";
            newline = (newline + 1) % perline;
            if (newline == 0) {
                ostream << "\n";
            }
        }
        ostream << "\n";
        indent--;
        ostream << getIndent() << "}";
        indent++;
    }

    template <typename K, typename V> void write(const std::map<K, V> &map) {
        auto &ostream = getOstream();
        if (map.empty()) {
            ostream << "{}";
            return;
        }
        ostream << "{\n";
        for (const auto &[k, v] : map) {
            ostream << std::string(indent, '\t');
            ostream << "{";
            indent++;
            write(k);
            ostream << ", ";
            write(v);
            indent--;
            ostream << ", ";
            ostream << "\n";
        }
        ostream << "\n";
        indent--;
        ostream << getIndent() << "}";
        indent++;
    }

    template <typename T, size_t N> void write(const T (&arr)[N]) {
        auto &ostream = getOstream();
        int perline = 1;
        if constexpr (std::same_as<T, unsigned int> || std::same_as<T, int> ||
                      std::same_as<T, int> || std::same_as<T, float>) {
            perline = 8;
        } else if constexpr (std::same_as<T, stbi_uc>) {
            perline = 16;
        } else if constexpr (std::same_as<T, std::string> ||
                             std::same_as<T, tinyobj::skin_weight_t> ||
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
                ostream << getIndent();
                ;
            }
            indent++;
            write(arr[i]);
            indent--;
            if (i != N - 1)
                ostream << ", ";
            newline = (newline + 1) % perline;
            if (newline == 0 && i != N - 1) {
                ostream << "\n";
            }
        }
        ostream << "\n";
        indent--;
        ostream << getIndent() << "}";
        indent++;
    }
    // NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
};
