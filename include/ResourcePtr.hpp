#pragma once

#include "MeshData.hpp"
#include "ShaderData.hpp"
#include "TextureData.hpp"

#include <memory>
#include <variant>

using ResourcePtr = std::variant<std::unique_ptr<ShaderData>, std::unique_ptr<TextureData>,
                                 std::unique_ptr<MeshData>>;
