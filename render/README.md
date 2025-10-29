# RenderEngine

一个基于 OpenGL 和 SDL3 构建的现代渲染引擎。

## 特性

### 渲染核心
- ✅ OpenGL 4.5+ 渲染后端
- ✅ SDL3 窗口管理
- ✅ 模块化架构设计
- ✅ 着色器系统（顶点/片段/几何着色器 + 缓存）
- ✅ 纹理系统（PNG/JPG/BMP/TGA + 异步加载）
- ✅ 网格系统（10种几何形状 + 外部模型加载）
- ✅ 材质系统（Phong 光照）
- ✅ 资源管理器（统一管理 + 引用计数）
- 🔒 全面线程安全设计

### 数学库 ⚡ **新增**
- ✅ **Transform** - 3D变换类（位置、旋转、缩放、父子关系）
- ✅ **MathUtils** - 数学工具函数（角度转换、向量/四元数/矩阵工具）
- ✅ **几何类型** - Plane、Ray、AABB
- ⚡ **性能优化** - SIMD（AVX2）+ 智能缓存 + 并行处理
- ⚡ **高性能** - 世界变换缓存提升 10-50x

### 性能特性
- ⚡ AVX2 SIMD 指令集（256位向量运算）
- ⚡ 智能缓存机制（Transform 查询 3.5ns/次）
- ⚡ OpenMP 并行处理（批量操作提升 2-4x）
- ⚡ 算法优化（FromEuler 快 40-50%）

## 构建

### Windows

使用 Visual Studio 2022:

```batch
# 配置项目
build.bat

# 编译
cmake --build build --config Release
```

### Linux/macOS

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 依赖项

- **SDL3**: 窗口管理和输入处理
- **Eigen3**: 数学库
- **GLAD**: OpenGL 加载库
- **OpenGL 4.5+**: 渲染 API

所有第三方库已包含在 `third_party/` 目录中。

## 示例

项目包含 19 个完整示例程序：

```batch
# 基础渲染
.\build\bin\Release\01_basic_window.exe
.\build\bin\Release\05_texture_test.exe
.\build\bin\Release\06_mesh_test.exe

# 数学库测试 ⚡ 新增
.\build\bin\Release\18_math_test.exe
.\build\bin\Release\19_math_benchmark.exe

# 资源管理
.\build\bin\Release\15_resource_manager_test.exe
.\build\bin\Release\17_model_with_resource_manager_test.exe
```

## 文档

### 📚 核心文档
- [完整文档索引](docs/README.md)
- [API 参考文档](docs/api/README.md)
- [开发指南](docs/DEVELOPMENT_GUIDE.md)
- [架构设计](docs/ARCHITECTURE.md)

### 📖 API 文档
- [Renderer](docs/api/Renderer.md) - 主渲染器
- [Shader](docs/api/Shader.md) - 着色器系统
- [Texture](docs/api/Texture.md) - 纹理系统
- [Mesh](docs/api/Mesh.md) - 网格系统
- [Material](docs/api/Material.md) - 材质系统
- [ResourceManager](docs/api/ResourceManager.md) - 资源管理
- [Transform](docs/api/Transform.md) - 3D变换 ⚡ **新增**
- [MathUtils](docs/api/MathUtils.md) - 数学工具 ⚡ **新增**

### 🎯 项目管理
- [Phase 1 进度](docs/todolists/PHASE1_BASIC_RENDERING.md)
- [功能列表](docs/FEATURE_LIST.md)

### ⚡ 性能优化
- [数学库性能优化报告](docs/MATH_FINAL_OPTIMIZATION_REPORT.md) **新增**

## 许可证

MIT License

## 作者

RenderEngine Team

