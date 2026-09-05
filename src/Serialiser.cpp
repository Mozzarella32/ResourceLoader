#include "ResourceLoader/private/Serialiser.hpp"

#include "ResourceLoader/private/MeshData.hpp"
#include "ResourceLoader/private/ShaderData.hpp"
#include "ResourceLoader/private/TextureData.hpp"

#include <ios>
#include <stb_image.h>

#include <tiny_obj_loader.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>

auto Serializer::getOstream() -> std::ostream & { return ostreamRef.get(); }

void Serializer::writeIndent() {
    auto &ostream = getOstream();
    ostream << std::string(indent, '\t');
}
Serializer::Serializer(std::ostream &ostream, size_t &indent)
    : ostreamRef(ostream), indent(indent) {}

void Serializer::decl(std::string_view name) {
    auto &ostream = getOstream();
    ostream << name << " {";
    ostream << "\n";
    writeIndent();
}

void Serializer::declC() {
    auto &ostream = getOstream();
    ostream << " {";
    ostream << "\n";
    writeIndent();
}

void Serializer::declNoAggregat(std::string_view name) {
    auto &ostream = getOstream();
    ostream << " []() {";
    ostream << "\n";
    writeIndent();
    ostream << name << " data;";
    ostream << "\n";
    writeIndent();
}

void Serializer::write(std::string_view str) {
    auto &ostream = getOstream();
    ostream << "\"" << str << "\"";
}

void Serializer::write(const plain_string &str) {
    auto &ostream = getOstream();
    ostream << str;
}

