cmake_minimum_required(VERSION 3.15)
project(RenderEngine VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    # UTF-8 编码支持
    add_compile_options(/utf-8)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # UTF-8 编码支持
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 第三方库路径
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# ============================================================================
# SDL3 配置 - 作为子项目编译
# ============================================================================
set(SDL3_DIR ${THIRD_PARTY_DIR}/SDL)

# 禁用 SDL3 的一些不需要的选项
set(SDL_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)

# 添加 SDL3 子项目
if(EXISTS ${SDL3_DIR}/CMakeLists.txt)
    message(STATUS "Adding SDL3 as subdirectory: ${SDL3_DIR}")
    add_subdirectory(${SDL3_DIR} ${CMAKE_BINARY_DIR}/SDL3 EXCLUDE_FROM_ALL)
    set(SDL3_LIBRARY SDL3::SDL3-static)
    set(SDL3_INCLUDE_DIRS ${SDL3_DIR}/include)
    message(STATUS "SDL3 configured as subproject")
else()
    message(FATAL_ERROR "SDL3 source not found at ${SDL3_DIR}")
endif()

# ============================================================================
# SDL3_image 配置 - 作为子项目编译
# ============================================================================
set(SDL3_IMAGE_DIR ${THIRD_PARTY_DIR}/SDL_image)

# 禁用 SDL3_image 的一些不需要的选项
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared library" FORCE)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "Enable SDL3_image install target" FORCE)
set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "Load dependencies dynamically" FORCE)
set(SDLIMAGE_VENDORED ON CACHE BOOL "Use vendored third-party libraries" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "Build SDL3_image samples" FORCE)

# 启用常用的图片格式支持（使用内置解码器）
set(SDLIMAGE_PNG ON CACHE BOOL "Enable PNG support" FORCE)
set(SDLIMAGE_JPG ON CACHE BOOL "Enable JPEG support" FORCE)
set(SDLIMAGE_BMP ON CACHE BOOL "Enable BMP support" FORCE)
set(SDLIMAGE_TGA ON CACHE BOOL "Enable TGA support" FORCE)

# 禁用不常用格式以加快编译
set(SDLIMAGE_AVIF OFF CACHE BOOL "Disable AVIF support" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "Disable JXL support" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "Disable TIFF support" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "Disable WEBP support" FORCE)

# 添加 SDL3_image 子项目
if(EXISTS ${SDL3_IMAGE_DIR}/CMakeLists.txt)
    message(STATUS "Adding SDL3_image as subdirectory: ${SDL3_IMAGE_DIR}")
    add_subdirectory(${SDL3_IMAGE_DIR} ${CMAKE_BINARY_DIR}/SDL3_image EXCLUDE_FROM_ALL)
    set(SDL3_IMAGE_LIBRARY SDL3_image::SDL3_image-static)
    set(SDL3_IMAGE_INCLUDE_DIRS ${SDL3_IMAGE_DIR}/include)
    message(STATUS "SDL3_image configured as subproject")
else()
    message(FATAL_ERROR "SDL3_image source not found at ${SDL3_IMAGE_DIR}")
endif()

# ============================================================================
# Eigen3 配置
# ============================================================================
set(EIGEN3_INCLUDE_DIR ${THIRD_PARTY_DIR}/eigen-3.4.0)
if(NOT EXISTS ${EIGEN3_INCLUDE_DIR})
    message(FATAL_ERROR "Eigen3 not found at ${EIGEN3_INCLUDE_DIR}")
endif()
message(STATUS "Using Eigen3 from: ${EIGEN3_INCLUDE_DIR}")

# ============================================================================
# GLAD 配置（OpenGL 加载库）
# ============================================================================
set(GLAD_DIR ${THIRD_PARTY_DIR}/glad)
set(GLAD_INCLUDE_DIR ${GLAD_DIR}/include)
set(GLAD_SRC ${GLAD_DIR}/src/glad.c)

if(NOT EXISTS ${GLAD_INCLUDE_DIR})
    message(FATAL_ERROR "GLAD not found at ${GLAD_DIR}")
endif()

# ============================================================================
# OpenGL 配置
# ============================================================================
find_package(OpenGL REQUIRED)

# ============================================================================
# 包含目录
# ============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_IMAGE_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${GLAD_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
)

# ============================================================================
# 源文件
# ============================================================================
set(RENDER_SOURCES
    # Core
    src/core/renderer.cpp
    src/core/opengl_context.cpp
    src/core/render_state.cpp
    
    # Rendering
    src/rendering/shader.cpp
    src/rendering/uniform_manager.cpp
    src/rendering/shader_cache.cpp
    src/rendering/texture.cpp
    src/rendering/texture_loader.cpp
    
    # Utils
    src/utils/logger.cpp
    src/utils/file_utils.cpp
    
    # GLAD
    ${GLAD_SRC}
)

set(RENDER_HEADERS
    # Core
    include/render/renderer.h
    include/render/opengl_context.h
    include/render/render_state.h
    include/render/types.h
    
    # Rendering
    include/render/shader.h
    include/render/uniform_manager.h
    include/render/shader_cache.h
    include/render/texture.h
    include/render/texture_loader.h
    
    # Utils
    include/render/logger.h
    include/render/file_utils.h
)

# ============================================================================
# 创建库
# ============================================================================
add_library(RenderEngine STATIC ${RENDER_SOURCES} ${RENDER_HEADERS})

# 链接库
target_link_libraries(RenderEngine
    PUBLIC
        ${SDL3_LIBRARY}
        ${SDL3_IMAGE_LIBRARY}
        ${OPENGL_LIBRARIES}
)

# ============================================================================
# 安装规则
# ============================================================================
install(TARGETS RenderEngine
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/render
    DESTINATION include
)

# ============================================================================
# 示例程序
# ============================================================================
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================================================
# 测试
# ============================================================================
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# 输出配置信息
# ============================================================================
message(STATUS "========================================")
message(STATUS "RenderEngine Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SDL3 Include: ${SDL3_INCLUDE_DIRS}")
message(STATUS "  SDL3 Library: ${SDL3_LIBRARY}")
message(STATUS "  SDL3_image Include: ${SDL3_IMAGE_INCLUDE_DIRS}")
message(STATUS "  SDL3_image Library: ${SDL3_IMAGE_LIBRARY}")
message(STATUS "  Eigen3 Include: ${EIGEN3_INCLUDE_DIR}")
message(STATUS "  OpenGL Include: ${OPENGL_INCLUDE_DIR}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "========================================")

