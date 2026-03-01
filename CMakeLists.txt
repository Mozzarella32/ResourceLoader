cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ResourceLoader LANGUAGES CXX C)

inclulde (FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(STB_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/submodules/stb")

set(TINY_OBJ_LOADER_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/submodules/tinyobjloader")

find_package(glslang QUIET)

if(NOT glslang_FOUND)
    set(GLSLANG_EXTERNAL_ROOT "${CMAKE_SOURCE_DIR}/external/glslang")

    message(STATUS "glslang not found on system.")
    set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "Skip building examples for SPIRV-Headers")
    set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "Skip install for SPIRV-Headers")

    #Tools
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

    #glslang
    set(BUILD_EXTERNAL ON CACHE BOOL "")             
    set(BUILD_WERROR OFF CACHE BOOL "")              
    set(GLSLANG_TESTS OFF CACHE BOOL "")             
    set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "")    
    set(ENABLE_SPIRV ON CACHE BOOL "")               
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")   
    set(ENABLE_HLSL OFF CACHE BOOL "")               
    set(ENABLE_OPT ON CACHE BOOL "")                
    set(ENABLE_PCH OFF CACHE BOOL "")                
    set(GLSLANG_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/external")


    FetchContent_Declare(
      spirv_headers
      GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
      SOURCE_DIR ${GLSLANG_EXTERNAL_ROOT}/External/spirv-tools/external/spirv-headers
    )
    FetchContent_Declare(
      spirv_tools
      GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
      SOURCE_DIR ${GLSLANG_EXTERNAL_ROOT}/External/spirv-tools
    )
    FetchContent_Declare(
      glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
      GIT_TAG ${GLSLANG_COMMIT}
      SOURCE_DIR ${GLSLANG_EXTERNAL_ROOT}
    )

    FetchContent_MakeAvailable(spirv_headers spirv_tools)
    FetchContent_MakeAvailable(glslang)
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

add_library(${PROJECT_NAME} STATIC ${SRCS})

target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/submodules/Visitor"
)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${TINY_OBJ_LOADER_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${GLSLANG_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glslang::glslang
    glslang::SPIRV
)
