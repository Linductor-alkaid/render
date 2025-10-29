# 架构设计文档

## 目录
[返回文档首页](README.md)

## 系统架构概述

RenderEngine 采用分层架构设计，确保高度的模块化和可扩展性。

## 核心架构层次

### 1. 硬件抽象层 (Hardware Abstraction Layer)
- **目的**: 封装OpenGL底层调用
- **组件**:
  - Context 管理 (OpenGL 上下文)
  - 资源管理 (纹理、缓冲区、着色器等)
  - 状态管理 (渲染状态机)
  - 扩展检测与兼容性处理

### 2. 渲染后端层 (Rendering Backend)
- **目的**: 提供跨平台的渲染接口
- **组件**:
  - 渲染器抽象基类
  - OpenGL 渲染器实现
  - 渲染管线管理器
  - 着色器编译器

### 3. 渲染对象层 (Renderable Objects)
- **目的**: 封装不同类型的渲染对象
- **组件**:
  - Mesh (网格)
  - Sprite (精灵)
  - Text (文本)
  - Particle (粒子)
  - Camera (相机)
  - Light (光源)

### 4. 场景管理层 (Scene Management)
- **目的**: 管理场景中的渲染对象
- **组件**:
  - Scene Graph (场景图)
  - Spatial Partitioning (空间分割)
  - Frustum Culling (视锥剔除)
  - LOD 系统

### 5. 渲染调度层 (Render Scheduler)
- **目的**: 优化渲染流程
- **组件**:
  - Render Queue (渲染队列)
  - Batching System (批处理系统)
  - Occlusion Culling (遮挡剔除)
  - 延迟渲染管理

### 6. 效果处理层 (Effect Processing)
- **目的**: 后处理与特效
- **组件**:
  - Post-processing Chain
  - Bloom
  - SSAO (Screen Space Ambient Occlusion)
  - Motion Blur
  - Depth of Field

### 7. ECS 集成层 (ECS Integration)
- **目的**: 与ECS系统集成
- **组件**:
  - Render Components
  - Render System
  - Layer Management
  - 组件生命周期管理

## 核心类设计

### Renderer 类
```cpp
class Renderer {
public:
    bool Initialize();
    void Shutdown();
    void BeginFrame();
    void EndFrame();
    void Present();
    
    RenderLayer* CreateLayer(uint32_t priority);
    void DestroyLayer(RenderLayer* layer);
    
    Mesh* CreateMesh();
    Texture* CreateTexture();
    Shader* CreateShader();
    
    void Submit(Renderable* object);
    void Flush();
};
```

### RenderLayer 类
```cpp
class RenderLayer {
public:
    void SetEnabled(bool enabled);
    void SetPriority(uint32_t priority);
    
    void Clear();
    void AddRenderable(Renderable* object);
    void RemoveRenderable(Renderable* object);
    
    void Render(RenderContext* context);
    
private:
    uint32_t m_priority;
    bool m_enabled;
    std::vector<Renderable*> m_renderables;
};
```

### Renderable 基类
```cpp
class Renderable {
public:
    virtual ~Renderable() = default;
    
    virtual void Render(RenderContext* context) = 0;
    virtual void Update(float dt) {}
    
    void SetLayer(RenderLayer* layer);
    void SetVisible(bool visible);
    
protected:
    RenderLayer* m_layer;
    bool m_visible;
    Transform m_transform;
};
```

## 资源管理系统

### 资源生命周期
1. **加载**: 从文件系统或内存加载资源
2. **验证**: 验证资源完整性和兼容性
3. **缓存**: 存储在资源缓存中
4. **使用**: 渲染时引用资源
5. **释放**: 未使用时自动释放

### 资源管理器
```cpp
class ResourceManager {
public:
    Texture* LoadTexture(const std::string& path);
    Mesh* LoadMesh(const std::string& path);
    Shader* LoadShader(const std::string& vertexPath, 
                       const std::string& fragmentPath);
    
    void UnloadResource(ResourceHandle handle);
    void CleanupUnusedResources();
    
private:
    std::unordered_map<std::string, ResourceHandle> m_cache;
    ResourcePool m_pools;
};
```

## 渲染管线

### Forward Rendering (前向渲染)
- 适用于简单场景和小规模光源
- 实时性好，易实现
- 性能随光源数量线性下降

### Deferred Rendering (延迟渲染)
- 适用于复杂光照场景
- G-Buffer 存储几何信息
- 光照计算在后处理阶段进行
- 支持大量光源

### Hybrid Rendering (混合渲染)
- 结合前向和延迟渲染
- 根据物体类型选择渲染方式
- 优化性能和质量平衡

## 内存管理

### 内存分配策略
1. **栈分配**: 临时小对象
2. **池分配**: 频繁创建/销毁的对象
3. **堆分配**: 长生命周期的大对象
4. **缓存行对齐**: 优化CPU缓存访问

### 内存池设计
```cpp
class MemoryPool {
public:
    void* Allocate(size_t size);
    void Deallocate(void* ptr);
    void Reset();
    
private:
    std::vector<MemoryBlock> m_blocks;
    uint32_t m_currentBlock;
};
```

## 多线程设计

### 渲染线程模型
- **主线程**: 逻辑更新、资源加载
- **渲染线程**: 渲染命令准备、状态设置
- **GPU线程**: 实际渲染执行

### 命令队列
```cpp
class RenderCommand {
    enum Type {
        DRAW_MESH,
        DRAW_SPRITE,
        CLEAR,
        SET_TEXTURE,
        SET_SHADER
    };
    
    void Execute(RenderContext* context);
};

class CommandBuffer {
public:
    void Push(RenderCommand cmd);
    void Flush(RenderContext* context);
    
private:
    std::queue<RenderCommand> m_queue;
    std::mutex m_mutex;
};
```

## 性能优化

### GPU 优化
- 顶点缓冲对象池 (VBO Pooling)
- 实例化渲染 (Instanced Rendering)
- 几何着色器优化
- 计算着色器利用

### CPU 优化
- 空间分割 (BSP/Octree)
- 视锥剔除 (Frustum Culling)
- 遮挡剔除 (Occlusion Culling)
- 批处理 (Batching)
- 裁剪 (Culling)

### 渲染排序
- 按材质排序（减少状态切换）
- 按深度排序（透明度处理）
- 按优先级排序（渲染层级）

## 扩展性设计

### 插件系统
- 渲染效果插件接口
- 资源加载器插件接口
- 后处理插件接口

### 配置系统
- JSON配置文件
- 运行时参数调整
- 热重载功能

---

[返回文档首页](README.md) | [下一篇: API 参考](API_REFERENCE.md)

