# Phase 1: 基础渲染功能 TODO 列表

## 项目设置

### ✅ 项目初始化
- [x] 创建项目目录结构
- [x] 编写项目文档
- [x] 配置 SDL3 依赖（third_party 中已有）

### ✅ 构建设置
- [x] 创建 CMakeLists.txt 配置
  - [x] 配置 C++17 标准
  - [x] 添加 SDL3 依赖（third_party/SDL）
  - [x] 添加 Eigen3 依赖（third_party/eigen-3.4.0）
  - [x] 添加 OpenGL 库
  - [x] 配置编译选项
- [x] 创建 src/ 目录结构
- [x] 创建 include/ 目录结构
- [x] 创建 tests/ 目录

---

## 核心基础设施

### ✅ OpenGL 抽象层
- [x] 创建 OpenGL 上下文管理
  - [x] OpenGLContext 类
  - [x] 窗口创建（SDL3）
  - [x] 上下文初始化
  - [x] 扩展检测
  - [x] 版本检测（确保 OpenGL 4.5+）
  
### ✅ 着色器系统
- [x] Shader 基类实现
  - [x] 着色器源码读取
  - [x] 顶点着色器编译
  - [x] 片段着色器编译
  - [x] 几何着色器编译（已测试）
  - [x] 程序链接
  - [x] Uniform 设置接口（UniformManager）
  - [x] 错误处理和日志
- [x] UniformManager 实现
  - [x] Uniform 位置缓存
  - [x] 支持多种数据类型（int, float, vec2/3/4, mat3/4）
  - [x] Uniform 自动扫描和列出
- [x] 着色器资源管理
  - [x] 着色器缓存系统（ShaderCache）
  - [x] 重载支持（Reload方法）
  - [x] 资源引用计数（shared_ptr）
  - [x] 着色器预编译

### ✅ 状态管理
- [x] RenderState 类
  - [x] 深度测试控制
  - [x] 混合模式控制
  - [x] 面剔除控制
  - [x] 视口设置
  - [x] 裁剪区域设置
  - [x] 状态缓存和优化
- [x] OpenGL 状态封装
  - [x] 基础状态管理
  - [x] 纹理绑定管理（支持32个纹理单元）
  - [x] 缓冲区绑定管理（VAO, VBO, EBO, UBO, SSBO）
  - [x] 着色器程序管理（自动缓存切换）

---

## 资源管理系统

### ✅ 纹理管理
- [x] Texture 类
  - [x] 纹理加载（使用 SDL_image）
  - [x] 纹理创建（运行时生成）
  - [x] 纹理参数设置
  - [x] Mipmap 生成
  - [x] 纹理释放
- [x] TextureLoader
  - [x] 纹理缓存机制
  - [x] 异步加载支持
  - [x] 错误处理
- [x] 支持格式：PNG, JPG, BMP, TGA

### ✅ 网格管理
- [x] Vertex 数据结构
  ```cpp
  struct Vertex {
      Vector3 position;
      Vector2 texCoord;
      Vector3 normal;
      Color color;
  };
  ```
- [x] Mesh 类
  - [x] 顶点数据设置
  - [x] 索引数据设置
  - [x] VBO/VAO 创建
  - [x] 绘制接口（Draw, DrawInstanced）
  - [x] 内存管理
  - [x] 法线/切线重计算
  - [x] 包围盒计算
- [x] MeshLoader
  - [x] 基础网格创建接口
  - [x] 几何形状生成（立方体、球体、圆柱、圆锥、圆环、胶囊、平面、四边形、三角形、圆形）
  - [x] 文件加载（OBJ, FBX, GLTF, Collada, Blender, 3DS, PLY, STL）✅ 已完成

### ✅ 材质管理
- [x] Material 类
  - [x] 材质属性（环境色、漫反射、镜面反射、自发光）
  - [x] 纹理引用（支持多纹理）
  - [x] 着色器引用
  - [x] 渲染状态（混合、剔除、深度）
  - [x] 物理材质属性（金属度、粗糙度、镜面反射强度）
  - [x] 自定义 uniform 参数
- [x] MaterialManager（暂不实现，使用 shared_ptr 直接管理）
  - [x] 材质生命周期管理（通过 shared_ptr）

### ✅ 资源管理器（ResourceManager）
- [x] 资源注册机制
- [x] 引用计数
- [x] 自动释放机制
- [x] 资源统计和监控
- [x] 统一管理纹理、网格、材质、着色器
- [x] 线程安全实现
- [x] 批量操作接口
- [x] ForEach遍历功能

