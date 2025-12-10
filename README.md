<p align="center">
  <h1 align="center">RenderEngine</h1>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/OpenGL-4.5%2B-green.svg" alt="OpenGL 4.5+">
  <img src="https://img.shields.io/badge/SDL-3.0-brightgreen.svg" alt="SDL3">
  <img src="https://img.shields.io/badge/CMake-3.20%2B-blueviolet.svg" alt="CMake">
  <img src="https://img.shields.io/badge/CI-GitHub%20Actions-blue.svg" alt="CI">
  <img src="https://img.shields.io/badge/OS-Windows-blue.svg" alt="Windows Only">
  <img src="https://img.shields.io/badge/License-AGPL--3.0-red.svg" alt="License">
</p>



一个基于现代 C++20 的 3D 渲染引擎，采用 ECS（Entity Component System）架构设计，支持 2D/3D 渲染、UI 系统、精灵动画、光照系统和资源异步加载等功能。

## 核心特性

### 渲染引擎核心
- ✅ **OpenGL 4.5+** 渲染后端，支持现代图形 API
- ✅ **SDL3** 窗口管理和输入处理
- ✅ **模块化架构** 清晰的分层设计，职责分离明确
- ✅ **着色器系统** 顶点/片段/几何着色器，支持热重载和缓存
- ✅ **纹理系统** PNG/JPG/BMP/TGA 支持，异步加载
- ✅ **网格系统** 10+ 种预设几何体，支持 Assimp 模型加载（OBJ/FBX/GLTF 等）
- ✅ **材质系统** Phong 光照模型，支持法线贴图、骨骼动画
- ✅ **资源管理器** 统一接口，智能引用计数，依赖管理
- ✅ **全面线程安全** 多线程环境下的安全设计

### ECS 架构系统
- ✅ **Entity Component System** 灵活的实体组件系统
- ✅ **组件系统** Transform、MeshRender、Sprite、Camera、Light、UI 等核心组件
- ✅ **系统架构** RenderSystem、AnimationSystem、TransformSystem、UISystem 等
- ✅ **场景管理** 场景切换、序列化、场景图支持

### 2D 渲染系统
- ✅ **精灵系统** Sprite、SpriteSheet、SpriteAtlas 支持
- ✅ **精灵动画** 状态机驱动的动画系统，支持动画事件
- ✅ **精灵批处理** 高效的大量精灵渲染
- ✅ **文本渲染** TTF 字体支持，文本渲染器

### UI 系统
- ✅ **UI 框架** 完整的 UI 控件系统（按钮、文本框、滑块、菜单等）
- ✅ **布局系统** Flex 和 Grid 布局，响应式设计
- ✅ **主题系统** 可配置的 UI 主题
- ✅ **菜单系统** UIMenu、UIPullDownMenu，参考 Blender UI 设计

### 3D 渲染与光照
- ✅ **光照系统** 定向光、点光源、聚光灯
- ✅ **法线贴图** 完整的法线贴图支持
- ✅ **骨骼动画** 骨骼调色板系统
- ✅ **后处理** 帧缓冲支持，后处理效果基础

### 性能优化系统
- ✅ **LOD 系统** 自动网格简化（meshoptimizer），距离相关的细节层次
- ✅ **实例化渲染** GPU 实例化与 LOD 系统集成
- ✅ **批处理系统** CPU 合批、GPU 实例化多种策略
- ✅ **材质排序** 减少 GPU 状态切换
- ✅ **视锥剔除** 高效的剔除系统
- ✅ **数学库优化** AVX2 SIMD、智能缓存、OpenMP 并行处理

## 技术栈

### 核心依赖
- **C++20** 现代 C++ 标准，使用概念、智能指针等特性
- **OpenGL 4.5+** 跨平台图形渲染 API
- **SDL3** 窗口管理、输入处理、跨平台支持
- **Eigen3** 高性能数学库（向量、矩阵、四元数）
- **GLAD** OpenGL 函数加载器

### 功能扩展库
- **Assimp** 3D 模型加载（OBJ、FBX、GLTF、Collada、MMD 等）
- **SDL3_image** 图像格式支持（PNG、JPG、BMP、TGA）
- **SDL3_ttf** 字体渲染和文本支持
- **meshoptimizer** 网格优化和 LOD 生成
- **nlohmann/json** JSON 序列化和配置

所有第三方库已包含在 `third_party/` 目录中，无需额外安装。

## 构建指南

### 前置要求
- **CMake 3.15+**
- **C++20 兼容的编译器**（MSVC 2019+、GCC 10+、Clang 12+）
- **OpenGL 4.5+** 驱动支持

### Windows

使用 Visual Studio 2022:

#### 方式一：使用自动化脚本（推荐）

使用自动化脚本一键设置所有依赖：

