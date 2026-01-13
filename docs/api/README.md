# API 参考文档

本目录包含 RenderEngine 的完整 API 参考文档。

---

## 核心模块

### 渲染核心
- **[Renderer](Renderer.md)** - 主渲染器类，提供高层渲染接口 🔒 **线程安全**
- **[OpenGLContext](OpenGLContext.md)** - OpenGL 上下文管理
- **[RenderState](RenderState.md)** - 渲染状态管理（包含 OpenGL 状态封装：纹理/缓冲区/着色器程序管理） 🔒 **线程安全**
- **[RenderBatchingPlan](RenderBatchingPlan.md)** - 渲染批处理方案规划与阶段落地指南
- **[RenderBatching](RenderBatching.md)** - 渲染批处理实现与使用指南（模式、流程、调试）
- **[Lighting](Lighting.md)** - 多光源管理、光照 uniform 与着色器对接
- **[GLThreadChecker](GLThreadChecker.md)** - OpenGL 线程安全检查器 🔒 **线程安全** 
  - 确保所有 OpenGL 调用在正确的线程中执行
  - 自动检测和报告线程错误
  - 提供详细的错误信息（文件、行号、线程 ID）
  - 编译时可禁用以提高性能

### 相机系统
- **[Camera](Camera.md)** - 相机类（透视/正交投影、视图变换、视锥体裁剪） 🔒 **线程安全** 
  - 支持透视投影和正交投影
  - 第一人称、轨道、第三人称相机控制器
  - 屏幕↔世界坐标转换
  - 视锥体裁剪优化

### 着色器系统
- **[Shader](Shader.md)** - 着色器程序管理
- **[ShaderCache](ShaderCache.md)** - 着色器缓存系统 🔒 **线程安全**
- **[UniformManager](UniformManager.md)** - Uniform 变量管理

### 纹理系统
- **[Texture](Texture.md)** - 纹理对象管理 🔒 **线程安全**
- **[TextureLoader](TextureLoader.md)** - 纹理加载器和缓存管理 🔒 **线程安全**
- **[Framebuffer](Framebuffer.md)** - 帧缓冲对象管理（离屏渲染、后处理、MSAA） 🔒 **线程安全**

### 网格系统
- **[Mesh](Mesh.md)** - 网格对象管理（VAO/VBO/EBO） 🔒 **线程安全**
- **[MeshLoader](MeshLoader.md)** - 几何形状生成器 🔒 **线程安全**
- **[GeometryPreset](GeometryPreset.md)** - 预设几何体注册与复用

### LOD 系统
- **[LOD](LOD.md)** - LOD（Level of Detail）系统，基于距离的细节级别管理
  - 支持网格、模型、材质和纹理的 LOD 配置
  - 与 ECS 系统无缝集成
  - 批量 LOD 计算和平滑过渡
  - 提供统计信息和调试工具
- **[LODGenerator](LODGenerator.md)** - LOD 网格生成器，自动生成不同 LOD 级别的网格
  - 使用 meshoptimizer 库进行网格简化
  - 支持单个网格、整个模型以及批量处理
  - 提供文件保存和加载功能
  - 支持自动配置 LODConfig

### 模型系统
- **[Model](Model.md)** - 组合模型、部件管理及统计功能 🔒 **线程安全**
- **[ModelRenderer](ModelRenderer.md)** - 模型渲染对象与排序/裁剪流程
- **[ModelLoader](ModelLoader.md)** - 模型导入与资源注册（集成 ResourceManager / AsyncResourceLoader）
- **[Skeleton](Skeleton.md)** - 骨骼层级封装与蒙皮调色板辅助计算

### 材质系统
- **[Material](Material.md)** - 材质管理（属性、纹理、着色器、渲染状态） 🔒 **线程安全**

### 资源管理
- **[ResourceManager](ResourceManager.md)** - 统一资源管理器（纹理、网格、材质、着色器） 🔒 **线程安全**
- **[AsyncResourceLoader](AsyncResourceLoader.md)** - 异步资源加载器 🔒 **线程安全**

