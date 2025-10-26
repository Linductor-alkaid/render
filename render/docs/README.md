# RenderEngine - 抽象渲染引擎

## 项目概述

RenderEngine 是一个基于 OpenGL 和 SDL3 构建的高性能抽象渲染引擎，旨在为游戏开发、电影特效摄影、实时可视化等应用提供完整的2D和3D渲染解决方案。

## 核心特性

### 2D 渲染
- 精灵渲染 (Sprite Rendering)
- 文本渲染 (Text Rendering)
- 粒子系统 (Particle System)
- UI渲染 (UI Rendering)
- 2D光照系统 (2D Lighting)
- 后处理效果 (Post-processing)

### 3D 渲染
- 模型加载与渲染 (Model Loading & Rendering)
- 场景图管理 (Scene Graph)
- 实时阴影 (Real-time Shadows)
- 延迟渲染管线 (Deferred Rendering Pipeline)
- HDR 渲染 (High Dynamic Range)
- PBR材质系统 (Physically Based Rendering)
- 全局光照 (Global Illumination)
- 体积渲染 (Volumetric Rendering)
- 屏幕空间反射 (Screen Space Reflections)

### 渲染管理
- 多线程渲染 (Multithreaded Rendering)
- 命令队列 (Command Queue)
- 资源管理 (Resource Management)
- 内存池管理 (Memory Pool)
- 渲染批处理 (Render Batching)

### ECS 集成
- 渲染层级管理 (Render Layer Management)
- 组件式渲染 (Component-based Rendering)
- 实体系统集成 (Entity System Integration)
- 渲染排序 (Render Sorting)

## 系统要求

- OpenGL 4.5+
- SDL3
- C++17 或更高版本
- 支持 GLSL 450+

## 文档索引

### 核心文档
- [架构设计](ARCHITECTURE.md) - 系统架构和技术设计
- [API 参考](API_REFERENCE.md) - 完整的API文档和使用示例
- [渲染层级管理](RENDERING_LAYERS.md) - 渲染层级系统和优先级管理
- [ECS 集成指南](ECS_INTEGRATION.md) - 与实体组件系统集成

### 开发文档
- [开发指南](DEVELOPMENT_GUIDE.md) - 开发环境配置和最佳实践
- [功能特性列表](FEATURE_LIST.md) - 已实现和计划中的功能
- [贡献指南](CONTRIBUTING.md) - 如何参与项目贡献

## 快速开始

```cpp
#include "render/renderer.h"

int main() {
    // 创建并初始化渲染器
    Renderer* renderer = Renderer::Create();
    renderer->Initialize();
    
    // 设置窗口
    renderer->SetWindowSize(1920, 1080);
    renderer->SetVSync(true);
    
    // 主循环
    while (isRunning) {
        renderer->BeginFrame();
        // ... 你的渲染代码 ...
        renderer->EndFrame();
        renderer->Present();
    }
    
    renderer->Shutdown();
    Renderer::Destroy(renderer);
    return 0;
}
```

## 主要功能模块

### 🎮 2D 渲染
完整的2D渲染功能，包括精灵、文本、粒子、UI等。

### 🎬 3D 渲染
强大的3D渲染能力，支持模型、材质、光照、阴影等。

### 🎨 渲染层级
灵活的层级管理系统，支持按优先级渲染和条件渲染。

### 🔗 ECS 集成
与ECS架构无缝集成，支持组件式渲染开发。

### ⚡ 性能优化
内置批处理、剔除、排序等性能优化机制。

## 技术优势

- **高度抽象**: 简洁的API，易于使用
- **高性能**: 优化的渲染管线，支持大规模场景
- **模块化**: 清晰的架构，易于扩展
- **跨平台**: 支持多种平台（扩展中）
- **生产就绪**: 适用于游戏、影视、可视化等应用

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件。

## 联系方式

- 项目仓库: https://github.com/yourusername/render
- 问题反馈: https://github.com/yourusername/render/issues
- 讨论区: https://github.com/yourusername/render/discussions

