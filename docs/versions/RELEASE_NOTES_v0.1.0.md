# RenderEngine v0.1.0 发行说明

**发布日期：** 2026年1月11日  
**版本：** 0.1.0  
**构建：** Release-x64

---

## 🎉 欢迎使用 RenderEngine v0.1.0！

RenderEngine v0.1.0 是我们基于现代 C++20 的 3D 渲染引擎的首个稳定版本。本版本提供了完整的渲染解决方案，采用 ECS 架构，支持 2D/3D 渲染、完整的 UI 系统以及先进的性能优化。

## ✨ v0.1.0 新特性

### 渲染引擎核心
- **OpenGL 4.5+** 渲染后端，支持现代图形 API
- **SDL3** 窗口管理和跨平台输入处理
- **模块化架构** 清晰的分层设计，职责分离明确
- **着色器系统** 顶点/片段/几何着色器支持，热重载和缓存
- **纹理系统** 支持 PNG/JPG/BMP/TGA，异步加载
- **网格系统** 10+ 种预设几何体，Assimp 模型加载（OBJ/FBX/GLTF/Collada/MMD）
- **材质系统** Phong 光照模型，支持法线贴图、骨骼动画
- **资源管理器** 统一接口，智能引用计数，依赖管理
- **全面线程安全** 多线程环境下的安全设计

### ECS 架构系统
- **Entity Component System** - 灵活的实体组件系统，适用于游戏/应用开发
- **组件系统** - 核心组件：Transform、MeshRender、Sprite、Camera、Light、UI、Physics
- **系统架构** - RenderSystem、AnimationSystem、TransformSystem、UISystem、PhysicsSystem
- **场景管理** - 场景切换、序列化、场景图支持
- **物理集成** - Bullet Physics 集成，支持 URDF 机器人仿真

### 2D 渲染系统
- **精灵系统** - Sprite、SpriteSheet、SpriteAtlas 支持
- **精灵动画** - 状态机驱动的动画系统，支持动画事件
- **精灵批处理** - 高效的大量精灵渲染
- **文本渲染** - TTF 字体支持，文本渲染器

### UI 系统
- **UI 框架** - 完整的 UI 控件系统（按钮、文本框、滑块、复选框、菜单等）
- **布局系统** - Flex 和 Grid 布局，响应式设计
- **主题系统** - 可配置的 UI 主题（支持亮色/暗色模式）
- **菜单系统** - UIMenu、UIPullDownMenu，参考 Blender UI 设计
- **ImGui 集成** - 可选的 ImGui 后端，用于即时模式 GUI

### 3D 渲染与光照
- **光照系统** - 定向光、点光源、聚光灯
- **法线贴图** - 完整的法线贴图支持，实现真实的表面细节
- **骨骼动画** - 骨骼调色板系统，支持角色动画
- **后处理** - 帧缓冲支持，后处理效果基础

### 性能优化系统
- **LOD 系统** - 自动网格简化（meshoptimizer），基于距离的细节层次
- **实例化渲染** - GPU 实例化与 LOD 系统集成
- **批处理系统** - 多种策略：CPU 合批和 GPU 实例化
- **材质排序** - 减少 GPU 状态切换，提升性能
- **视锥剔除** - 高效的剔除系统
- **数学库优化** - AVX2 SIMD，智能缓存，OpenMP 并行处理（性能提升 2-4 倍）

### 应用框架
- **Application Host** - 模块化应用框架，插件式架构
- **事件系统** - EventBus 发布-订阅通信机制
- **模块系统** - CoreRenderModule、InputModule、UIRuntimeModule、DebugHUDModule
- **场景序列化** - 基于 JSON 的场景序列化和反序列化
- **工具链集成** - 材质着色器面板、层遮罩编辑器、场景图可视化器

### 机器人仿真支持
- **URDF 加载器** - 加载和可视化 URDF 机器人模型
- **关节变换系统** - 支持正向/逆向运动学
- **TF 可视化器** - 变换坐标系可视化
- **IMU 接口** - IMU 传感器集成接口

## 📦 包内容

### 静态库版本（默认）

此预编译库包包含：