### ECS 系统 🆕 **v1.1 已更新**
- **[ECS](ECS.md)** - ECS 系统总览（Entity Component System）🆕 **v1.1 新增安全接口**
- **[Entity](Entity.md)** - 实体和实体管理器（轻量级 ID、版本号、标签系统）🆕 **v1.1 性能优化**
- **[Component](Component.md)** - 组件和组件注册表（Transform、MeshRender、Camera、Light 等）🆕 **v1.1 安全接口**
- **[System](System.md)** - 系统基类和内置系统（Camera、Transform、ResourceLoading、MeshRender、Light 等）
- **[World](World.md)** - ECS 世界容器（统一的实体、组件、系统管理）

**v1.1 重要改进**：
- ✅ EntityManager 递归锁优化，性能提升 5-10%
- ✅ ComponentRegistry 新增安全迭代接口（ForEachComponent 等）
- ✅ 完全向后兼容，旧代码无需修改
- 📖 详见：[ECS 安全性改进报告](../ECS_SAFETY_IMPROVEMENTS.md)

### Renderable 渲染对象
- **[Renderable](Renderable.md)** - 渲染对象基类（统一的渲染接口）
- **[MeshRenderable](MeshRenderable.md)** - 3D 网格渲染对象
- **[SpriteRenderable](SpriteRenderable.md)** - 2D 精灵渲染对象
- **[Sprite](Sprite.md)** - 精灵数据对象（纹理、帧、颜色）
- **[SpriteSheet](SpriteSheet.md)** - 图集帧管理
- **[SpriteAnimator](SpriteAnimator.md)** - 精灵动画播放控制
- **[SpriteAnimation](SpriteAnimation.md)** - ECS 动画组件与系统事件
- **[SpriteAnimationScriptRegistry](SpriteAnimationScriptRegistry.md)** - 动画脚本注册表
- **[SpriteAnimationDebugger](SpriteAnimationDebugger.md)** - 动画调试器与日志面板（Debug 构建）
- **[SpriteBatch](SpriteBatch.md)** - 渲染批处理框架概述
- **[SpriteRenderer](SpriteRenderer.md)** - 即时模式 2D 渲染器
- **[SpriteAtlas](SpriteAtlas.md)** - 带元数据的精灵图集
- **[SpriteAtlasImporter](SpriteAtlasImporter.md)** - 精灵图集 JSON 导入器
- **[SpriteBatcher](SpriteBatcher.md)** - 精灵实例批处理器
- **[SpriteRenderLayer](SpriteRenderLayer.md)** - 精灵渲染层级管理器
- **[TextRenderable](TextRenderable.md)** - 文本渲染对象
- **[Text](Text.md)** - 文本数据对象（字符串、颜色、换行）
- **[Font](Font.md)** - SDL_ttf 字体封装与光栅化
- **[TextRenderer](TextRenderer.md)** - 即时模式文本渲染器

### UI 系统 🎨 **v1.0 菜单系统**
- **[UIWidget](UIWidget.md)** - UI控件基类（布局、事件、状态管理）
- **[UICanvas](UICanvas.md)** - UI画布（缩放、DPI适配、状态管理）
- **[UITheme](UITheme.md)** - UI主题系统（颜色、字体、尺寸配置）

**基础控件**:
- **[UIButton](UIButton.md)** - 按钮控件
- **[UITextField](UITextField.md)** - 文本输入框
- **[UICheckBox](UICheckBox.md)** - 复选框控件
- **[UIRadioButton](UIRadioButton.md)** - 单选按钮
- **[UIToggle](UIToggle.md)** - 开关控件
- **[UISlider](UISlider.md)** - 滑块控件
- **[UIColorPicker](UIColorPicker.md)** - 颜色选择器

**菜单系统** 🆕 **v1.0**:
- **[UIMenuItem](UIMenuItem.md)** - 菜单项（普通、可选中、分隔符、子菜单）
- **[UIMenu](UIMenu.md)** - 菜单容器（滚动、键盘导航）
- **[UIPullDownMenu](UIPullDownMenu.md)** - 下拉菜单（定位、触发）

