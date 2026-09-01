#include "ResourcePaths.hpp"
#include "ResourcePreprocessor.hpp"

#ifndef RESOURCE_PREPROCESSOR_RESOURCES_DIR
#warning "RESOURCE_PREPROCESSOR_RESOURCES_DIR has to be set"
auto main() -> int {}
#else
int main() {
    const std::filesystem::path resourcesDir = RESOURCE_PREPROCESSOR_RESOURCES_DIR;

    ResourcePaths resourcePaths{.inputDir = resourcesDir / "input",
                                .outputDir = resourcesDir / "output"};
    ResourcePaths::info(resourcePaths);
    resourcePreprocessor(resourcePaths);
}
#endif
