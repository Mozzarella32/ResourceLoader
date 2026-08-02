#include "GlslangRAII.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

GlslangRAII::GlslangRAII() { glslang::InitializeProcess(); }
GlslangRAII::~GlslangRAII() { glslang::FinalizeProcess(); }
