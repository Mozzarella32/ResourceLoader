#pragma once

#include <cstdint>
#include <vector>

struct ShaderData {
    uint64_t timestamp;
    std::vector<uint32_t> data;
    uint64_t data_len;
};