---

## 渲染核心

### ✅ Renderer 主类
- [x] Renderer 实现
  - [x] 初始化（Initialize）
  - [x] 清理（Shutdown）
  - [x] 帧开始（BeginFrame）
  - [x] 帧结束（EndFrame）
  - [x] 呈现（Present）
  - [x] 清屏（Clear）
- [x] 窗口管理
  - [x] 窗口创建
  - [x] 窗口大小控制
  - [x] 全屏支持
  - [x] VSync 控制
  - [x] 窗口事件处理（由 SDL3 提供）
- [x] 渲染统计
  - [x] Draw Call 计数（框架已准备）
  - [x] 三角形计数（框架已准备）
  - [x] FPS 统计
  - [x] 帧时间统计

### ✅ 相机系统
- [x] Camera 类
  - [x] 透视投影
  - [x] 正交投影
  - [x] 视图变换矩阵
  - [x] 投影变换矩阵
  - [x] 视锥体（Frustum）
  - [x] 屏幕↔世界坐标转换
  - [x] 智能缓存机制（视图/投影/视锥体）
  - [x] 线程安全实现
- [x] 相机控制
  - [x] 第一人称相机（FirstPersonCameraController）
  - [x] 第三人称相机（ThirdPersonCameraController）
  - [x] 轨道相机（OrbitCameraController）
  - [x] 相机移动和旋转
  - [x] 鼠标控制支持
  - [x] 平滑跟随（第三人称）
- [ ] 多相机支持（可选，后续）

### 渲染对象抽象（ECS 集成）

#### ✅ 基础框架（已完成）
- [x] Renderable 基类
  - [x] 变换矩阵（使用 shared_ptr 复用 Transform）
  - [x] 可见性控制（SetVisible/IsVisible）
  - [x] 渲染接口（Render、SubmitToRenderer 纯虚函数）
  - [x] 包围盒接口（GetBoundingBox 纯虚函数）
  - [x] 层级和优先级管理（layerID、renderPriority）
  - [x] 线程安全实现（std::shared_mutex）
  
- [x] MeshRenderable 类（3D 渲染）
  - [x] 网格引用（Ref<Mesh>）
  - [x] 材质设置（Ref<Material>）
  - [x] 渲染实现（Render: 材质绑定 → uniform 设置 → 网格绘制）
  - [x] 阴影属性（castShadows、receiveShadows）
  - [x] 包围盒计算（支持世界变换）
  
- [x] SpriteRenderable 类（2D 渲染）
  - [x] 纹理引用（Ref<Texture>）
  - [x] UV 源矩形（sourceRect）
  - [x] 显示大小和着色（size、tintColor）
  - [x] 2D 包围盒计算
  - [x] 渲染实现 ✅ **已完成（共享资源 + 正交矩阵 + UniformManager）**

#### ✅ ECS 系统集成（部分完成）
- [x] MeshRenderSystem ✅ **已完整实现**
  - [x] 查询具有 TransformComponent + MeshRenderComponent 的实体
  - [x] 检查可见性和资源加载状态
  - [x] 创建 MeshRenderable 对象（使用对象池 m_renderables）
  - [x] 设置所有属性（mesh、material、transform、阴影等）
  - [x] 提交到渲染队列（renderer->SubmitRenderable）
  - [x] 渲染统计（visibleMeshes、culledMeshes、drawCalls）
  - [x] 视锥体裁剪 
  
- [x] SpriteRenderSystem
  - [x] 框架代码存在（构造函数、Update 接口）
  - [x] 查询 TransformComponent + SpriteRenderComponent
  - [x] 创建 SpriteRenderable ✅ **对象池复用，自动填充纹理/尺寸/颜色**
  - [x] 提交到渲染队列 ✅ **结合 Renderer 排序渲染**
- 当前状态：已完成核心功能（正交矩阵、alpha 混合、尺寸回退）并扩展：
  - ✅ 九宫格拆分（CPU 端 3×3 分块，保持单批次）
  - ✅ 镜像翻转（`SpriteUI::SpriteFlipFlags` + 模型矩阵变换）
  - ✅ 子像素对齐（`snapToPixel` + `subPixelOffset`）
  - ✅ 统计接口：`GetLastBatchCount()`、`GetLastSubmittedSpriteCount()` 用于批处理验证
  - ✅ `43_sprite_batch_validation_test` 新增九宫格/翻转场景回归批次数与实例数

