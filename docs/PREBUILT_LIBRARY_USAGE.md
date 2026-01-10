# 预编译库使用指南

本文档说明如何构建和使用RenderEngine的预编译库，以缩短其他项目的编译时间。

## 目录

- [构建预编译库](#构建预编译库)
- [使用预编译库](#使用预编译库)
- [常见问题](#常见问题)

## 构建预编译库

### 步骤1: 构建项目

首先，确保项目已经完成构建：

```powershell
# 配置CMake（如果还没有）
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建Release版本
cmake --build . --config Release
```

### 步骤2: 安装库

使用CMake安装命令将库安装到指定目录：

```powershell
# 在build目录中执行
cmake --install . --config Release --prefix ../install
```

或者，如果你想直接安装到系统目录（需要管理员权限）：

```powershell
cmake --install . --config Release
```

### 步骤3: 打包预编译库（可选）

使用提供的打包脚本创建可分发的预编译库包：

```powershell
# 从项目根目录运行
PowerShell -ExecutionPolicy Bypass -File .\scripts\package_prebuilt.ps1 -BuildDir build -Config Release -Arch x64
```

这将在 `prebuilt/` 目录下创建一个包含所有必要文件的包。

## 使用预编译库

### 方法1: 使用CMake find_package（推荐）

这是最推荐的方式，因为它会自动处理依赖关系和路径。

#### 在你的项目中

1. **设置RenderEngine路径**

在你的 `CMakeLists.txt` 中，在 `find_package` 之前设置路径：

```cmake
# 方式A: 如果预编译库在项目目录中
set(RenderEngine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine-prebuilt-Release-x64/lib/cmake/RenderEngine")

# 方式B: 如果安装在系统目录
# 不需要设置，CMake会自动查找

# 方式C: 使用环境变量
# 设置环境变量 RenderEngine_DIR 指向 lib/cmake/RenderEngine 目录
```

2. **查找并链接库**

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyProject)

# 查找RenderEngine
find_package(RenderEngine REQUIRED)

# 创建你的可执行文件
add_executable(my_app main.cpp)

# 链接RenderEngine
target_link_libraries(my_app PRIVATE RenderEngine::RenderEngine)

# 如果需要访问shader文件路径
target_compile_definitions(my_app PRIVATE 
    SHADER_DIR="${RenderEngine_SHADER_DIR}"
)
```

#### 完整示例

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyRenderApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置RenderEngine路径（根据你的实际情况调整）
set(RenderEngine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/RenderEngine/lib/cmake/RenderEngine")

# 查找RenderEngine
find_package(RenderEngine REQUIRED)

# 创建可执行文件
add_executable(my_app
    src/main.cpp
)

# 链接库
target_link_libraries(my_app PRIVATE RenderEngine::RenderEngine)

# 如果需要shader路径
message(STATUS "RenderEngine Shader Dir: ${RenderEngine_SHADER_DIR}")
```

### 方法2: 直接包含（简单项目）

对于简单的项目，可以直接指定路径：

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# 设置预编译库路径
set(RENDER_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine-prebuilt-Release-x64")

# 添加头文件路径
target_include_directories(my_app PRIVATE 
    "${RENDER_ENGINE_ROOT}/include"
)

# 链接库文件
if(WIN32)
    set(RENDER_ENGINE_LIB "${RENDER_ENGINE_ROOT}/lib/RenderEngine.lib")
else()
    set(RENDER_ENGINE_LIB "${RENDER_ENGINE_ROOT}/lib/libRenderEngine.a")
endif()

target_link_libraries(my_app PRIVATE ${RENDER_ENGINE_LIB})

# 链接OpenGL（RenderEngine需要）
find_package(OpenGL REQUIRED)
target_link_libraries(my_app PRIVATE OpenGL::GL)
```

### 方法3: 作为子项目（开发时）

如果你在开发RenderEngine或需要频繁修改，可以将其作为子项目：

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp)

set(CMAKE_CXX_STANDARD 20)

# 添加RenderEngine作为子目录
add_subdirectory(path/to/RenderEngine)

# 创建可执行文件
add_executable(my_app main.cpp)

# 链接库
target_link_libraries(my_app PRIVATE RenderEngine::RenderEngine)
```

## 目录结构

预编译库的目录结构如下：

```
RenderEngine-prebuilt-Release-x64/
├── lib/                           # 库文件目录
│   ├── RenderEngine.lib          # 静态库（Windows）
│   │   (或 libRenderEngine.a)    # 静态库（Linux）
│   └── cmake/
│       └── RenderEngine/         # CMake配置文件
│           ├── RenderEngineConfig.cmake
│           ├── RenderEngineConfigVersion.cmake
│           └── RenderEngineTargets.cmake
├── include/                       # 头文件目录
│   └── render/                    # RenderEngine头文件
│       ├── renderer.h
│       ├── shader.h
│       └── ...
└── share/
    └── RenderEngine/
        └── shaders/               # Shader文件
            ├── sprite.vert
            ├── sprite.frag
            └── ...
```

## 依赖项说明

### 已包含的依赖

以下依赖已静态链接到RenderEngine库中，**不需要**单独链接：
- SDL3
- SDL3_image
- SDL3_ttf
- Assimp
- meshoptimizer
- Bullet Physics
- Eigen3（仅头文件）
- GLAD
- ImGui
- nlohmann/json（仅头文件）

### 需要单独提供的依赖

以下依赖**需要**在你的项目中单独提供：
- **OpenGL 4.5+** - 通过系统驱动提供
- **C++20编译器** - MSVC 2019+, GCC 10+, Clang 12+

### 链接OpenGL

在使用预编译库时，你仍需要链接OpenGL：

```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(your_target PRIVATE OpenGL::GL)
```

## 常见问题

### Q1: find_package找不到RenderEngine

**解决方案：**

1. 检查路径是否正确：
   ```cmake
   set(RenderEngine_DIR "完整路径/lib/cmake/RenderEngine")
   ```

2. 确保路径指向包含 `RenderEngineConfig.cmake` 的目录

3. 使用 `message()` 调试：
   ```cmake
   message(STATUS "Looking for RenderEngine in: ${RenderEngine_DIR}")
   find_package(RenderEngine REQUIRED)
   ```

### Q2: 链接错误，找不到符号

**可能原因：**
- 编译选项不匹配（如C++标准版本）
- 架构不匹配（x86 vs x64）

**解决方案：**
- 确保使用相同的C++标准（C++20）
- 确保架构匹配（x64 vs x86）
- 重新构建预编译库，使用与你的项目相同的编译选项

### Q3: 运行时找不到DLL（Windows）

**说明：** RenderEngine是静态库，理论上不应该需要DLL。但如果出现此错误：

1. 检查是否有第三方库的DLL依赖
2. 确保所有必要的运行时库都已安装（Visual C++ Redistributable）

### Q4: Shader文件路径问题

**解决方案：**

使用CMake变量获取shader路径：

```cmake
# 在CMakeLists.txt中
find_package(RenderEngine REQUIRED)
target_compile_definitions(your_target PRIVATE 
    SHADER_DIR="${RenderEngine_SHADER_DIR}"
)

# 在C++代码中
#ifndef SHADER_DIR
#define SHADER_DIR "."
#endif

std::string shaderPath = std::string(SHADER_DIR) + "/sprite.vert";
```

或者，将shader文件复制到你的项目目录中。

### Q5: 如何更新预编译库

1. 重新构建RenderEngine项目
2. 重新运行安装命令
3. 如果使用打包脚本，重新运行打包脚本
4. 在你的项目中，确保CMake缓存已清除：
   ```powershell
   Remove-Item -Recurse build
   mkdir build
   cd build
   cmake ..
   ```

## 性能优化建议

1. **使用Release配置**：预编译库应使用Release配置构建以获得最佳性能

2. **链接时优化（LTO）**：如果可能，在构建预编译库时启用LTO：
   ```cmake
   cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
   ```

3. **并行编译**：构建预编译库时使用多核：
   ```powershell
   cmake --build . --config Release -j 8
   ```

## 版本兼容性

- **CMake版本**：需要CMake 3.15或更高版本
- **C++标准**：C++20
- **编译器**：MSVC 2019+, GCC 10+, Clang 12+
- **OpenGL**：4.5或更高版本

## 许可证

使用预编译库时，请遵守RenderEngine项目的许可证要求（AGPL-3.0）。