```powershell
# 运行依赖设置脚本（自动下载和配置所有第三方库）
PowerShell -ExecutionPolicy Bypass -File ".\setup-dependencies.ps1"

# 编译 Release 版本
mkdir build; cd build; cmake ..; cd ..
cmake --build build --config Release

# 运行示例
.\build\bin\Release\01_basic_window.exe
```

**脚本功能**：
- 自动克隆所有必需的第三方库
- 自动运行 Get-GitModules.ps1 脚本
- 自动复制 SDL3_ttf 所需的 cmake 文件
- 自动下载并解压 Eigen3
- 智能检测已存在的库，避免重复下载
- 支持选择性跳过某些库（使用 `-SkipEigen`、`-SkipSDL` 等参数）

#### 方式二：手动设置

如果需要手动控制每个步骤，可以按照以下步骤操作：

```batch
# 获取第三方库
cd third_party
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
cd SDL_image\external
PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
cd ..\..\SDL3_ttf-3.2.2\external
PowerShell -ExecutionPolicy Bypass -File ".\Get-GitModules.ps1"
cd ..\..
git clone https://github.com/nlohmann/json.git
git clone https://github.com/assimp/assimp.git
git clone https://github.com/zeux/meshoptimizer.git
git clone https://github.com/bulletphysics/bullet3.git

# 复制 SDL3_ttf 所需的 cmake 文件
copy "third_party\SDL\cmake\GetGitRevisionDescription.cmake" "third_party\SDL3_ttf-3.2.2\cmake\GetGitRevisionDescription.cmake"
copy "third_party\SDL\cmake\PkgConfigHelper.cmake" "third_party\SDL3_ttf-3.2.2\cmake\PkgConfigHelper.cmake"
copy "third_party\SDL\cmake\sdlcpu.cmake" "third_party\SDL3_ttf-3.2.2\cmake\sdlcpu.cmake"
copy "third_party\SDL\cmake\sdlplatform.cmake" "third_party\SDL3_ttf-3.2.2\cmake\sdlplatform.cmake"
copy "third_party\SDL\cmake\sdlmanpages.cmake" "third_party\SDL3_ttf-3.2.2\cmake\sdlmanpages.cmake"
copy "third_party\SDL_image\cmake\PrivateSdlFunctions.cmake" "third_party\SDL3_ttf-3.2.2\cmake\PrivateSdlFunctions.cmake"

# 获取eigen
wget "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip" -OutFile "eigen-3.4.0.zip"
Expand-Archive -Path "eigen-3.4.0.zip" -DestinationPath "."

# 编译 Release 版本
cd .. # 回到主目录
mkdir build; cd build; cmake ..; cd ..
cmake --build build --config Release

# 运行示例
.\build\bin\Release\01_basic_window.exe
```

**注意**: 如果 SDL3_ttf-3.2.2 目录不存在，请先手动下载并解压到 `third_party/` 目录。

### 构建选项
- `BUILD_EXAMPLES=ON` 构建示例程序（默认开启）
- `BUILD_TESTS=ON` 构建测试程序（默认开启）
- `ENABLE_OPENMP=ON` 启用 OpenMP 并行处理（默认开启，提升批量操作性能）

## 示例程序

项目包含 **62 个完整示例程序**，覆盖引擎的各个功能模块：

### 基础渲染示例
```batch
01_basic_window.exe              # 基础窗口创建
02_shader_test.exe               # 着色器系统测试
05_texture_test.exe              # 纹理加载和渲染
06_mesh_test.exe                 # 网格渲染
12_material_test.exe             # 材质系统测试
```

### ECS 系统示例
```batch
31_ecs_basic_test.exe            # ECS 基础功能
32_ecs_renderer_test.exe         # ECS 渲染系统
35_ecs_comprehensive_test.exe    # ECS 综合测试
```

### 2D 渲染示例
```batch
38_sprite_render_test.exe        # 精灵渲染
39_sprite_api_test.exe           # 精灵 API 使用
40_sprite_animation_test.exe     # 精灵动画系统
41_sprite_batch_test.exe         # 精灵批处理
42_sprite_state_machine_test.exe # 动画状态机
44_text_render_test.exe          # 文本渲染
```

### 3D 渲染示例
```batch
20_camera_test.exe               # 相机系统
45_lighting_test.exe             # 光照系统
46_normal_map_test.exe           # 法线贴图
47_skeleton_palette_test.exe     # 骨骼动画
48_model_render_test.exe         # 模型渲染
49_miku_model_test.exe           # MMD 模型加载示例
```

### UI 系统示例
```batch
60_ui_framework_showcase.exe     # UI 框架展示（布局系统、控件）
61_ui_menu_example.exe           # UI 菜单系统示例
```