- [ ] SpriteAnimationSystem 🚧
  - [x] 动画组件数据结构（剪辑/播放状态）
  - [x] 基础播放逻辑（按帧推进、循环控制、更新 sourceRect）
  - [ ] 事件回调与高级模式（PingPong、反向播放）
  - 当前状态：基础动画驱动已实现（示例 `40_sprite_animation_test`），等待资源/事件扩展
  
- [x] 渲染队列管理 ✅ **已完整实现**
  - [x] Renderer::SubmitRenderable() - 提交对象到队列
  - [x] Renderer::FlushRenderQueue() - 批量渲染并排序
- [x] Renderer::SortRenderQueue() - 按层级 → 材质键 → 渲染优先级 → 类型排序（透明对象保持原提交顺序）
  - [x] 对象池复用（MeshRenderSystem::m_renderables）

#### ⚠️ 已知问题
   
1. **材质键覆盖范围仍需扩展**（评估 2025-11-08 → 更新 2025-11-08）
   - ✅ `MeshRenderSystem` 会为 `MeshRenderable` 构建稳定键（包含材质、着色器、渲染状态、覆盖哈希、管线标记）
   - ✅ `SpriteRenderSystem` 的批次键使用纹理指针 + 视图/投影哈希 + 混合模式标记，落地到 `SpriteBatchRenderable` 后会写回排序键
   - ✅ 非 ECS 流程：`Renderer::SubmitRenderable()` 现已在缺失时自动补全 `MeshRenderable`/`SpriteRenderable`/`TextRenderable` 的材质键，`materialSortKeyMissing` 在 `37_batching_benchmark`、`43_sprite_batch_validation_test`、`44_text_render_test` 中均为 0
   - ✅ `SpriteRenderable` 与 `TextRenderable` 复用共享资源后可直接提交队列，仍保持 UI 层屏幕空间标记与颜色/纹理哈希覆盖
   - ➕ 后续新增渲染类型时，继续通过 UniformManager/ResourceManager 生成稳定键，避免退化到指针哈希；透明管线材质分组与 `MaterialStateCache` 待纳入下一阶段优化
   
2. **光照系统后续扩展**
   - 计划加入多光源阴影与环境光探针
   - 统一点光/聚光阴影贴图与 `LightManager` 集成
   - 评估 `LightComponent` 事件回调、优先级配置可视化工具

#### 🎯 下一步工作
1. **透明排序与材质缓存** ✅ 2025-11-08  
   - Renderer 内对透明段应用 “层级 → 深度提示 → 材质键 → RenderPriority → 原序” 稳定排序  
   - 新增 `MaterialStateCache` 缓存最近一次 `Material::Bind()`，Mesh/Text/Sprite 共享缓存逻辑  
   - `43_sprite_batch_validation_test`、`44_text_render_test`、`37_batching_benchmark` 均验证 `materialSortKeyMissing == 0`
2. **渲染层级系统设计** - 引入 RenderLayer/RenderQueue，实现层级调度与属性配置

---

## 2D 渲染

### 精灵系统
- [x] Sprite 类
  - [x] 纹理引用
  - [x] 裁剪区域（UV 坐标）
  - [x] 颜色混合
  - [x] 缩放和平移
- [x] SpriteRenderer
  - [x] 精灵批处理（`SpriteBatcher` + GPU Instancing）
  - [x] 2D 变换（正交矩阵 / 屏幕空间支持）
  - [x] 渲染优化（分层排序、批次统计、资源复用）
- [x] SpriteAtlasImporter
  - [x] 解析 JSON 图集（帧 + 动画配置）
  - [x] 输出 SpriteSheet / SpriteAnimationComponent
  - [x] ResourceManager 注册 & 依赖追踪
  - [x] 示例整合（40/42 号示例展示状态机与批处理验证）

### 文本渲染
- [x] Font 类
  - [x] 字体加载（SDL_ttf）
  - [x] 字体大小控制
  - [x] 字符纹理缓存
- [x] Text 类
  - [x] 文本内容设置
  - [x] 文本颜色
  - [x] 文本对齐
  - [x] 换行支持（可选）
- [x] TextRenderer
  - [x] 文本批处理
  - [x] 文本渲染

---

## 3D 渲染

### 模型渲染
- [x] Model 类 ✅ （支持网格/材质集合、局部变换与统计）
  - [x] 网格集合
  - [x] 材质绑定
  - [x] 变换矩阵
