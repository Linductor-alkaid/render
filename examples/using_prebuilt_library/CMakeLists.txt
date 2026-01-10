# 示例：如何在其他项目中使用RenderEngine预编译库
# 
# 使用方法：
# 1. 将RenderEngine预编译库解压到项目目录（例如 third_party/RenderEngine）
# 2. 修改下面的 RENDER_ENGINE_PREBUILT_PATH 指向预编译库目录
# 3. 运行 cmake 和构建

cmake_minimum_required(VERSION 3.15)
project(MyRenderApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# 方式1: 使用 find_package（推荐）
# ============================================================================

# 设置RenderEngine预编译库路径
# 假设预编译库在项目根目录的 third_party/RenderEngine 下
set(RENDER_ENGINE_PREBUILT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../prebuilt/RenderEngine-prebuilt-Release-x64")

# 设置RenderEngine_DIR指向CMake配置目录
set(RenderEngine_DIR "${RENDER_ENGINE_PREBUILT_PATH}/lib/cmake/RenderEngine")

# 查找RenderEngine包
find_package(RenderEngine REQUIRED)

# 显示找到的信息
message(STATUS "RenderEngine found!")
message(STATUS "  Version: ${RenderEngine_VERSION}")
message(STATUS "  Include dirs: ${RenderEngine_INCLUDE_DIRS}")
message(STATUS "  Shader dir: ${RenderEngine_SHADER_DIR}")

# ============================================================================
# 方式2: 直接包含（备选方案，如果find_package不工作）
# ============================================================================
# 如果上面的find_package不工作，可以取消注释下面的代码：

# set(RENDER_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../prebuilt/RenderEngine-prebuilt-Release-x64")
# 
# # 添加头文件路径
# include_directories("${RENDER_ENGINE_ROOT}/include")
# 
# # 链接库文件
# if(WIN32)
#     set(RENDER_ENGINE_LIB "${RENDER_ENGINE_ROOT}/lib/RenderEngine.lib")
# else()
#     set(RENDER_ENGINE_LIB "${RENDER_ENGINE_ROOT}/lib/libRenderEngine.a")
# endif()

# ============================================================================
# 创建可执行文件
# ============================================================================

add_executable(my_app
    main.cpp
)

# 链接RenderEngine（使用方式1）
target_link_libraries(my_app PRIVATE RenderEngine::RenderEngine)

# 如果使用方式2，取消注释下面这行：
# target_link_libraries(my_app PRIVATE ${RENDER_ENGINE_LIB})

# 链接OpenGL（RenderEngine需要）
find_package(OpenGL REQUIRED)
target_link_libraries(my_app PRIVATE OpenGL::GL)

# 如果需要shader路径，可以添加编译定义
target_compile_definitions(my_app PRIVATE 
    SHADER_DIR="${RenderEngine_SHADER_DIR}"
)
