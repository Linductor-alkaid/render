cmake_minimum_required(VERSION 3.15)
project(RenderEngine VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项（仅用于变量保存，不全局应用）
if(MSVC)
    set(PROJECT_COMPILE_OPTIONS /utf-8 /W4 /WX-)
    # 启用 SIMD 优化（AVX2）以提升 Eigen 性能
    list(APPEND PROJECT_COMPILE_OPTIONS /arch:AVX2)
else()
    set(PROJECT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    # 启用 SIMD 优化（AVX2 + FMA）
    list(APPEND PROJECT_COMPILE_OPTIONS -mavx2 -mfma)
endif()

# OpenMP 支持（用于批量变换的并行处理）
option(ENABLE_OPENMP "Enable OpenMP for parallel processing" ON)
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found, enabling parallel processing")
        list(APPEND PROJECT_COMPILE_OPTIONS ${OpenMP_CXX_FLAGS})
        set(PROJECT_LINK_OPTIONS ${OpenMP_CXX_FLAGS})
    else()
        message(STATUS "OpenMP not found, parallel processing disabled")
    endif()
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
# SDL3_ttf 配置 - 作为子项目编译
# ============================================================================
set(SDL3_TTF_DIR ${THIRD_PARTY_DIR}/SDL3_ttf-3.2.2)

# 配置 SDL3_ttf 构建选项
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared library" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "Enable SDL3_ttf install target" FORCE)
set(SDLTTF_INSTALL_CPACK OFF CACHE BOOL "Disable SDL3_ttf CPack packaging" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "Disable SDL3_ttf samples" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL3_ttf" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "Disable Harfbuzz for SDL3_ttf" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "Disable PlutoSVG for SDL3_ttf" FORCE)

if(EXISTS ${SDL3_TTF_DIR}/CMakeLists.txt)
    message(STATUS "Adding SDL3_ttf as subdirectory: ${SDL3_TTF_DIR}")
    add_subdirectory(${SDL3_TTF_DIR} ${CMAKE_BINARY_DIR}/SDL3_ttf EXCLUDE_FROM_ALL)
    set(SDL3_TTF_LIBRARY SDL3_ttf::SDL3_ttf-static)
    set(SDL3_TTF_INCLUDE_DIRS ${SDL3_TTF_DIR}/include)
    message(STATUS "SDL3_ttf configured as subproject")
else()
    message(FATAL_ERROR "SDL3_ttf source not found at ${SDL3_TTF_DIR}")
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
# Assimp 配置 - 作为子项目编译
# ============================================================================
set(ASSIMP_DIR ${THIRD_PARTY_DIR}/assimp)

# 禁用 Assimp 的一些不需要的选项
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp tests" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp tools" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Enable Assimp install target" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "Build all importers by default" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Build all exporters by default" FORCE)

# 只启用常用的模型格式
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "Enable OBJ import" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "Enable FBX import" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "Enable GLTF import" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "Enable Collada import" FORCE)
set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "Enable Blender import" FORCE)
set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "Enable 3DS import" FORCE)
set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "Enable PLY import" FORCE)
set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "Enable STL import" FORCE)
set(ASSIMP_BUILD_MMD_IMPORTER ON CACHE BOOL "Enable MMD (PMX/PMD/VMD) import" FORCE)

# 禁用导出器（我们只需要导入）
set(ASSIMP_NO_EXPORT ON CACHE BOOL "Disable export functionality" FORCE)

# 添加 Assimp 子项目
if(EXISTS ${ASSIMP_DIR}/CMakeLists.txt)
    message(STATUS "Adding Assimp as subdirectory: ${ASSIMP_DIR}")
    add_subdirectory(${ASSIMP_DIR} ${CMAKE_BINARY_DIR}/assimp EXCLUDE_FROM_ALL)
    set(ASSIMP_LIBRARY assimp)
    set(ASSIMP_INCLUDE_DIRS ${ASSIMP_DIR}/include ${CMAKE_BINARY_DIR}/assimp/include)
    message(STATUS "Assimp configured as subproject")
else()
    message(FATAL_ERROR "Assimp source not found at ${ASSIMP_DIR}")