void Serializer::write(const bool &value) {
    auto &ostream = getOstream();
    if (value) {
        ostream << "true";
    } else {
        ostream << "false";
    }
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
void Serializer::write(const float &value) {
    auto &ostream = getOstream();
    ostream << std::setfill(' ') << std::setw(9) << value;
}

void Serializer::write(const int &value) {
    auto &ostream = getOstream();
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

void Serializer::write(const std::uint64_t &value) {
    auto &ostream = getOstream();
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(8) << value << "U";
}

void Serializer::write(const unsigned int &value) {
    auto &ostream = getOstream();
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(8) << value << "U";
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

void Serializer::write(const stbi_uc &value) {
    auto &ostream = getOstream();
    ostream << std::hex << std::setfill('0');
    ostream << "0x" << std::setw(2) << static_cast<short>(value);
}

void Serializer::write(const tinyobj::texture_type_t &texture) {
    auto &ostream = getOstream();
    ostream << std::hex << std::setfill('0');
    ostream << "tinyobj::texture_type_t(0x" << std::setw(3) << static_cast<size_t>(texture) << ")";
}

void Serializer::write(const ShaderData &data, std::string_view key) {
    declC();
    member("timestamp", data.timestamp);
    exp("data", plain_string(std::string(key) + "_data_spv"));
    member("data_len", data.data_len, true);
}

void Serializer::write(const TextureData &data, std::string_view key) {
    declC();
    member("timestamp", data.timestamp);
    member("width", data.width);
    member("height", data.height);
    exp("pixels", plain_string(std::string(key) + "_data_pixels"));
    member("pixels_len", data.pixels_len, true);
}

void Serializer::write(const tinyobj::attrib_t &data) {
    declNoAggregat("tinyobj::attrib_t");
    memberNoAggregat("vertices", data.vertices);
    memberNoAggregat("normals", data.normals);
    memberNoAggregat("texcoords", data.texcoords);
    memberNoAggregat("colors", data.colors,true);
}

void Serializer::write(const tinyobj::mesh_t &data) {
    decl("tinyobj::mesh_t");
    member("indices", data.indices);
    member("num_face_vertices", data.num_face_vertices);
    member("material_ids", data.material_ids);
    member("smoothing_group_ids", data.smoothing_group_ids);
    member("tags", data.tags, true);
}

void Serializer::write(const tinyobj::tag_t &data) {
    decl("tinyobj::tag_t");
    member("name", data.name);
    member("intValues", data.intValues);
    member("floatValues", data.floatValues);
    member("stringValues", data.stringValues, true);
}

void Serializer::write(const tinyobj::index_t &data) {
    decl("tinyobj::index_t");
    member("vertex_index", data.vertex_index);
    member("normal_index", data.normal_index);
    member("texcoord_index", data.texcoord_index, true);
}

void Serializer::write(const tinyobj::shape_t &data) {
    decl("tinyobj::shape_t");
    member("name", data.name);
    member("mesh", data.mesh,true);
}

void Serializer::write(const tinyobj::texture_option_t &data) {
    decl("tinyobj::texture_option_t");
    member("type", data.type);
    member("sharpness", data.sharpness);
    member("brightness", data.brightness);
    member("contrast", data.contrast);
    member("origin_offset", data.origin_offset);
    member("scale", data.scale);
    member("turbulence", data.turbulence);
    member("clamp", data.clamp);
    member("imfchan", data.imfchan);
    member("blendu", data.blendu);
    member("blendv", data.blendv);
    member("bump_multiplier", data.bump_multiplier);
    member("colorspace", data.colorspace, true);
}

void Serializer::write(const tinyobj::material_t &data) {
    decl("tinyobj::material_t");
    member("name", data.name);
    member("ambient", data.ambient);
    member("diffuse", data.diffuse);
    member("specular", data.specular);
    member("transmittance", data.transmittance);
    member("emission", data.emission);
    member("shininess", data.shininess);
    member("ior", data.ior);
    member("dissolve", data.dissolve);
    member("illum", data.illum);
    member("dummy", data.dummy);
    member("ambient_texname", data.ambient_texname);
    member("diffuse_texname", data.diffuse_texname);
    member("specular_texname", data.specular_texname);
    member("specular_highlight_texname", data.specular_highlight_texname);
    member("bump_texname", data.bump_texname);
    member("displacement_texname", data.displacement_texname);
    member("alpha_texname", data.alpha_texname);
    member("reflection_texname", data.reflection_texname);
    member("ambient_texopt", data.ambient_texopt);
    member("diffuse_texopt", data.diffuse_texopt);
    member("specular_texopt", data.specular_texopt);
    member("specular_highlight_texopt", data.specular_highlight_texopt);
    member("bump_texopt", data.bump_texopt);
    member("displacement_texopt", data.displacement_texopt);
    member("alpha_texopt", data.alpha_texopt);
    member("reflection_texopt", data.reflection_texopt);
    member("roughness", data.roughness);
    member("metallic", data.metallic);
    member("sheen", data.sheen);
    member("clearcoat_thickness", data.clearcoat_thickness);
    member("clearcoat_roughness", data.clearcoat_roughness);
    member("anisotropy", data.anisotropy);
    member("anisotropy_rotation", data.anisotropy_rotation);
    member("pad0", data.pad0);
    member("roughness_texname", data.roughness_texname);
    member("metallic_texname", data.metallic_texname);
    member("sheen_texname", data.sheen_texname);
    member("emissive_texname", data.emissive_texname);
    member("normal_texname", data.normal_texname);
    member("roughness_texopt", data.roughness_texopt);
    member("metallic_texopt", data.metallic_texopt);
    member("emissive_texopt", data.emissive_texopt);
    member("normal_texopt", data.normal_texopt);
    member("pad2", data.pad2);
    member("unknown_parameter", data.unknown_parameter, true);
}

void Serializer::write(const MeshData &data) {
    decl("MeshData");
    member("timestamp", data.timestamp);
    member("attrib", data.attrib);
    member("shapes", data.shapes);
    member("materials", data.materials, true);
}

void Serializer::expNoAggregat(std::string_view memberName, const auto &expresionValue, bool last) {
    auto &ostream = getOstream();
    ostream << "data." << memberName << " = ";
    indent++;
    write(expresionValue);
    indent--;
    ostream << ";\n";
    writeIndent();
    if (last) {
        ostream << "return data";
        indent--;
        ostream << ";\n";
        writeIndent();
        indent++;
        ostream << "}()";
    }
}

void Serializer::exp(std::string_view memberName, const auto &expresionValue, bool last) {
    auto &ostream = getOstream();
    ostream << "." << memberName << " = ";
    indent++;
    write(expresionValue);
    ostream << ",\n";
    writeIndent();
    indent--;
    if (last) {
        ostream << "}";
    }
}

void Serializer::memberNoAggregat(std::string_view memberName, const auto &memberValue, bool last) {
    auto &ostream = getOstream();
    ostream << "data." << memberName << " = ";
    indent++;
    write(memberValue);
    indent--;
    ostream << ";\n";
    writeIndent();
    if (last) {
        indent--;
        ostream << ";\n";
        writeIndent();
        indent++;
        ostream << "return data";
        ostream << "}()";
    }
}

void Serializer::member(std::string_view memberName, const auto &memberValue, bool last) {
    auto &ostream = getOstream();
    ostream << "." << memberName << " = ";
    indent++;
    write(memberValue);
    indent--;
    if (!last) {
        ostream << ",\n";
        writeIndent();
        return;
    }
    ostream << "}";
}
