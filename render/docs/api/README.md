# API 参考文档

本目录包含 RenderEngine 的完整 API 参考文档。

---

## 核心模块

### 渲染核心
- **[Renderer](Renderer.md)** - 主渲染器类，提供高层渲染接口
- **[OpenGLContext](OpenGLContext.md)** - OpenGL 上下文管理
- **[RenderState](RenderState.md)** - 渲染状态管理

### 着色器系统
- **[Shader](Shader.md)** - 着色器程序管理
- **[ShaderCache](ShaderCache.md)** - 着色器缓存系统
- **[UniformManager](UniformManager.md)** - Uniform 变量管理

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
- [着色器热重载](ShaderCache.md#热重载)
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

Logger (单例)
FileUtils (静态工具类)
```

---

## API 版本

- **Engine Version**: 1.0.0
- **OpenGL Version**: 4.5+
- **C++ Standard**: C++17
- **Last Updated**: 2025-10-27

---

## 相关文档

- [开发指南](../DEVELOPMENT_GUIDE.md)
- [着色器缓存使用指南](../SHADER_CACHE_GUIDE.md)
- [架构文档](../ARCHITECTURE.md)
- [Phase 1 进度](../todolists/PHASE1_BASIC_RENDERING.md)

---

[返回文档首页](../README.md)