**UI 文档**:
- [UI框架基础计划](../guides/UI_FRAMEWORK_FOUNDATION_PLAN.md) - UI框架设计
- [UI菜单系统文档](../ui/UI_MENU_SYSTEM.md) - 菜单系统详细文档
- [UI Blender参考计划](../application/UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md) - UI系统整体规划
- [UI颜色选择器](../ui/UI_COLOR_PICKER_USAGE.md) - 颜色选择器使用指南

### 数学库
- **[Types](Types.md)** - 数学类型和基础类型定义（Vector, Matrix, Quaternion, Plane, Ray）
- **[MathUtils](MathUtils.md)** - 数学工具函数库（角度转换、向量/四元数/矩阵工具）⚡ **性能优化**
- **[Transform](Transform.md)** - 3D变换类（位置、旋转、缩放、层级关系）⚡ **高性能缓存**

### 物理系统 🆕 **v1.6.0**
- **[Physics](Physics.md)** - 物理引擎 API 参考手册 🆕
  - 基础架构（刚体、碰撞体、材质）
  - 碰撞检测系统（粗检测、细检测）
  - 碰撞形状（球体、盒体、胶囊体、网格）
  - 碰撞事件系统
  - 触发器系统
  - 碰撞层和掩码
  - **当前阶段**: 阶段 1-2 已完成（基础架构 + 碰撞检测系统）

### 机器人系统 🤖 **v1.0**
- **[Robot](Robot.md)** - 机器人系统 API 参考手册 🆕
  - URDF文件加载和解析
  - 机器人模型构建（Link、Joint）
  - 关节控制（位置、速度、力矩）
  - 物理约束集成（Bullet Physics）
  - TF（Transform）可视化
  - 正向运动学计算

### 应用层系统 🆕 **Phase 2**
- **[ApplicationHost](ApplicationHost.md)** - 应用宿主，统一入口和生命周期管理 🆕
- **[ModuleRegistry](ModuleRegistry.md)** - 模块注册表，管理应用模块的生命周期和依赖 🆕
- **[AppContext](AppContext.md)** - 应用上下文，提供核心服务引用 🆕
- **[EventBus](EventBus.md)** - 事件总线，类型安全的事件订阅和发布 🆕
- **[SceneManager](SceneManager.md)** - 场景管理器，管理场景栈和热切换 🆕

**应用层文档**:
- [场景API](../application/Scene_API.md) - 场景接口详细文档
- [模块开发指南](../application/Module_Guide.md) - 模块开发指南
- [事件总线使用指南](../application/EventBus_Guide.md) - 事件系统使用指南
- [工具链集成指南](../application/Toolchain_Intergration_Guide.md) - 工具链集成指南

### 错误处理系统
- **[ErrorHandler](ErrorHandler.md)** - 统一错误处理器 🔒 **线程安全** 
  - 统一的错误码体系（50+ 错误码）
  - 多级严重程度（Info/Warning/Error/Critical）
  - 自动源代码位置追踪
  - OpenGL 错误检查
  - 回调系统和错误统计
- **[RenderError](RenderError.md)** - 渲染错误异常类
  - 详细的错误信息
  - 错误分类和严重程度
  - C++20 source_location 支持

### 工具类
- **[Logger](Logger.md)** - 日志系统 🔒 **线程安全**
- **[FileUtils](FileUtils.md)** - 文件工具
- **[GLThreadChecker](GLThreadChecker.md)** - OpenGL 线程安全检查（参见上方"渲染核心"部分）

---

## 快速导航

