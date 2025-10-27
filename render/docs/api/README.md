# API 参考文档

本目录包含 RenderEngine 的完整 API 参考文档。

---

## 核心模块

### 渲染核心
- **[Renderer](Renderer.md)** - 主渲染器类，提供高层渲染接口
- **[OpenGLContext](OpenGLContext.md)** - OpenGL 上下文管理
- **[RenderState](RenderState.md)** - 渲染状态管理（包含 OpenGL 状态封装：纹理/缓冲区/着色器程序管理）

### 着色器系统
- **[Shader](Shader.md)** - 着色器程序管理
- **[ShaderCache](ShaderCache.md)** - 着色器缓存系统
- **[UniformManager](UniformManager.md)** - Uniform 变量管理

### 纹理系统
- **[Texture](Texture.md)** - 纹理对象管理
- **[TextureLoader](TextureLoader.md)** - 纹理加载器和缓存管理

### 网格系统
- **[Mesh](Mesh.md)** - 网格对象管理（VAO/VBO/EBO）
- **[MeshLoader](MeshLoader.md)** - 几何形状生成器

### 工具类
- **[Logger](Logger.md)** - 日志系统
- **[FileUtils](FileUtils.md)** - 文件工具

### 类型定义
- **[Types](Types.md)** - 数学类型和基础类型定义

---

## 快速导航

### 初学者指南
1. [渲染器初始化](Renderer.md#初始化)
2. [创建着色器](Shader.md#从文件加载)
3. [使用着色器缓存](ShaderCache.md#基本使用)
4. [设置 Uniform](UniformManager.md#基本使用)

### 常用任务
- [窗口管理](Renderer.md#窗口管理)
- [渲染状态设置](RenderState.md#状态设置)
- [OpenGL 状态封装（纹理/VAO/着色器）](RenderState.md#opengl-状态封装)
- [着色器热重载](ShaderCache.md#热重载)
- [加载纹理](TextureLoader.md#基本使用)
- [日志记录](Logger.md#基本使用)

---

## 模块关系图

```
Renderer
  ├── OpenGLContext (窗口和 OpenGL 上下文)
  └── RenderState (渲染状态管理)

Shader
  └── UniformManager (Uniform 变量管理)

ShaderCache (单例)
  └── Shader[] (着色器集合)

TextureLoader (单例)
  └── Texture[] (纹理集合)

MeshLoader (静态工具类)
  └── 生成 Mesh 对象

Mesh (网格对象)
  ├── VAO/VBO/EBO (OpenGL 缓冲区)
  └── Vertex[] + Index[] (顶点和索引数据)

Logger (单例)
FileUtils (静态工具类)
```

---

## API 版本

- **Engine Version**: 1.1.0
- **API Version**: 1.1.0
- **OpenGL Version**: 4.5+
- **C++ Standard**: C++17
- **Last Updated**: 2025-10-27

### 版本更新记录
- **v1.1.0** (2025-10-27): 新增 OpenGL 状态封装（纹理/缓冲区/着色器程序管理）
- **v1.0.0** (2025-10-27): 初始版本，基础渲染系统

---

## 示例程序

完整示例代码请参考：
- [01_basic_window.cpp](../../examples/01_basic_window.cpp) - 基础窗口和渲染循环
- [02_shader_test.cpp](../../examples/02_shader_test.cpp) - 着色器系统使用
- [03_geometry_shader_test.cpp](../../examples/03_geometry_shader_test.cpp) - 几何着色器和缓存
- [04_state_management_test.cpp](../../examples/04_state_management_test.cpp) - **状态管理和 OpenGL 状态封装**
- [05_texture_test.cpp](../../examples/05_texture_test.cpp) - **纹理加载和渲染**
- [06_mesh_test.cpp](../../examples/06_mesh_test.cpp) - **网格系统和几何形状生成**

## 相关文档

- [开发指南](../DEVELOPMENT_GUIDE.md)
- [着色器缓存使用指南](../SHADER_CACHE_GUIDE.md)
- [纹理系统使用指南](../TEXTURE_SYSTEM.md)
- [架构文档](../ARCHITECTURE.md)
- [Phase 1 进度](../todolists/PHASE1_BASIC_RENDERING.md)
- [API 文档完成总结](../API_DOCUMENTATION_SUMMARY.md)

---

[返回文档首页](../README.md)

