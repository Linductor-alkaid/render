# RenderEngine 文档索引

## 导航

### 📚 文档结构

```
docs/
├── README.md                    - 项目概述和快速开始
├── INDEX.md                     - 文档索引（本文件）
├── ARCHITECTURE.md              - 系统架构设计
├── API_REFERENCE.md             - API 完整参考
├── RENDERING_LAYERS.md          - 渲染层级管理
├── ECS_INTEGRATION.md           - ECS 集成指南
├── DEVELOPMENT_GUIDE.md         - 开发指南
├── FEATURE_LIST.md              - 功能特性列表
├── CONTRIBUTING.md              - 贡献指南
├── THREAD_SAFETY.md             - RenderState 线程安全
└── RENDERER_THREAD_SAFETY.md    - Renderer 线程安全（新）
```

## 推荐阅读顺序

### 新手入门
1. [README.md](README.md) - 了解项目概况
2. [ARCHITECTURE.md](ARCHITECTURE.md) - 理解系统架构
3. [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md) - 设置开发环境
4. [API_REFERENCE.md](API_REFERENCE.md) - 学习如何使用

### 进阶开发
5. [RENDERING_LAYERS.md](RENDERING_LAYERS.md) - 掌握层级系统
6. [ECS_INTEGRATION.md](ECS_INTEGRATION.md) - 集成到ECS项目
7. [FEATURE_LIST.md](FEATURE_LIST.md) - 查看完整功能

### 参与贡献
8. [CONTRIBUTING.md](CONTRIBUTING.md) - 了解贡献流程

## 快速链接

### 核心概念
- [渲染器架构](ARCHITECTURE.md#核心架构层次)
- [层级管理](RENDERING_LAYERS.md#概述)
- [ECS组件](ECS_INTEGRATION.md#组件定义)

### 常用API
- [初始化渲染器](API_REFERENCE.md#renderer-初始化)
- [3D网格渲染](API_REFERENCE.md#网格渲染)
- [2D精灵渲染](API_REFERENCE.md#精灵渲染)
- [相机控制](API_REFERENCE.md#相机控制)
- [光照阶段设计](guides/LightingSystem_PhaseA.md)
- [光照系统](API_REFERENCE.md#光照系统)

### 开发资源
- [环境配置](DEVELOPMENT_GUIDE.md#环境配置)
- [项目结构](DEVELOPMENT_GUIDE.md#项目结构)
- [性能优化](DEVELOPMENT_GUIDE.md#性能优化技巧)
- [调试工具](DEVELOPMENT_GUIDE.md#调试工具)

### 线程安全
- [Renderer 线程安全](RENDERER_THREAD_SAFETY.md) - Renderer 类多线程使用指南
- [RenderState 线程安全](THREAD_SAFETY.md) - RenderState 类线程安全实现
- [网格系统线程安全](MESH_THREAD_SAFETY.md) - Mesh 和 MeshLoader 线程安全指南

## 文档维护

文档采用 Markdown 格式编写，遵循以下规范：

- 每个文档开头都有返回链接
- 文档之间相互链接，便于导航
- 代码示例统一使用 C++ 语言
- 关键概念提供示例代码

## 反馈与更新

如发现文档问题或需要补充内容，请：
1. 提交 Issue 描述问题
2. 或直接提交 Pull Request 更新文档

---

[返回文档首页](README.md)

