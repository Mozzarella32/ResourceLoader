#pragma once

#include <cstdint>
#include <tiny_obj_loader.h>
#include <vector>

struct MeshData {
    uint64_t timestamp;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
};