```
RenderEngine-prebuilt-Release-x64-Static/
├── lib/                           # 静态库文件目录
│   ├── RenderEngine.lib          # RenderEngine主库（Windows）
│   ├── SDL3-static.lib           # SDL3静态库（依赖，需一起链接）
│   ├── SDL3_image-static.lib     # SDL3_image静态库（依赖，需一起链接）
│   ├── SDL3_ttf-static.lib       # SDL3_ttf静态库（依赖，需一起链接）
│   ├── assimp-vc143-mt.lib      # Assimp模型加载库（依赖，文件名包含MSVC版本后缀）
│   ├── meshoptimizer.lib         # 网格优化库（依赖，需一起链接）
│   ├── BulletDynamics.lib        # Bullet物理引擎（依赖，需一起链接）
│   ├── BulletCollision.lib       # Bullet碰撞检测（依赖，需一起链接）
│   ├── LinearMath.lib            # Bullet数学库（依赖，需一起链接）
│   └── cmake/                    # CMake 配置文件
│       └── RenderEngine/
│           ├── RenderEngineConfig.cmake
│           ├── RenderEngineConfigVersion.cmake
│           └── RenderEngineTargets.cmake
├── include/                       # 头文件目录
│   ├── render/                   # RenderEngine头文件
│   ├── SDL3/                     # SDL3头文件（可直接使用）
│   ├── glad/                     # GLAD头文件（可直接使用）
│   ├── KHR/                      # KHR平台头文件
│   ├── imgui.h                   # ImGui头文件（可直接使用）
│   ├── backends/                 # ImGui后端头文件
│   ├── BulletCollision/          # Bullet Physics头文件（可直接使用）
│   ├── BulletDynamics/           # Bullet Physics头文件（可直接使用）
│   ├── LinearMath/               # Bullet Physics头文件（可直接使用）
│   └── json/                     # nlohmann/json头文件
└── share/
    └── RenderEngine/
        └── shaders/               # 着色器文件
```

**重要说明：**
- 静态库版本包含所有依赖的静态库文件，使用时需要**链接所有库文件**（见下方使用示例）
- 在Windows上，推荐使用**动态库版本**（见下方）以避免静态库链接问题

### 动态库版本（推荐用于Windows）

动态库版本可以避免静态库链接问题，使用方法更简单：

```
RenderEngine-prebuilt-Release-x64-Shared/
├── bin/                           # 运行时文件目录
│   └── RenderEngine.dll          # 动态库（Windows）
├── lib/                           # 库文件目录
│   ├── RenderEngine.lib           # 导入库（Windows）
│   └── cmake/                    # CMake 配置文件
│       └── RenderEngine/
├── include/                       # 头文件目录（同静态库版本）
└── share/
    └── RenderEngine/
        └── shaders/               # 着色器文件
```

**动态库版本优势：**
- 只需链接一个导入库（`RenderEngine.lib`）
- 避免Windows静态库链接问题
- 更符合Windows开发习惯

**构建动态库版本：**
```powershell
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build . --config Release
```

## 🔧 系统要求

### 最低要求
- **操作系统：** Windows 10/11 (x64)
- **编译器：** MSVC 2019+ / GCC 10+ / Clang 12+，支持 C++20
- **CMake：** 3.15 或更高版本
- **OpenGL：** 4.5 或更高版本（需要驱动支持）
- **OpenMP：** 用于并行处理（必需依赖）
- **内存：** 最低 4GB，推荐 8GB

### 已包含的依赖

**静态链接的库：**
- SDL3（窗口管理）
- SDL3_image（图像加载）
- SDL3_ttf（字体渲染）
- Assimp（3D 模型加载）
- meshoptimizer（网格优化）
- Bullet Physics（物理仿真）

**仅头文件库：**
- Eigen3（数学库）
- nlohmann/json（JSON 解析）
- GLAD（OpenGL 加载器）
- ImGui（UI 后端）

**头文件可直接使用：**
预编译包已包含 SDL3、GLAD、ImGui 和 Bullet Physics 的头文件，可以直接使用：
- SDL3：`#include <SDL3/SDL.h>`
- GLAD：`#include <glad/glad.h>`
- ImGui：`#include "imgui.h"`
- Bullet Physics：`#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>`、`#include <BulletDynamics/Dynamics/btRigidBody.h>`、`#include <LinearMath/btVector3.h>` 等