- [x] ModelRenderer ✅ （ModelRenderable + ModelRenderSystem 提交流程）
  - [x] 模型加载接口
  - [x] 模型渲染
  - [x] 模型变换

### 基本几何形状
- [x] 预定义网格生成器
  - [x] 立方体（Cube）
  - [x] 球体（Sphere）
  - [x] 平面（Plane）
  - [x] 圆锥（Cone）
  - [x] 圆柱（Cylinder）
  - [x] 圆环（Torus）
  - [x] 胶囊（Capsule）
  - [x] 四边形（Quad）
  - [x] 三角形（Triangle）
  - [x] 圆形（Circle）
  - [x] 线框/法线贴图验证示例（`50_geometry_catalog_test`）

---

## 光照系统（基础）

### 光源类型
- [x] Light 基类
  - [x] 光源颜色
  - [x] 光源强度
- [x] DirectionalLight（定向光）
  - [x] 方向设置
  - [x] 光照计算
- [x] PointLight（点光源）
  - [x] 位置设置
  - [x] 范围设置
  - [x] 衰减计算
- [x] SpotLight（聚光灯）
  - [x] 位置和方向
  - [x] 内角和外角
  - [x] 衰减计算
- [x] AmbientLight（环境光）
  - [x] 环境光颜色和强度

### 光照管理器
- [x] LightManager 类
  - [x] 光源注册
  - [x] 最大光源数限制（建议4-8个）
  - [x] 光照数据上传到着色器
  - [x] 光源排序（按重要性）

---

## 渲染层级系统

### RenderLayer 类
- [ ] 层级创建和销毁
- [ ] 优先级设置
- [ ] 启用/禁用控制
- [ ] 渲染对象添加/移除
- [ ] 层级排序功能

### 渲染队列
- [ ] RenderQueue 类
  - [ ] 按优先级排序
  - [ ] 按材质排序
  - [ ] 按深度排序
- [ ] 渲染提交机制
  - [ ] Submit 接口
  - [ ] Flush 接口

### 层级属性配置
- [ ] 深度测试配置
- [ ] 混合模式配置
- [ ] 面剔除配置
- [ ] 视口配置

---

## 数学库集成

### ✅ 数学数据结构
- [x] Vector2, Vector3, Vector4
- [x] Matrix3, Matrix4
- [x] Quaternion
- [x] Color（RGBA）
- [x] Rect, AABB
- [x] Plane, Ray

### ✅ 数学工具函数
- [x] 矩阵运算（使用 Eigen）
- [x] 向量运算
- [x] 四元数运算
- [x] 变换计算（MathUtils）

### ✅ Transform 类
- [x] 位置、旋转、缩放
- [x] 本地和世界变换
- [x] 父子关系

---

## 基础工具

### ✅ 日志系统
- [x] Logger 类
  - [x] 日志级别（Debug, Info, Warning, Error）
  - [x] 文件输出（自动创建 logs 文件夹）
  - [x] 时间戳命名（每次运行新日志文件）
  - [x] 控制台输出
  - [x] 格式化日志

### ✅ 文件系统
- [x] FileUtils
  - [x] 文件读取
  - [x] 路径处理
  - [x] 资源路径管理

### 配置系统（可选）
- [ ] Config 类
  - [ ] 配置文件读取
  - [ ] 设置项管理
  - [ ] 默认值设置

---

## 示例程序

### 基础测试
- [x] 创建窗口测试 (01_basic_window)
- [x] 着色器加载和编译测试 (02_shader_test)
- [x] 几何着色器和缓存系统测试 (03_geometry_shader_test)
- [x] OpenGL 状态管理测试 (04_state_management_test) - 展示状态缓存和三角形渲染
- [x] 纹理加载和渲染测试 (05_texture_test) - 纹理系统和 TextureLoader
- [x] 网格管理系统测试 (06_mesh_test) - 10种几何形状生成和渲染（立方体、球体、圆柱、圆锥等）
- [x] 模型文件加载测试 (11_model_loader_test) - 外部模型加载（OBJ/FBX/GLTF等）
- [x] 材质系统测试 (12_material_test) - 材质属性、纹理、渲染状态测试 ✅ 已完成
- [x] 资源管理器测试 (15_resource_manager_test) - 统一资源管理、引用计数、自动清理 ✅ 已完成
- [x] 资源管理器线程安全测试 (16_resource_manager_thread_safe_test) - 多线程并发访问测试 ✅ 已完成
- [x] 资源管理器实战测试 (17_model_with_resource_manager_test) - 使用资源管理器加载和管理完整模型 ✅ 已完成
- [x] 数学库测试 (18_math_test) - Transform、数学工具函数、Ray、Plane 测试 ✅ 已完成
- [x] 数学库性能基准测试 (19_math_benchmark) - 性能测试和优化验证 ✅ 已完成
- [x] 相机系统测试 (20_camera_test) - 三种相机控制模式、投影切换、miku模型渲染 ✅ 已完成
- [x] 精灵渲染测试 (38_sprite_render_test) - SpriteRenderSystem + UV 裁剪 + Tint ✅ 新增
- [x] 光照测试 (45_lighting_test)