endif()

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
    ${SDL3_TTF_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIRS}
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
    src/core/resource_manager.cpp
    src/core/async_resource_loader.cpp
    src/core/transform.cpp
    src/core/camera.cpp
    src/core/gl_thread_checker.cpp
    
    # ECS
    src/ecs/entity_manager.cpp
    src/ecs/world.cpp
    src/ecs/systems.cpp
    src/ecs/components.cpp
    
    # Rendering
    src/rendering/shader.cpp
    src/rendering/uniform_manager.cpp
    src/rendering/shader_cache.cpp
    src/rendering/texture.cpp
    src/rendering/texture_loader.cpp
    src/rendering/mesh.cpp
    src/rendering/mesh_loader.cpp
    src/rendering/material.cpp
    src/rendering/material_sort_key.cpp
    src/rendering/material_state_cache.cpp
    src/rendering/framebuffer.cpp
    src/rendering/renderable.cpp
    src/rendering/render_batching.cpp
    src/ecs/sprite_animation_script_registry.cpp

    # Debug
    src/debug/sprite_animation_debugger.cpp
    src/debug/sprite_animation_debug_panel.cpp

    # Sprite
    src/sprite/sprite.cpp
    src/sprite/sprite_sheet.cpp
    src/sprite/sprite_atlas.cpp
    src/sprite/sprite_atlas_importer.cpp
    src/sprite/sprite_batcher.cpp
    src/sprite/sprite_animator.cpp
    src/sprite/sprite_renderer.cpp
    src/sprite/sprite_layer.cpp
    
    # Text
    src/text/font.cpp
    src/text/text.cpp
    src/text/text_renderer.cpp

    # Shaders
    shaders/sprite.vert
    shaders/sprite.frag
    shaders/text.vert
    shaders/text.frag
    
    # Utils
    src/utils/logger.cpp
    src/utils/file_utils.cpp
    src/utils/error.cpp
    src/utils/resource_handle.cpp
    src/utils/resource_dependency.cpp
    
    # GLAD
    ${GLAD_SRC}
)

set(RENDER_HEADERS
    # Core
    include/render/renderer.h
    include/render/opengl_context.h
    include/render/render_state.h
    include/render/types.h
    include/render/resource_manager.h
    include/render/resource_handle.h
    include/render/resource_slot.h
    include/render/transform.h
    include/render/camera.h
    include/render/math_utils.h
    include/render/gl_thread_checker.h
    
    # ECS
    include/render/ecs/entity.h
    include/render/ecs/entity_manager.h
    include/render/ecs/component_registry.h
    include/render/ecs/components.h
    include/render/ecs/system.h
    include/render/ecs/systems.h
    include/render/ecs/world.h
    include/render/ecs/sprite_animation_script_registry.h
    
    # Rendering
    include/render/shader.h
    include/render/uniform_manager.h
    include/render/shader_cache.h
    include/render/texture.h
    include/render/texture_loader.h
    include/render/mesh.h
    include/render/mesh_loader.h
    include/render/material.h
    include/render/material_sort_key.h
    include/render/material_state_cache.h
    include/render/framebuffer.h
    include/render/renderable.h
    include/render/render_batching.h
    include/render/debug/sprite_animation_debugger.h
    include/render/debug/sprite_animation_debug_panel.h

    # Sprite
    include/render/sprite/sprite.h
    include/render/sprite/sprite_sheet.h
    include/render/sprite/sprite_atlas.h
    include/render/sprite/sprite_atlas_importer.h
    include/render/sprite/sprite_batcher.h
    include/render/sprite/sprite_animator.h
    include/render/sprite/sprite_renderer.h
    include/render/sprite/sprite_layer.h
    include/render/sprite/sprite_nineslice.h
    
    # Text
    include/render/text/font.h
    include/render/text/text.h
    include/render/text/text_renderer.h
    
    # Utils
    include/render/logger.h
    include/render/file_utils.h
    include/render/error.h
    include/render/resource_dependency.h
)

# ============================================================================
# 创建库
# ============================================================================
add_library(RenderEngine STATIC ${RENDER_SOURCES} ${RENDER_HEADERS})

# 头文件搜索路径
target_include_directories(RenderEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/single_include
)

# 设置编译选项（仅对 RenderEngine）
target_compile_options(RenderEngine PRIVATE ${PROJECT_COMPILE_OPTIONS})

# 链接库
target_link_libraries(RenderEngine
    PUBLIC
        ${SDL3_LIBRARY}
        ${SDL3_IMAGE_LIBRARY}
        ${SDL3_TTF_LIBRARY}
        ${ASSIMP_LIBRARY}
        ${OPENGL_LIBRARIES}
)

# 链接 OpenMP（如果启用）
if(ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(RenderEngine PUBLIC OpenMP::OpenMP_CXX)
endif()

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
message(STATUS "  SDL3_ttf Include: ${SDL3_TTF_INCLUDE_DIRS}")
message(STATUS "  SDL3_ttf Library: ${SDL3_TTF_LIBRARY}")
message(STATUS "  Eigen3 Include: ${EIGEN3_INCLUDE_DIR}")
message(STATUS "  Assimp Include: ${ASSIMP_INCLUDE_DIRS}")
message(STATUS "  Assimp Library: ${ASSIMP_LIBRARY}")
message(STATUS "  OpenGL Include: ${OPENGL_INCLUDE_DIR}")
message(STATUS "  Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "========================================")

