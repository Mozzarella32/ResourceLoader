cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ResourceLoader LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(STB_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/submodules/stb")
set(TINY_OBJ_LOADER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/submodules/tinyobjloader")

find_package(glslang QUIET)

if(glslang_FOUND)
    find_path(GLSLANG_INCLUDE_DIR
        NAMES SPIRV/GlslangToSpv.h
        HINTS
            ${glslang_INCLUDE_DIRS}
            ${GLSLANG_INCLUDE_DIR}
        PATH_SUFFIXES
            glslang
    )
else()
    set(GLSLANG_EXTERNAL_ROOT "${CMAKE_SOURCE_DIR}/external/glslang")

    message(STATUS "glslang not found on system.")
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "Skip building examples for SPIRV-Headers")
    set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "Skip install for SPIRV-Headers")

    # Tools
    set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building the executable and tests along with the library")
    set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip building tests along with the library")
    set(SPIRV_BUILD_FUZZER OFF CACHE BOOL "Disable spirv-fuzz")
    set(SPIRV_BUILD_LIBFUZZER_TARGETS OFF CACHE BOOL "Disable libFuzzer targets")
    set(SPIRV_BUILD_COMPRESSION OFF CACHE BOOL "Disable compression codec (removed anyway)")
    set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip installation")
    set(SPIRV_TOOLS_BUILD_STATIC ON CACHE BOOL "")
    set(SPIRV_TOOLS_BUILD_SHARED OFF CACHE BOOL "SPIRV-Tools shared" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
    set(SPIRV_TOOLS_LIBRARY_TYPE "STATIC" CACHE STRING "")

    # glslang
    set(BUILD_EXTERNAL ON CACHE BOOL "")
    set(BUILD_WERROR OFF CACHE BOOL "")
    set(GLSLANG_TESTS OFF CACHE BOOL "")
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "")
    set(ENABLE_SPIRV ON CACHE BOOL "")
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
    set(ENABLE_HLSL OFF CACHE BOOL "")
    set(ENABLE_OPT ON CACHE BOOL "")
    set(ENABLE_PCH OFF CACHE BOOL "")

    FetchContent_Declare(
      spirv_headers
      GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
      GIT_TAG main
    )

    FetchContent_Declare(
      spirv_tools
      GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
      GIT_TAG main
    )

    FetchContent_Declare(
      glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
      GIT_TAG main
    )

    FetchContent_MakeAvailable(spirv_headers spirv_tools glslang)
endif()

if (MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Werror -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
endif()

file(GLOB_RECURSE SRCS
   CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(ResourceLoader STATIC ${SRCS})
add_library(ResourceLoader::Library ALIAS ResourceLoader)

target_include_directories(ResourceLoader
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/Visitor"
)

target_link_libraries(ResourceLoader
    PRIVATE
        glslang::glslang
        glslang::SPIRV
        glslang::glslang-default-resource-limits
)

target_include_directories(ResourceLoader
    SYSTEM PUBLIC
        "${TINY_OBJ_LOADER_INCLUDE_DIR}"
        "${STB_INCLUDE_DIR}"
        "${GLSLANG_INCLUDE_DIR}"
)

add_library(ResourceLoaderHeaders INTERFACE)
add_library(ResourceLoader::Headers ALIAS ResourceLoaderHeaders)

target_include_directories(ResourceLoaderHeaders
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/submodules/Visitor"
        "${TINY_OBJ_LOADER_INCLUDE_DIR}"
        "${STB_INCLUDE_DIR}"
        "${GLSLANG_INCLUDE_DIR}"
)

if (RESOURCE_PREPROCESSOR_RESOURCES_DIR)
    add_executable(ResourceLoaderPreprocessor "${CMAKE_CURRENT_SOURCE_DIR}/standalone/main.cpp")

    target_compile_definitions(ResourceLoaderPreprocessor
        PRIVATE
            RESOURCE_PREPROCESSOR_RESOURCES_DIR="${RESOURCE_PREPROCESSOR_RESOURCES_DIR}"
    )

    target_link_libraries(ResourceLoaderPreprocessor
        PRIVATE
            ResourceLoader::Library
    )

    set(RESOURCE_PREPROCESSOR_INPUT_DIR "${RESOURCE_PREPROCESSOR_RESOURCES_DIR}/input")
    set(RESOURCE_PREPROCESSOR_OUTPUT_DIR "${RESOURCE_PREPROCESSOR_RESOURCES_DIR}/output")

    file(GLOB_RECURSE RESOURCE_INPUT_FILES
        CONFIGURE_DEPENDS
        "${RESOURCE_PREPROCESSOR_INPUT_DIR}/*"
    )

    set(RESOURCE_OUTPUT_FILES)

    set(RESOURCE_SHADER_EXTENSIONS frag vert geom comp tesc tese spv)
    set(RESOURCE_TEXTURE_EXTENSIONS png jpg)
    set(RESOURCE_MESH_EXTENSIONS obj)

    foreach(resource IN LISTS RESOURCE_INPUT_FILES)
        if(IS_DIRECTORY "${resource}")
            continue()
        endif()

        file(RELATIVE_PATH relative_resource
            "${RESOURCE_PREPROCESSOR_INPUT_DIR}"
            "${resource}"
        )

        get_filename_component(resource_dir
            "${relative_resource}"
            DIRECTORY
        )

        get_filename_component(resource_name
            "${relative_resource}"
            NAME_WE
        )

        get_filename_component(resource_extension
            "${relative_resource}"
            EXT
        )

        string(SUBSTRING "${resource_extension}" 1 -1 resource_extension)

        if(resource_extension IN_LIST RESOURCE_SHADER_EXTENSIONS)
            set(generated_extension ".c")
        elseif(resource_extension IN_LIST RESOURCE_TEXTURE_EXTENSIONS)
            set(generated_extension ".c")
        elseif(resource_extension IN_LIST RESOURCE_MESH_EXTENSIONS)
            set(generated_extension ".cpp")
        else()
            continue()
        endif()

        set(generated_file "${RESOURCE_PREPROCESSOR_OUTPUT_DIR}/${resource_dir}/${resource_name}_${resource_extension}${generated_extension}")

        list(APPEND RESOURCE_OUTPUT_FILES "${generated_file}")
    endforeach()
        list(APPEND RESOURCE_OUTPUT_FILES "${RESOURCE_PREPROCESSOR_OUTPUT_DIR}/Resource.cpp")

    add_custom_command(
        OUTPUT ${RESOURCE_OUTPUT_FILES}
        COMMAND $<TARGET_FILE:ResourceLoaderPreprocessor>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ResourceLoaderPreprocessor
        COMMENT "Running ResourceLoaderPreprocessor"
        VERBATIM
    )

    add_library(ResourceLoaderPreprocessedLibrary
        ${RESOURCE_OUTPUT_FILES}
    )

    target_link_libraries(ResourceLoaderPreprocessedLibrary
        PRIVATE
            ResourceLoader::Headers
    )

    add_library(ResourceLoader::Preprocessed ALIAS ResourceLoaderPreprocessedLibrary)
endif()
