# 预编译库使用指南

本文档说明如何构建和使用RenderEngine的预编译库，以缩短其他项目的编译时间。

**重要提示**：
- **推荐在Windows上使用动态库版本**（`-DBUILD_SHARED_LIBS=ON`），可以避免静态库链接问题（见[Q8](#q8-sdl3链接错误lnk2019)）
- 静态库版本在Windows上可能遇到SDL3链接错误，这是Windows静态库链接机制的特性

## 目录

- [构建预编译库](#构建预编译库)
- [使用预编译库](#使用预编译库)
- [常见问题](#常见问题)

## 构建预编译库

### 步骤1: 构建项目

首先，确保项目已经完成构建：

**构建静态库（默认）**：
```powershell
# 配置CMake（如果还没有）
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建Release版本
cmake --build . --config Release
```

**构建动态库（推荐用于Windows，可避免静态库链接问题）**：
```powershell
# 配置CMake，启用动态库构建
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON

# 构建Release版本
cmake --build . --config Release
```

**注意**：在Windows上，建议使用动态库版本（`-DBUILD_SHARED_LIBS=ON`）以避免静态库链接问题（见[Q8](#q8-sdl3链接错误lnk2019)）。

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
    # 检查是否是动态库版本（检查bin目录是否存在）
    if(EXISTS "${RENDER_ENGINE_ROOT}/bin/RenderEngine.dll")
        # 动态库版本：链接导入库
        target_link_libraries(my_app PRIVATE
            "${RENDER_ENGINE_ROOT}/lib/RenderEngine.lib"
        )
        # 复制DLL到输出目录
        add_custom_command(TARGET my_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RENDER_ENGINE_ROOT}/bin/RenderEngine.dll"
            $<TARGET_FILE_DIR:my_app>
        )
    else()
        # 静态库版本：需要链接所有依赖库
        # 所有依赖的静态库文件都包含在lib目录中
        target_link_libraries(my_app PRIVATE
            "${RENDER_ENGINE_ROOT}/lib/RenderEngine.lib"
            "${RENDER_ENGINE_ROOT}/lib/SDL3-static.lib"
            "${RENDER_ENGINE_ROOT}/lib/SDL3_image-static.lib"
            "${RENDER_ENGINE_ROOT}/lib/SDL3_ttf-static.lib"
            "${RENDER_ENGINE_ROOT}/lib/assimp-vc143-mt.lib"  # 文件名可能因MSVC版本而异，请检查lib目录中的实际文件名
            "${RENDER_ENGINE_ROOT}/lib/meshoptimizer.lib"
            "${RENDER_ENGINE_ROOT}/lib/BulletDynamics.lib"
            "${RENDER_ENGINE_ROOT}/lib/BulletCollision.lib"
            "${RENDER_ENGINE_ROOT}/lib/LinearMath.lib"
        )
    endif()
else()
    # Linux/Mac: 静态库
    target_link_libraries(my_app PRIVATE
        "${RENDER_ENGINE_ROOT}/lib/libRenderEngine.a"
    )
endif()

# 链接OpenGL（RenderEngine需要）
find_package(OpenGL REQUIRED)
target_link_libraries(my_app PRIVATE OpenGL::GL)

# 链接OpenMP（RenderEngine需要，用于并行处理）
find_package(OpenMP REQUIRED)
target_link_libraries(my_app PRIVATE OpenMP::OpenMP_CXX)
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

**静态库版本**：
```
RenderEngine-prebuilt-Release-x64-Static/
├── lib/                           # 库文件目录
│   ├── RenderEngine.lib          # RenderEngine主库（Windows）
│   ├── SDL3-static.lib           # SDL3静态库（依赖）
│   ├── SDL3_image-static.lib     # SDL3_image静态库（依赖）
│   ├── SDL3_ttf-static.lib       # SDL3_ttf静态库（依赖）
│   ├── assimp-vc143-mt.lib      # Assimp模型加载库（依赖，文件名包含MSVC版本后缀）
│   ├── meshoptimizer.lib         # 网格优化库（依赖）
│   ├── BulletDynamics.lib        # Bullet物理引擎（依赖）
│   ├── BulletCollision.lib       # Bullet碰撞检测（依赖）
│   ├── LinearMath.lib            # Bullet数学库（依赖）
│   └── cmake/
│       └── RenderEngine/         # CMake配置文件
│           ├── RenderEngineConfig.cmake
│           ├── RenderEngineConfigVersion.cmake
│           └── RenderEngineTargets.cmake
```

**动态库版本**（Windows）：
```
RenderEngine-prebuilt-Release-x64/
├── bin/                           # 运行时文件目录
│   ├── RenderEngine.dll          # 动态库（Windows）
│   └── RenderEngine.lib          # 导入库（Windows）
├── lib/                           # 库文件目录
│   └── cmake/
│       └── RenderEngine/         # CMake配置文件
│           ├── RenderEngineConfig.cmake
│           ├── RenderEngineConfigVersion.cmake
│           └── RenderEngineTargets.cmake
├── include/                       # 头文件目录
│   ├── render/                   # RenderEngine头文件
│   │   ├── renderer.h
│   │   ├── shader.h
│   │   └── ...
│   ├── SDL3/                     # SDL3头文件
│   │   ├── SDL.h
│   │   └── ...
│   ├── glad/                     # GLAD头文件
│   │   ├── glad.h
│   │   └── ...
│   ├── KHR/                      # KHR平台头文件
│   │   └── khrplatform.h
│   ├── imgui.h                   # ImGui头文件
│   ├── imgui_internal.h
│   ├── backends/                 # ImGui后端头文件
│   │   ├── imgui_impl_sdl3.h
│   │   └── ...
│   └── json/                     # nlohmann/json头文件
│       └── nlohmann/
└── share/
    └── RenderEngine/
        └── shaders/               # Shader文件
            ├── sprite.vert
            ├── sprite.frag
            └── ...
```

## 依赖项说明

### 已包含的依赖

以下依赖已包含在RenderEngine的构建中，它们的头文件已提供：
- SDL3（库和头文件）
- SDL3_image
- SDL3_ttf
- Assimp
- meshoptimizer
- Bullet Physics
- Eigen3（仅头文件）
- GLAD（库和头文件）
- ImGui（库和头文件）
- nlohmann/json（仅头文件）

**重要说明**：
- 预编译库包含了SDL3、GLAD和ImGui的头文件，可以直接使用
- **在Windows上**，推荐使用动态库版本（`-DBUILD_SHARED_LIBS=ON`）以避免静态库链接问题
- 如果使用静态库版本并遇到SDL3链接错误（见Q8），建议改用动态库版本或从源码构建

**头文件使用**：
- SDL3头文件：`#include <SDL3/SDL.h>`
- GLAD头文件：`#include <glad/glad.h>`
- ImGui头文件：`#include "imgui.h"` 或 `#include <imgui.h>`
- Bullet Physics头文件：
  - `#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>`
  - `#include <BulletDynamics/Dynamics/btRigidBody.h>`
  - `#include <LinearMath/btVector3.h>`
  - `#include <LinearMath/btTransform.h>`
  - 等等

### 需要单独提供的依赖

以下依赖**需要**在你的项目中单独提供：
- **OpenGL 4.5+** - 通过系统驱动提供
- **C++20编译器** - MSVC 2019+, GCC 10+, Clang 12+
- **OpenMP** - 用于并行处理（批量操作优化）

### 链接依赖库

#### 链接OpenGL

在使用预编译库时，你仍需要链接OpenGL：

```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(your_target PRIVATE OpenGL::GL)
```

#### 链接OpenMP

RenderEngine使用OpenMP进行并行处理，提升批量操作的性能（如批量变换）。当使用**方法1（find_package）**时，CMake会自动处理OpenMP依赖。当使用**方法2（直接包含）**时，需要手动链接OpenMP：

```cmake
find_package(OpenMP REQUIRED)
target_link_libraries(your_target PRIVATE OpenMP::OpenMP_CXX)
```

**注意：** 如果使用 `find_package(RenderEngine REQUIRED)` 方式（方法1），OpenMP依赖会自动处理，无需手动添加。

#### 使用动态库版本的运行时要求

如果使用动态库版本（`-DBUILD_SHARED_LIBS=ON`构建），需要确保DLL文件在运行时可用：

**方法1（推荐）**：将DLL复制到可执行文件目录
```cmake
# 在CMakeLists.txt中
if(WIN32 AND TARGET RenderEngine::RenderEngine)
    get_target_property(RenderEngine_DLL RenderEngine::RenderEngine LOCATION)
    get_filename_component(RenderEngine_DLL_DIR ${RenderEngine_DLL} DIRECTORY)
    # 复制DLL到输出目录
    add_custom_command(TARGET your_target POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RenderEngine_DLL_DIR}/RenderEngine.dll"
        $<TARGET_FILE_DIR:your_target>
    )
endif()
```

**方法2**：将DLL目录添加到PATH环境变量

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

**说明**：
- 如果使用**静态库版本**，理论上不应该需要DLL。如果出现此错误：
  1. 检查是否有第三方库的DLL依赖
  2. 确保所有必要的运行时库都已安装（Visual C++ Redistributable）

- 如果使用**动态库版本**（`-DBUILD_SHARED_LIBS=ON`），需要确保`RenderEngine.dll`在运行时可用：
  1. 将`RenderEngine.dll`复制到可执行文件目录
  2. 或者将包含DLL的目录添加到PATH环境变量
  3. 参考[使用动态库版本的运行时要求](#使用动态库版本的运行时要求)部分

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

### Q6: 如何使用SDL3、GLAD和ImGui头文件

预编译库已包含SDL3、GLAD和ImGui的头文件，可以直接使用：

**使用SDL3**：
```cpp
#include <SDL3/SDL.h>
// 使用SDL3 API
```

**使用GLAD**：
```cpp
#include <glad/glad.h>
// 使用OpenGL API（通过GLAD加载）
```

**使用ImGui**：
```cpp
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"
// 使用ImGui API
```

**重要提示**：
- **不要**在自己的项目中编译 `imgui_impl_sdl3.cpp` 和 `imgui_impl_opengl3.cpp`
- 这些文件已经编译并静态链接到RenderEngine库中
- 只需要包含头文件即可使用ImGui后端功能
- 如果自己编译这些文件，会导致SDL3链接错误（见Q8）

**注意**：使用 `find_package(RenderEngine REQUIRED)` 方式时，这些头文件会自动包含在头文件搜索路径中。使用直接包含方式时，确保包含路径指向预编译库的 `include` 目录。

### Q7: ImGui DockSpaceOverViewport不可用

如果遇到 `ImGui::DockSpaceOverViewport` 不可用的错误，可能的原因：

1. **ImGui版本问题**：Docking功能需要ImGui 1.89+版本，且需要启用docking分支
2. **缺少docking头文件**：某些ImGui版本需要额外的docking扩展

**解决方案**：
- 检查ImGui版本，确保使用支持docking的版本
- 如果不需要docking功能，可以注释掉相关代码
- 或者使用传统的窗口布局方式替代docking

### Q8: SDL3链接错误（LNK2019）

如果遇到大量SDL3函数无法解析的链接错误（如 `SDL_CloseGamepad`, `SDL_GetKeyboardFocus` 等），这通常是因为**没有链接所有依赖的静态库**。

#### 问题原因

在Windows/MSVC上，静态库（.lib文件）是对象文件的归档，而不是合并后的代码：
- RenderEngine.lib包含了imgui_impl_sdl3.obj（已编译的ImGui后端代码）
- imgui_impl_sdl3.obj引用了SDL3函数
- 但SDL3的代码**没有**被合并到RenderEngine.lib中（因为SDL3是另一个静态库）
- 当只链接RenderEngine.lib时，链接器找不到SDL3的实现，导致链接错误

**注意**：这与"静态链接"的概念不同。SDL3的代码确实被包含在RenderEngine的构建中，但在Windows上，静态库之间的依赖需要在最终链接时解决。

#### 解决方案

**方案1（推荐）**：链接所有依赖库
- 预编译库的`lib`目录中包含了所有依赖的静态库文件
- 使用静态库版本时，需要链接所有依赖库（见[方法2](#方法2-直接包含简单项目)的示例代码）
- 所有需要的库文件都在预编译包的`lib`目录中：
  - `RenderEngine.lib`
  - `SDL3-static.lib`
  - `SDL3_image-static.lib`
  - `SDL3_ttf-static.lib`
  - `assimp-vc143-mt.lib`（或类似，文件名包含MSVC版本后缀，请检查lib目录中的实际文件名）
  - `meshoptimizer.lib`
  - `BulletDynamics.lib`, `BulletCollision.lib`, `LinearMath.lib`

**方案2**：使用动态库版本
- 在构建预编译库时，使用 `-DBUILD_SHARED_LIBS=ON` 选项构建动态库版本
- 动态库版本可以避免静态库链接问题，只需要链接一个导入库
- 使用方法：
  ```powershell
  cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
  cmake --build . --config Release
  ```
- 使用动态库时，需要在运行时提供DLL文件（通常在`bin`目录）

**方案3**：从源码构建（使用`add_subdirectory`方式）
- 使用[方法3](#方法3-作为子项目开发时)从源码构建
- 这样可以避免预编译库的链接问题，CMake会自动处理所有依赖

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