### 综合示例
- [ ] Simple Scene - 基础3D场景
  - [ ] 相机控制
  - [ ] 多个物体
  - [ ] 光照渲染
- [ ] 2D Demo
  - [ ] 精灵渲染
  - [ ] 文本显示
  - [ ] UI元素

---

## 测试

### 单元测试
- [ ] 数学库测试
- [ ] 资源管理测试
- [ ] 着色器编译测试
- [ ] 渲染管线测试

### 集成测试
- [ ] 场景渲染测试
- [ ] 性能基准测试
- [ ] 内存泄漏检测

---

## 文档更新

### 代码文档
- [ ] 核心类文档注释
- [ ] API 文档更新
- [ ] 使用示例代码

### 开发文档
- [ ] 更新开发指南
- [ ] 添加快速开始示例
- [ ] API 使用示例

---

## 性能优化（基础）

### 渲染优化
- [ ] 减少状态切换
- [ ] 绘制调用批处理
- [ ] 纹理绑定优化
- [ ] 着色器程序缓存

### 内存优化
- [ ] 对象池（Object Pooling）
- [ ] 资源内存管理
- [ ] 内存统计

---

## 依赖项使用

### ✅ SDL3
- [x] 窗口创建
- [x] 事件处理
- [x] 定时器
- [ ] 音频（可选，后续）

### ✅ Eigen3
- [x] 矩阵运算
- [x] 向量运算
- [x] 四元数运算
- [x] 线性代数功能

### ✅ Assimp
- [x] 外部模型文件加载（OBJ, FBX, GLTF, Collada, Blender, 3DS, PLY, STL）
- [x] 自动三角化和法线生成
- [x] 网格优化和后处理
- [x] 多网格模型支持

---

## 里程碑检查点

### Milestone 1: 基础框架 ✅ 已完成
- [x] 项目构建成功
- [x] 窗口创建和渲染
- [x] 基础着色器加载
- [x] 着色器缓存系统
- [x] 几何着色器支持
- [x] 网格渲染（Mesh 和 MeshLoader）

### Milestone 2: 基础渲染
- [x] 纹理渲染
- [x] 材质系统 ✅ 已完成
- [x] 相机系统 ✅ 已完成
- [x] 光照系统 ✅ 已完成

### Milestone 3: 2D 和层级
- [x] 精灵渲染 ✅ （SpriteRenderable + SpriteRenderSystem 完成）
- [ ] 文本渲染
- [ ] 层级系统
- [ ] 完整示例程序

### Milestone 4: 完善和测试
- [ ] 所有基础功能测试通过
- [ ] 文档完善
- [ ] 性能基准测试
- [ ] 准备进入 Phase 2

---

## 开发建议

### 优先级排序
1. **高优先级**: 项目结构、OpenGL抽象、基础渲染 ✅ **已完成**
2. **中优先级**: 资源管理 ✅ **已完成**、相机系统 ✅ **已完成**、光照系统 ✅ **已完成**、2D渲染深化（文本/层级）
3. **低优先级**: 高级特性、优化、工具

### 开发顺序
1. ✅ 设置 CMakeLists.txt 和项目结构
2. ✅ 实现 OpenGL 抽象层
3. ✅ 实现着色器系统
4. ✅ 实现基础 Mesh 渲染
5. ✅ 实现相机系统 ✅ **已完成**
6. ✅ 实现光照系统 ✅ **已完成**
7. 实现 2D 渲染（Sprite ✅，文本/层级待完成）
8. 实现渲染层级
9. 编写示例和测试
10. 文档和优化

### 注意事项
- 确保 OpenGL 上下文正确管理
- 注意资源生命周期管理
- 编写清晰的错误处理
- 保持代码模块化
- 及时更新文档

---

**完成标准**：能够渲染一个包含光照的3D场景，支持2D元素叠加，具备完整的渲染层级管理。

[返回文档首页](../README.md)