### 初学者指南
1. [渲染器初始化](Renderer.md#初始化)
2. [理解 OpenGL 线程安全](GLThreadChecker.md#概述) 
3. [创建相机](Camera.md#构造函数) 
4. [创建着色器](Shader.md#从文件加载)
5. [使用着色器缓存](ShaderCache.md#基本使用)
6. [设置 Uniform](UniformManager.md#基本使用)
7. [设置相机矩阵](Camera.md#矩阵操作)
8. [创建UI控件](UIWidget.md#使用示例) 🎨
9. [使用菜单系统](UIMenu.md#使用示例) 🆕 

### 常用任务
- [创建 ECS 世界](ECS.md#快速开始)
- [创建实体和组件](World.md#实体管理)
- [注册系统](World.md#系统管理)
- [查询实体](World.md#查询)
- [创建自定义系统](System.md#创建自定义系统)
- [使用 MeshRenderable](MeshRenderable.md#完整使用示例)
- [使用 SpriteRenderable](SpriteRenderable.md#完整使用示例)
- [窗口管理](Renderer.md#窗口管理)
- [渲染状态设置](RenderState.md#状态设置)
- [OpenGL 状态封装（纹理/VAO/着色器）](RenderState.md#opengl-状态封装)
- [OpenGL 线程安全检查](GLThreadChecker.md#使用示例) 🔒 
- [错误处理](ErrorHandler.md#基本使用) ⚠️
- [异常处理](RenderError.md#使用示例) ⚠️
- [着色器热重载](ShaderCache.md#热重载)
- [加载纹理](TextureLoader.md#基本使用)
- [帧缓冲和离屏渲染](Framebuffer.md#使用示例) 🎨
- [后处理效果](Framebuffer.md#后处理效果链) 🎨
- [MSAA抗锯齿](Framebuffer.md#msaa-抗锯齿) 🎨
- [创建几何形状](MeshLoader.md#几何形状生成方法)
- [网格管理](Mesh.md#公共方法)
- [相机设置和控制](Camera.md#使用示例) 
- [鼠标拾取](Camera.md#鼠标拾取示例) 
- [视锥体裁剪](Camera.md#视锥体裁剪优化) 
- [3D 变换操作](Transform.md#使用示例) 
- [数学工具函数](MathUtils.md#使用示例) 
- [射线投射](Types.md#ray) 
- [日志记录](Logger.md#基本使用)
- [多线程渲染](Renderer.md#线程安全) 🔒
- [创建UI按钮](UIButton.md#使用示例) 🎨
- [创建菜单](UIMenu.md#使用示例) 🆕
- [主题切换](UITheme.md#使用示例) 🎨
- [颜色选择器](UIColorPicker.md#使用示例) 🎨

---

## 模块关系图

```
Renderer
  ├── OpenGLContext (窗口和 OpenGL 上下文)
  │   └── GLThreadChecker (线程安全检查) 
  └── RenderState (渲染状态管理)

Camera 
  ├── Transform (位置、旋转)
  ├── Frustum (视锥体裁剪)
  └── CameraController (相机控制器)
      ├── FirstPersonCameraController
      ├── OrbitCameraController
      └── ThirdPersonCameraController

Shader
  └── UniformManager (Uniform 变量管理)

ShaderCache (单例)
  └── Shader[] (着色器集合)

TextureLoader (单例)
  └── Texture[] (纹理集合)

Framebuffer (帧缓冲对象)
  ├── Texture[] (颜色/深度附件)
  └── RBO[] (渲染缓冲对象)

MeshLoader (静态工具类)
  └── 生成 Mesh 对象

Mesh (网格对象)
  ├── VAO/VBO/EBO (OpenGL 缓冲区)
  └── Vertex[] + Index[] (顶点和索引数据)

Material (材质对象)
  ├── 材质属性 (颜色、金属度、粗糙度等)
  ├── 纹理引用
  └── 渲染状态

ResourceManager (单例)
  ├── Texture[]
  ├── Mesh[]
  ├── Material[]
  └── Shader[]

Transform (3D变换)
  ├── 位置、旋转、缩放
  ├── 父子关系
  └── 高性能缓存

ErrorHandler (单例)
  ├── 错误处理
  ├── 回调系统
  └── 错误统计

RenderError (异常类)
  ├── 错误码和类别
  ├── 严重程度
  └── 源代码位置

Logger (单例)
FileUtils (静态工具类)
MathUtils (静态工具类)
```

---

## API 版本

- **Engine Version**: 0.14.0
- **API Version**: 0.14.0
- **OpenGL Version**: 4.5+
- **C++ Standard**: C++20
- **Last Updated**: 2025-11-04

### 版本更新记录
- **v0.14.0** (2025-11-04): 🏗️ **ECS 和 Renderable 系统实现**
  - ✅ 新增完整的 ECS（Entity Component System）架构
  - ✅ 新增 `Entity` - 轻量级实体 ID（索引 + 版本号）
  - ✅ 新增 `EntityManager` - 实体管理器（创建、销毁、标签系统）
  - ✅ 新增 `Component` - 组件系统（Transform、MeshRender、Sprite、Camera、Light）
  - ✅ 新增 `ComponentRegistry` - 组件注册表（类型安全、O(1) 访问）
  - ✅ 新增 `System` - 系统基类（优先级、生命周期）
  - ✅ 新增内置系统（CameraSystem、TransformSystem、ResourceLoadingSystem、MeshRenderSystem、LightSystem）
  - ✅ 新增 `World` - ECS 世界容器（统一管理接口）
  - ✅ 新增 `Renderable` - 渲染对象基类
  - ✅ 新增 `MeshRenderable` - 3D 网格渲染对象
  - ✅ 新增 `SpriteRenderable` - 2D 精灵渲染对象
  - 🔒 **全面线程安全** - 所有 ECS 操作使用互斥锁保护
  - ⚡ **资源复用** - Transform、Camera 等对象使用 shared_ptr 复用
  - ⚡ **异步资源加载集成** - ResourceLoadingSystem 与 AsyncResourceLoader 深度集成
  - ✅ 新增 ECS 异步加载集成测试程序 (33_ecs_async_test)
  - ✅ 完整的 ECS 和 Renderable API 文档（8个文档）
  - 🎯 **数据导向设计** - 组件存储紧凑，缓存友好
- **v0.13.0** (2025-11-03): 🎨 **帧缓冲系统实现**
  - ✅ 新增 `Framebuffer` 类 - 完整的帧缓冲对象管理
  - ✅ 支持多种附件类型（颜色、深度、模板）
  - ✅ 支持多重采样抗锯齿 (MSAA, 1-16x)
  - ✅ 支持多渲染目标 (MRT, 最多8个颜色附件)
  - ✅ 支持纹理和渲染缓冲对象附件
  - ✅ 动态调整大小 (`Resize()`)
  - ✅ Blit 操作（帧缓冲复制）
  - ✅ 完整性检查和状态查询
  - 🔒 **全面线程安全** - 所有操作使用互斥锁保护
  - ✅ 构建器模式配置 (`FramebufferConfig`)
  - ✅ 新增帧缓冲测试程序 (30_framebuffer_test)
  - ✅ 新增屏幕着色器 (screen.vert/screen.frag)，内置Y轴翻转
  - ✅ 完整的 Framebuffer API 文档和使用指南
  - 🎨 **支持离屏渲染、后处理效果、阴影映射等高级技术**
- **v0.12.0** (2025-11-01): ⚡ **异步资源加载系统**
  - ✅ 新增 `AsyncResourceLoader` 类 - 异步资源加载器
  - ✅ 支持延迟上传（`autoUpload=false`）
  - ✅ 批量上传接口 (`BatchUpload()`)
  - ✅ 进度追踪和回调
  - 🔒 **线程安全** - 工作线程加载，主线程上传
  - ✅ 新增异步加载测试程序 (29_async_loading_test)
  - ✅ 完整的 AsyncResourceLoader API 文档
- **v0.11.0** (2025-10-31): ⚠️ **统一错误处理系统**
  - ✅ 新增 `ErrorHandler` 类 - 统一错误处理器（单例）
  - ✅ 新增 `RenderError` 类 - 渲染错误异常类
  - ✅ 统一的错误码体系（50+ 错误码，7个类别）
  - ✅ 多级严重程度（Info/Warning/Error/Critical）
  - ✅ C++20 `std::source_location` 自动位置追踪
  - ✅ OpenGL 错误自动检查（CHECK_GL_ERROR 宏）
  - ✅ 错误回调系统（支持多个回调）
  - ✅ 错误统计功能（按严重程度统计）
  - ✅ 便捷宏（RENDER_ERROR、RENDER_TRY/CATCH、RENDER_ASSERT 等）
  - 🔒 **全面线程安全** - 所有操作都是线程安全的
  - ✅ 集成到所有单例类（ResourceManager、ShaderCache、TextureLoader 等）
  - ✅ 集成到所有核心类（Texture、Mesh、Material、Camera 等）
  - ✅ 完整的错误处理 API 文档和使用指南
  - ⚠️ **提升代码健壮性** - 统一的错误处理和追踪机制
- **v0.10.0** (2025-10-30): 🔒 **OpenGL 线程安全检查系统**
  - ✅ 新增 `GLThreadChecker` 类 - OpenGL 线程安全检查器（单例）
  - ✅ 自动检测所有 OpenGL 调用是否在正确的线程中执行
  - ✅ 提供详细的错误信息（文件名、行号、函数名、线程 ID）
  - ✅ 支持运行时配置（terminateOnError 选项）
  - ✅ 编译时可禁用以提高性能（`GL_DISABLE_THREAD_CHECK`）
  - ✅ 全面集成到核心模块（OpenGLContext、RenderState、Shader、Texture、Mesh）
  - ✅ 新增线程安全检查测试程序 (22_gl_thread_safety_test)
  - ✅ 完整的 GLThreadChecker API 文档和使用指南
  - 🔒 **保障渲染线程安全** - 防止跨线程 OpenGL 调用导致的崩溃
- **v0.9.0** (2025-10-29): 📷 **相机系统实现**
  - ✅ 新增 `Camera` 类 - 完整的3D相机功能
  - ✅ 支持透视投影和正交投影
  - ✅ 视图矩阵和投影矩阵计算
  - ✅ 视锥体裁剪（`Frustum`）
  - ✅ 屏幕↔世界坐标转换
  - ✅ 新增 `FirstPersonCameraController` - 第一人称FPS风格控制
  - ✅ 新增 `OrbitCameraController` - 轨道相机（类似Blender）
  - ✅ 新增 `ThirdPersonCameraController` - 第三人称跟随相机
  - 🔒 **全面线程安全** - 所有公共方法使用互斥锁保护
  - ⚡ **智能缓存机制** - 视图/投影/视锥体自动缓存
  - ✅ 新增相机系统测试程序 (20_camera_test)
  - ✅ 完整的 Camera API 文档和使用指南
  - 📷 支持鼠标拾取、视锥体优化等高级功能
- **v0.8.0** (2025-10-29): ⚡ **数学库集成与性能优化**
  - ✅ 完整的数学库集成（Transform、MathUtils）
  - ✅ 新增 `Transform` 类 - 3D变换管理（位置、旋转、缩放、父子关系）
  - ✅ 新增 `MathUtils` 命名空间 - 完整的数学工具函数库
  - ✅ 新增 `Plane` 和 `Ray` 数据结构
  - ⚡ **性能优化** - FromEuler 快 40-50%
  - ⚡ **性能优化** - LookRotation 快 30-40%
  - ⚡ **性能优化** - Transform 世界变换缓存（10-50x 提升）
  - ⚡ **SIMD 优化** - 启用 AVX2 指令集
  - ⚡ **并行处理** - OpenMP 支持批量变换
  - ✅ SafeNormalize 智能归一化（快30%）
  - ✅ 内存对齐优化（EIGEN_MAKE_ALIGNED_OPERATOR_NEW）
  - ✅ 批量变换接口（TransformPoints/Directions）
  - ✅ 新增数学库测试程序 (18_math_test)
  - ✅ 新增性能基准测试程序 (19_math_benchmark)
  - ✅ 完整的 MathUtils 和 Transform API 文档
- **v0.7.0** (2025-10-28): 🎯 **统一资源管理系统**
  - ✅ 新增 `ResourceManager` 类 - 统一资源管理器
  - ✅ 支持纹理、网格、材质、着色器的统一管理
  - ✅ 资源注册、获取、移除接口
  - ✅ 自动引用计数和生命周期管理
  - ✅ 自动清理未使用资源（`CleanupUnused()`）
  - ✅ 详细的资源统计和监控功能
  - ✅ 批量操作接口（`Clear()`, `ClearType()`）
  - ✅ ForEach遍历功能
  - 🔒 **全面线程安全** - 所有公共方法使用互斥锁保护
  - ✅ 新增资源管理器测试程序 (15_resource_manager_test)
  - ✅ 新增资源管理器线程安全测试程序 (16_resource_manager_thread_safe_test)
  - ✅ 完整的 ResourceManager API 文档和使用指南
- **v0.6.0** (2025-10-28): 🎨 **材质纹理加载与性能优化**
  - ✅ `MeshLoader::LoadFromFileWithMaterials()` - 从模型文件加载材质和纹理
  - ✅ 新增 `MeshWithMaterial` 结构体，关联网格、材质和名称
  - ✅ 自动加载漫反射、镜面反射、法线、AO、自发光贴图
  - ✅ **性能优化**: `TextureLoader::LoadTexture()` 使用双重检查锁定，IO 操作移到锁外
  - ✅ **性能优化**: `Texture::LoadFromFile()` 将 IMG_Load 移到锁外
  - ✅ **性能优化**: `Material::Bind()` 采用双阶段锁定，OpenGL 调用移到锁外
  - ✅ **路径兼容**: 移除 `std::filesystem` 依赖，解决中文路径问题
  - ✅ 纹理智能缓存：使用完整路径作为标识，自动去重
  - ✅ Phong 着色器添加纹理支持（`diffuseMap` 和 `hasDiffuseMap`）
  - ✅ 新增模型材质加载测试程序 (14_model_material_loader_test)
  - ✅ 全面支持 PMX/MMD 模型的材质和纹理加载
- **v0.5.0** (2025-10-28): 🎨 **材质系统完整实现**
  - ✅ 新增 `Material` 类 - 完整的材质管理系统
  - ✅ 支持颜色属性（环境光、漫反射、镜面反射、自发光）
  - ✅ 支持物理材质参数（金属度、粗糙度、镜面反射强度）
  - ✅ 支持多纹理贴图
  - ✅ 支持自定义 uniform 参数
  - ✅ 渲染状态集成（混合、剔除、深度测试）
  - 🔒 **全面线程安全** - 所有公共方法使用互斥锁保护
  - ✅ 新增材质系统测试程序 (12_material_test)
  - ✅ 新增材质系统线程安全测试程序 (13_material_thread_safe_test)
  - ✅ 新增 Phong 光照着色器（支持镜面反射）
  - ✅ 完整的 Material API 文档和使用指南
- **v0.4.0** (2025-10-28): 🔒 **网格系统线程安全优化**
  - `Mesh` 和 `MeshLoader` 全面线程安全
  - 所有公共方法使用互斥锁保护
  - 移动操作使用 `std::scoped_lock` 避免死锁
  - 所有 getter 方法添加线程安全保护
  - 新增网格系统线程安全测试程序 (10_mesh_thread_safe_test)
  - 新增完整的网格线程安全使用指南文档
  - 更新 API 文档，添加详细线程安全说明
- **v0.3.0** (2025-10-28): 🔒 **纹理系统线程安全优化**
  - `Texture` 和 `TextureLoader` 全面线程安全
  - 所有公共方法使用互斥锁保护
  - 移动操作使用 `std::scoped_lock` 避免死锁
  - 修复 `GetTotalMemoryUsage()` 的线程安全问题
  - 新增纹理系统线程安全测试程序 (09_texture_thread_safe_test)
  - 更新 API 文档，添加详细线程安全说明
- **v0.2.0** (2025-10-28): 🔒 **Renderer 类线程安全优化**
  - 所有公共方法都是线程安全的
  - 添加互斥锁保护所有可变状态
  - 初始化状态使用原子操作
  - 新增 Renderer 线程安全测试程序 (08_renderer_thread_safe_test)
- **v0.1.0** (2025-10-27): 新增 OpenGL 状态封装（纹理/缓冲区/着色器程序管理）
- **v0.0.0** (2025-10-27): 初始版本，基础渲染系统

---

## 示例程序

完整示例代码请参考：
- [01_basic_window.cpp](../../examples/01_basic_window.cpp) - 基础窗口和渲染循环
- [02_shader_test.cpp](../../examples/02_shader_test.cpp) - 着色器系统使用
- [03_geometry_shader_test.cpp](../../examples/03_geometry_shader_test.cpp) - 几何着色器和缓存
- [04_state_management_test.cpp](../../examples/04_state_management_test.cpp) - 状态管理和 OpenGL 状态封装
- [05_texture_test.cpp](../../examples/05_texture_test.cpp) - 纹理加载和渲染
- [06_mesh_test.cpp](../../examples/06_mesh_test.cpp) - 网格系统和几何形状生成
- [07_thread_safe_test.cpp](../../examples/07_thread_safe_test.cpp) - 着色器系统线程安全测试 🔒
- [08_renderer_thread_safe_test.cpp](../../examples/08_renderer_thread_safe_test.cpp) - Renderer 线程安全测试 🔒
- [09_texture_thread_safe_test.cpp](../../examples/09_texture_thread_safe_test.cpp) - 纹理系统线程安全测试 🔒
- [10_mesh_thread_safe_test.cpp](../../examples/10_mesh_thread_safe_test.cpp) - **网格系统线程安全测试** 🔒
- [11_model_loader_test.cpp](../../examples/11_model_loader_test.cpp) - 外部模型加载测试
- [12_material_test.cpp](../../examples/12_material_test.cpp) - **材质系统测试**
- [13_material_thread_safe_test.cpp](../../examples/13_material_thread_safe_test.cpp) - **材质系统线程安全测试** 🔒
- [14_model_material_loader_test.cpp](../../examples/14_model_material_loader_test.cpp) - **模型材质纹理加载测试**
- [15_resource_manager_test.cpp](../../examples/15_resource_manager_test.cpp) - **资源管理器测试** 
- [16_resource_manager_thread_safe_test.cpp](../../examples/16_resource_manager_thread_safe_test.cpp) - **资源管理器线程安全测试** 🔒 
- [17_model_with_resource_manager_test.cpp](../../examples/17_model_with_resource_manager_test.cpp) - **使用资源管理器加载模型** 
- [18_math_test.cpp](../../examples/18_math_test.cpp) - **数学库功能测试** 
- [19_math_benchmark.cpp](../../examples/19_math_benchmark.cpp) - **数学库性能基准测试** ⚡ 
- [20_camera_test.cpp](../../examples/20_camera_test.cpp) - **相机系统测试（三种相机控制模式）** 📷 
- [21_transform_thread_safe_test.cpp](../../examples/21_transform_thread_safe_test.cpp) - **Transform 线程安全测试** 🔒 
- [22_gl_thread_safety_test.cpp](../../examples/22_gl_thread_safety_test.cpp) - **OpenGL 线程安全检查测试** 🔒 
- [29_async_loading_test.cpp](../../examples/29_async_loading_test.cpp) - **异步资源加载测试** ⚡
- [30_framebuffer_test.cpp](../../examples/30_framebuffer_test.cpp) - **帧缓冲测试（离屏渲染、后处理、MSAA）** 🎨 
- [33_ecs_async_test.cpp](../../examples/33_ecs_async_test.cpp) - **ECS + 异步加载集成测试** 🏗️
- [60_ui_framework_showcase.cpp](../../examples/60_ui_framework_showcase.cpp) - **UI框架展示** 🎨
- [61_ui_menu_example.cpp](../../examples/61_ui_menu_example.cpp) - **UI菜单系统示例** 🆕 

## 相关文档

### 使用指南
- [开发指南](../DEVELOPMENT_GUIDE.md)
- [着色器缓存使用指南](../SHADER_CACHE_GUIDE.md)
- [纹理系统使用指南](../TEXTURE_SYSTEM.md)
- [架构文档](../ARCHITECTURE.md)

### 线程安全 🔒
- [OpenGL 线程安全检查](GLThreadChecker.md) 
- [Renderer 线程安全指南](../RENDERER_THREAD_SAFETY.md)
- [RenderState 线程安全文档](../THREAD_SAFETY.md)
- [网格系统线程安全指南](../MESH_THREAD_SAFETY.md)
- [整体线程安全总结](../THREAD_SAFETY_SUMMARY.md)

### 性能优化 ⚡
- [数学库性能优化报告](../MATH_FINAL_OPTIMIZATION_REPORT.md) 

### 项目管理
- [Phase 1 进度](../todolists/PHASE1_BASIC_RENDERING.md)
- [API 文档完成总结](../API_DOCUMENTATION_SUMMARY.md)

---

[返回文档首页](../README.md)

