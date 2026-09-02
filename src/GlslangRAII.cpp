#include "ResourceLoader/GlslangRAII.hpp"

#include <glslang/Public/ShaderLang.h>
// #include <SPIRV/GlslangToSpv.h>

namespace ResourceLoader {
GlslangRAII::GlslangRAII() { glslang::InitializeProcess(); }
GlslangRAII::~GlslangRAII() { glslang::FinalizeProcess(); }

} // namespace ResourceLoader
