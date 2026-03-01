#include "GlslangRAII.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

GlslangRAII::GlslangRAII() { glslang::InitializeProcess(); }
GlslangRAII::~GlslangRAII() { glslang::FinalizeProcess(); }
