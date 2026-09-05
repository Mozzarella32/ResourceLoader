#include "ResourceLoader/Paths.hpp"
#include "ResourceLoader/private/Preprocessor.hpp"

#ifndef RESOURCE_PREPROCESSOR_RESOURCES_DIR
#warning "RESOURCE_PREPROCESSOR_RESOURCES_DIR has to be set"
auto main() -> int {}
#else
int main() {
    const std::filesystem::path resourcesDir = RESOURCE_PREPROCESSOR_RESOURCES_DIR;

    ResourceLoader::Paths resourcePaths{.inputDir = resourcesDir / "input",
                                                .outputDir = resourcesDir / "output"};
    ResourceLoader::Paths::info(resourcePaths);
    ResourceLoader::preprocessor(resourcePaths);
}
#endif