### 性能优化示例
```batch
37_batching_benchmark.exe        # 批处理性能测试
58_lod_generator_test.exe        # LOD 生成器测试
59_lod_instanced_rendering_test.exe  # LOD 实例化渲染
```

### 应用框架示例
```batch
52_application_boot_demo.exe     # 应用框架启动示例
53_event_system_test.exe         # 事件系统
54_module_hud_test.exe           # 模块化 HUD 系统
55_scene_serialization_test.exe  # 场景序列化
```

### 工具链示例
```batch
57_toolchain_integration_test.exe  # 工具链集成测试
```

更多示例请查看 `examples/` 目录，每个示例都有详细注释说明使用方法。

## 项目架构

RenderEngine 采用清晰的分层架构设计：

### 架构层次
1. **硬件抽象层 (HAL)** - OpenGL 上下文管理、扩展检测、线程安全
2. **核心渲染层** - Renderer、RenderState、RenderLayer、批处理管理器
3. **资源管理层** - ResourceManager、异步加载、缓存系统
4. **ECS 架构层** - World、Entity、Component、System
5. **应用框架层** - SceneManager、ModuleRegistry、EventBus

### 设计模式
- **ECS 架构** - 灵活的实体组件系统，便于扩展
- **RAII 资源管理** - 智能指针和自动资源管理
- **模块化设计** - 插件式架构，易于添加新功能
- **观察者模式** - EventBus 实现发布-订阅机制

### 性能优化策略
- **LOD 系统** - 自动网格简化，距离相关的细节层次，支持批量计算
- **实例化渲染** - GPU 实例化与 LOD 系统集成
- **批处理优化** - CPU 合批、GPU 实例化等多种策略
- **材质排序** - 减少 GPU 状态切换
- **SIMD 优化** - AVX2 指令集，智能缓存机制
- **并行处理** - OpenMP 支持，批量操作性能提升 2-4x

## 文档

### 📚 核心文档
- [文档索引](docs/README.md) - 完整文档导航
- [项目结构分析](docs/PROJECT_STRUCTURE_ANALYSIS.md) - 详细的项目架构说明
- [项目结构图](docs/PROJECT_STRUCTURE_DIAGRAM.md) - 可视化架构图
- [架构设计](docs/ARCHITECTURE.md) - 架构设计文档
- [功能列表](docs/FEATURE_LIST.md) - 已实现和计划中的功能

### 📖 API 参考
- [核心 API](docs/api/README.md) - API 文档索引
- **渲染核心**: [Renderer](docs/api/Renderer.md)、[Shader](docs/api/Shader.md)、[Texture](docs/api/Texture.md)、[Mesh](docs/api/Mesh.md)、[Material](docs/api/Material.md)
- **ECS 系统**: [World](docs/api/World.md)、[Component](docs/api/Component.md)、[System](docs/api/System.md)、[Entity](docs/api/Entity.md)
- **资源管理**: [ResourceManager](docs/api/ResourceManager.md)、[AsyncResourceLoader](docs/api/AsyncResourceLoader.md)
- **数学工具**: [Transform](docs/api/Transform.md)、[MathUtils](docs/api/MathUtils.md)
- **LOD 系统**: [LOD](docs/api/LOD.md)、[LODGenerator](docs/api/LODGenerator.md)
- **UI 系统**: [UICanvas](docs/api/UICanvas.md)、[UIWidget](docs/api/UIWidget.md)、[UIButton](docs/api/UIButton.md) 等
- **2D 渲染**: [Sprite](docs/api/Sprite.md)、[SpriteAnimation](docs/api/SpriteAnimation.md)、[Text](docs/api/Text.md)

### 🎯 使用指南
- [ECS 快速开始](docs/ECS_QUICK_START.md) - ECS 系统使用指南
- [场景管理指南](docs/application/Scene_API.md) - 场景系统使用
- [模块开发指南](docs/application/Module_Guide.md) - 模块化开发
- [资源管理指南](docs/application/Resource_Management_Guide.md) - 资源管理最佳实践
- [UI 开发指南](docs/application/UI_DEVELOPMENT_PROGRESS_REPORT.md) - UI 系统开发

### 🔧 技术文档
- [材质排序架构](docs/MATERIAL_SORTING_ARCHITECTURE.md) - 渲染优化
- [JSON 序列化指南](docs/JSON_SERIALIZATION_GUIDE.md) - 场景序列化

## 项目特点

✅ **现代 C++20** - 充分利用新特性，类型安全且性能优异  
✅ **完整功能** - 2D/3D 渲染、UI、动画、光照等完整特性  
✅ **高性能** - 多种优化策略，适合实时渲染应用  
✅ **易于扩展** - 模块化架构，插件式设计  
✅ **文档完善** - 详细的 API 文档和使用指南  

## 许可证

本项目采用 [GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE) 许可证。

## 作者

李朝宇 (2052046346@qq.com)