**需要单独提供的依赖：**
- **OpenGL 4.5+** - 通过系统驱动提供
- **OpenMP** - 用于并行处理（必需）

## 📥 安装与使用

### 方法1：使用 CMake find_package（推荐）

这是最推荐的方式，因为它会自动处理依赖关系和路径。

```cmake
cmake_minimum_required(VERSION 3.15)
project(YourProject)

set(CMAKE_CXX_STANDARD 20)

# 设置 RenderEngine 路径
set(RenderEngine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine-prebuilt-Release-x64-Static/lib/cmake/RenderEngine")

# 查找 RenderEngine
find_package(RenderEngine REQUIRED)

# 创建你的可执行文件
add_executable(your_app main.cpp)

# 链接 RenderEngine（会自动处理所有依赖）
target_link_libraries(your_app PRIVATE RenderEngine::RenderEngine)

# 链接 OpenGL（必需）
find_package(OpenGL REQUIRED)
target_link_libraries(your_app PRIVATE OpenGL::GL)

# 链接 OpenMP（必需）
find_package(OpenMP REQUIRED)
target_link_libraries(your_app PRIVATE OpenMP::OpenMP_CXX)
```

### 方法2：直接包含（静态库版本）

对于静态库版本，需要链接所有依赖库：

```cmake
cmake_minimum_required(VERSION 3.15)
project(YourProject)

set(CMAKE_CXX_STANDARD 20)

# 设置预编译库路径
set(RENDER_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine-prebuilt-Release-x64-Static")

# 添加头文件路径
target_include_directories(your_app PRIVATE 
    "${RENDER_ENGINE_ROOT}/include"
)

# 链接所有静态库（静态库版本需要链接所有依赖）
target_link_libraries(your_app PRIVATE
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

# 链接 OpenGL（必需）
find_package(OpenGL REQUIRED)
target_link_libraries(your_app PRIVATE OpenGL::GL)

# 链接 OpenMP（必需）
find_package(OpenMP REQUIRED)
target_link_libraries(your_app PRIVATE OpenMP::OpenMP_CXX)
```

### 方法3：直接包含（动态库版本，推荐）

动态库版本使用更简单：

```cmake
cmake_minimum_required(VERSION 3.15)
project(YourProject)

set(CMAKE_CXX_STANDARD 20)

# 设置预编译库路径
set(RENDER_ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/path/to/RenderEngine-prebuilt-Release-x64-Shared")

# 添加头文件路径
target_include_directories(your_app PRIVATE 
    "${RENDER_ENGINE_ROOT}/include"
)

# 链接导入库（动态库版本只需链接一个库）
target_link_libraries(your_app PRIVATE
    "${RENDER_ENGINE_ROOT}/lib/RenderEngine.lib"
)

# 复制DLL到输出目录
add_custom_command(TARGET your_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${RENDER_ENGINE_ROOT}/bin/RenderEngine.dll"
    $<TARGET_FILE_DIR:your_app>
)

# 链接 OpenGL（必需）
find_package(OpenGL REQUIRED)
target_link_libraries(your_app PRIVATE OpenGL::GL)

# 链接 OpenMP（必需）
find_package(OpenMP REQUIRED)
target_link_libraries(your_app PRIVATE OpenMP::OpenMP_CXX)
```

### 构建你的项目

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 示例代码

```cpp
#include <render/renderer.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>

int main() {
    // 初始化 RenderEngine
    Render::Renderer renderer;
    if (!renderer.Initialize()) {
        return -1;
    }
    
    // 创建 ECS 世界
    Render::World world;
    
    // 创建带组件的实体
    auto entity = world.CreateEntity();
    world.AddComponent<Render::TransformComponent>(entity);
    world.AddComponent<Render::MeshRenderComponent>(entity);
    
    // 主循环
    while (renderer.IsRunning()) {
        renderer.BeginFrame();
        // 你的渲染代码
        renderer.EndFrame();
    }
    
    return 0;
}
```

更多示例请查看源代码仓库的 `examples/` 目录。

## 📚 文档

- **API 文档：** 参见 `docs/api/` 目录
- **快速开始指南：** `docs/ECS_QUICK_START.md`
- **预编译库使用指南：** `docs/PREBUILT_LIBRARY_USAGE.md`（包含详细的使用说明和常见问题）
- **完整文档：** `docs/README.md`

## 🎯 核心特性亮点

### 性能
- **AVX2 SIMD 优化** - 向量化数学运算，性能提升 2-4 倍
- **OpenMP 并行处理** - 多线程批量操作
- **LOD 系统** - 自动网格简化，基于距离的渲染
- **GPU 实例化** - 高效渲染多个相同对象
- **材质排序** - 最小化 GPU 状态切换

### 开发体验
- **现代 C++20** - 充分利用概念、智能指针等现代特性
- **ECS 架构** - 灵活可扩展的实体组件系统
- **线程安全设计** - 多线程环境下的安全设计
- **完善的 API** - 详细文档，包含 63+ 个示例程序
- **热重载** - 着色器热重载支持，加快迭代速度

### 渲染能力
- **2D & 3D 渲染** - 统一的渲染管线，支持 2D 和 3D 内容
- **高级光照** - 多种光源类型，Phong 着色
- **法线贴图** - 真实的表面细节
- **骨骼动画** - 角色动画支持
- **后处理** - 帧缓冲支持，用于特效

## ⚠️ 已知限制与注意事项

### 平台支持
- **平台：** 目前仅支持 Windows (x64)
- **仅 OpenGL：** 使用 OpenGL 4.5+，暂无 Vulkan/Metal 支持

### 静态库版本注意事项
- **Windows静态库链接：** 在Windows上，静态库版本需要链接所有依赖库（见使用方法）
- **推荐使用动态库：** 在Windows上，推荐使用动态库版本（`-DBUILD_SHARED_LIBS=ON`）以避免静态库链接问题
- **二进制文件较大：** 所有依赖均为静态链接，二进制文件较大

### 其他限制
- **无音频：** 本版本不包含音频系统
- **OpenMP必需：** 需要系统支持OpenMP，用于并行处理

## 🔄 从源码构建迁移

如果你之前从源码构建，使用预编译库将：
- **缩短构建时间** 从约 10-30 分钟缩短到几秒钟
- **简化依赖管理** - 所有依赖已包含
- **确保构建一致性** - 所有项目使用相同的库

只需将 `add_subdirectory(RenderEngine)` 替换为 `find_package(RenderEngine REQUIRED)`。

## 🐛 问题报告与支持

如遇到问题：
1. 查看[预编译库使用指南](docs/PREBUILT_LIBRARY_USAGE.md)中的常见问题部分
2. 查看[完整文档](docs/README.md)
3. 在项目仓库中提交 issue

**常见问题：**
- **SDL3链接错误（LNK2019）**：这是Windows静态库链接的正常行为。解决方案：使用动态库版本，或链接所有依赖库（见文档Q8）
- **找不到DLL**：如果使用动态库版本，确保DLL文件在运行时可用（见文档Q3）
- **OpenMP链接错误**：确保安装了OpenMP并正确链接（见文档）

## 📄 许可证

本项目采用 **GNU Affero General Public License v3.0 (AGPL-3.0)** 许可证。

详情请参见 [LICENSE](LICENSE) 文件。

## 🙏 致谢

RenderEngine v0.1.0 构建在优秀的开源库之上：
- SDL3, SDL3_image, SDL3_ttf
- Assimp
- Bullet Physics
- Eigen3
- meshoptimizer
- nlohmann/json
- ImGui
- GLAD

特别感谢所有开源社区。

## 📈 未来计划

未来版本可能包括：
- Linux 和 macOS 支持
- 音频系统
- 更多后处理效果
- Vulkan 后端支持

---

**下载：** 
- [RenderEngine-prebuilt-Release-x64-Static.zip](RenderEngine-prebuilt-Release-x64-Static.zip)（静态库版本）
- [RenderEngine-prebuilt-Release-x64-Shared.zip](RenderEngine-prebuilt-Release-x64-Shared.zip)（动态库版本，推荐）

**文档：** 参见 `docs/` 目录  
**示例：** 参见源代码仓库的 `examples/` 目录

**祝使用愉快！🚀**
