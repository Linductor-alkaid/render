# 抽象基类开发指南

> **目标**: 安全地引入抽象基类而不导致崩溃  
> **前提**: 完成 FIX_AND_OPTIMIZATION_PLAN.md 中的 P0 任务

---

## ⚠️ 为什么之前崩溃？

基于代码评估，崩溃的主要原因：

### 原因 1: 生命周期管理问题 (最可能)

```cpp
// 假设您的抽象基类设计
class IRenderable {
public:
    virtual ~IRenderable() = default;
    virtual void Render() = 0;
};

class MeshRenderable : public IRenderable {
private:
    OpenGLContext* m_context;  // ❌ 从 Renderer::GetContext() 获取的裸指针
    
public:
    MeshRenderable(Renderer* renderer) {
        m_context = renderer->GetContext();  // 获取裸指针
    }
    
    void Render() override {
        m_context->SwapBuffers();  // ❌ 如果 renderer 已 Shutdown，崩溃！
    }
};
```

**崩溃场景**:
1. 创建 `MeshRenderable` 实例，保存 `m_context` 裸指针
2. 在另一个线程或之后调用 `Renderer::Shutdown()`
3. `m_context` 指向的内存被释放
4. 调用 `MeshRenderable::Render()` 时访问野指针 → 崩溃

---

### 原因 2: 虚函数表和多线程

```cpp
// 线程 A
IRenderable* renderable = GetRenderable();
renderable->Render();  // 访问虚函数表

// 线程 B（同时）
delete renderable;  // 销毁对象，虚函数表失效

// 线程 A 的虚函数调用崩溃
```

---

### 原因 3: 移动语义和虚函数

```cpp
std::vector<std::unique_ptr<IRenderable>> renderables;

// 添加对象
renderables.push_back(std::make_unique<MeshRenderable>());

// vector 扩容时移动对象
// ❌ 移动构造函数可能调用 OpenGL，但没有线程检查
```

---

## ✅ 安全的抽象基类设计

### 方案 A: ECS（推荐）

**不使用继承，使用组合**

```cpp
// 组件（纯数据）
struct RenderableComponent {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible = true;
};

struct TransformComponent {
    Vector3 position = Vector3::Zero();
    Quaternion rotation = Quaternion::Identity();
    Vector3 scale = Vector3::Ones();
    
    Matrix4 GetMatrix() const {
        // ... 计算变换矩阵
    }
};

// 实体（组件容器）
class Entity {
public:
    template<typename T>
    void AddComponent(T component) {
        // 使用 type_index 存储组件
    }
    
    template<typename T>
    T* GetComponent() {
        // 获取组件
    }
    
private:
    std::unordered_map<std::type_index, std::any> m_components;
};

// 系统（处理逻辑）
class RenderSystem {
public:
    void Update(std::vector<Entity*>& entities, Camera& camera) {
        for (auto* entity : entities) {
            auto* renderable = entity->GetComponent<RenderableComponent>();
            auto* transform = entity->GetComponent<TransformComponent>();
            
            if (renderable && transform && renderable->visible) {
                // 提交到批量渲染器
                DrawCall call;
                call.mesh = renderable->mesh;
                call.material = renderable->material;
                call.transform = transform->GetMatrix();
                m_batchRenderer.Submit(call);
            }
        }
        
        m_batchRenderer.Render(camera);
        m_batchRenderer.Clear();
    }
    
private:
    BatchRenderer m_batchRenderer;
};
```

**优势**:
- ✅ 无虚函数，无虚函数表问题
- ✅ 组件可以在线程间安全移动
- ✅ 数据和逻辑分离，易于优化
- ✅ 灵活组合，无需多重继承

**使用示例**:
```cpp
// 创建场景
Scene scene;
RenderSystem renderSystem;

// 创建实体
Entity cube;
cube.AddComponent(RenderableComponent{
    .mesh = resourceManager.GetMesh("cube"),
    .material = resourceManager.GetMaterial("pbr")
});
cube.AddComponent(TransformComponent{
    .position = Vector3(0, 0, 0)
});

scene.AddEntity(&cube);

// 渲染循环
while (running) {
    renderSystem.Update(scene.GetEntities(), camera);
}
```

---

### 方案 B: 安全的继承（如果必须使用）

**1. 使用 shared_ptr 管理**

```cpp
class IRenderable : public std::enable_shared_from_this<IRenderable> {
public:
    virtual ~IRenderable() = default;
    
    // ✅ 传递引用而非保存指针
    virtual void Render(Renderer& renderer, Camera& camera) = 0;
    
    // 禁止拷贝和移动（避免虚函数表问题）
    IRenderable(const IRenderable&) = delete;
    IRenderable& operator=(const IRenderable&) = delete;
    IRenderable(IRenderable&&) = delete;
    IRenderable& operator=(IRenderable&&) = delete;
};

class MeshRenderable : public IRenderable {
public:
    MeshRenderable(std::shared_ptr<Mesh> mesh, 
                   std::shared_ptr<Material> material)
        : m_mesh(std::move(mesh))
        , m_material(std::move(material)) {
    }
    
    void Render(Renderer& renderer, Camera& camera) override {
        // ✅ 每次都从 renderer 获取，不保存指针
        auto renderState = renderer.GetRenderState();
        if (renderState) {
            m_material->Bind(renderState.get());
            m_mesh->Draw();
        }
    }
    
private:
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Material> m_material;
};

// 管理类
class RenderableManager {
public:
    void Add(std::shared_ptr<IRenderable> renderable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_renderables.push_back(std::move(renderable));
    }
    
    void Remove(std::shared_ptr<IRenderable> renderable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 不立即删除，添加到待删除列表
        m_toDelete.push_back(renderable);
    }
    
    void RenderAll(Renderer& renderer, Camera& camera) {
        // 获取当前渲染列表的副本（避免长时间持锁）
        std::vector<std::shared_ptr<IRenderable>> localList;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            localList = m_renderables;
        }
        
        // 渲染（在锁外）
        for (auto& renderable : localList) {
            if (renderable) {
                renderable->Render(renderer, camera);
            }
        }
    }
    
    void EndFrame() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 清理待删除对象
        for (auto& toDelete : m_toDelete) {
            m_renderables.erase(
                std::remove(m_renderables.begin(), m_renderables.end(), toDelete),
                m_renderables.end()
            );
        }
        m_toDelete.clear();
    }
    
private:
    std::vector<std::shared_ptr<IRenderable>> m_renderables;
    std::vector<std::shared_ptr<IRenderable>> m_toDelete;
    std::mutex m_mutex;
};
```

**使用示例**:
```cpp
RenderableManager manager;
Renderer renderer;
Camera camera;

// 创建对象
auto mesh = std::make_shared<MeshRenderable>(
    resourceManager.GetMesh("cube"),
    resourceManager.GetMaterial("pbr")
);

manager.Add(mesh);

// 渲染循环
while (running) {
    renderer.BeginFrame();
    manager.RenderAll(renderer, camera);
    manager.EndFrame();  // 清理待删除对象
    renderer.EndFrame();
}
```

---

### 方案 C: 类型擦除（type erasure）

**使用 std::function 或类似技术**

```cpp
// 无需继承
class Renderable {
public:
    using RenderFunc = std::function<void(Renderer&, Camera&)>;
    
    Renderable(RenderFunc func) : m_renderFunc(std::move(func)) {}
    
    void Render(Renderer& renderer, Camera& camera) {
        if (m_renderFunc) {
            m_renderFunc(renderer, camera);
        }
    }
    
private:
    RenderFunc m_renderFunc;
};

// 创建对象
Renderable CreateMeshRenderable(std::shared_ptr<Mesh> mesh,
                                std::shared_ptr<Material> material) {
    return Renderable([mesh, material](Renderer& renderer, Camera& camera) {
        auto renderState = renderer.GetRenderState();
        if (renderState) {
            material->Bind(renderState.get());
            mesh->Draw();
        }
    });
}

// 使用
std::vector<Renderable> renderables;
renderables.push_back(CreateMeshRenderable(mesh, material));

// 渲染
for (auto& r : renderables) {
    r.Render(renderer, camera);
}
```

**优势**:
- ✅ 无虚函数
- ✅ 类型安全
- ✅ 可以捕获任意数据

**劣势**:
- ⚠️ std::function 有性能开销
- ⚠️ 不能直接访问捕获的数据

---

## 🚫 错误示范（避免）

### ❌ 错误 1: 保存裸指针

```cpp
class BadRenderable : public IRenderable {
    Renderer* m_renderer;  // ❌ 裸指针
    
public:
    BadRenderable(Renderer* r) : m_renderer(r) {}
    
    void Render() override {
        m_renderer->Clear();  // ❌ 可能野指针
    }
};
```

### ❌ 错误 2: 在析构函数中调用虚函数

```cpp
class BadRenderable : public IRenderable {
public:
    ~BadRenderable() override {
        Cleanup();  // ❌ 虚函数在析构中调用有问题
    }
    
    virtual void Cleanup() {
        // ...
    }
};
```

### ❌ 错误 3: 没有禁止拷贝/移动

```cpp
class BadRenderable : public IRenderable {
    // ❌ 默认的拷贝/移动构造可能导致问题
};

std::vector<BadRenderable> list;
list.push_back(BadRenderable());  // 可能崩溃
```

---

## 🧪 测试策略

### 测试 1: 生命周期测试

```cpp
TEST(RenderableTest, LifecycleManagement) {
    auto renderer = std::make_shared<Renderer>();
    renderer->Initialize();
    
    auto manager = std::make_shared<RenderableManager>();
    
    // 创建对象
    auto renderable = std::make_shared<MeshRenderable>(mesh, material);
    manager->Add(renderable);
    
    // 渲染
    manager->RenderAll(*renderer, camera);
    
    // 删除渲染器
    renderer->Shutdown();
    renderer.reset();
    
    // ✅ 此时 renderable 仍存在，但不应崩溃
    // 因为它不保存 renderer 的裸指针
}
```

### 测试 2: 多线程压力测试

```cpp
TEST(RenderableTest, MultiThreadStress) {
    RenderableManager manager;
    Renderer renderer;
    renderer.Initialize();
    
    // 线程 1: 添加对象
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            auto r = std::make_shared<MeshRenderable>(mesh, material);
            manager.Add(r);
        }
    });
    
    // 线程 2: 删除对象
    std::thread t2([&]() {
        // 随机删除
    });
    
    // 主线程: 渲染
    for (int i = 0; i < 100; ++i) {
        manager.RenderAll(renderer, camera);
        manager.EndFrame();
    }
    
    t1.join();
    t2.join();
    
    // ✅ 不应有任何崩溃或竞态
}
```

### 测试 3: 资源释放顺序测试

```cpp
TEST(RenderableTest, DestructionOrder) {
    {
        auto renderer = std::make_shared<Renderer>();
        renderer->Initialize();
        
        auto manager = std::make_shared<RenderableManager>();
        
        // 添加多个对象
        for (int i = 0; i < 100; ++i) {
            manager->Add(std::make_shared<MeshRenderable>(mesh, material));
        }
        
        // 随机顺序释放
        if (rand() % 2) {
            renderer.reset();  // 先释放 renderer
            manager.reset();   // 后释放 manager
        } else {
            manager.reset();   // 先释放 manager
            renderer.reset();  // 后释放 renderer
        }
    }
    
    // ✅ 任意顺序都不应崩溃
}
```

---

## 📋 实施检查清单

在开始抽象基类开发前，确保：

- [ ] 已完成 P0 任务修复（Renderer 不再返回裸指针）
- [ ] 已阅读 RESOURCE_LIFETIME.md
- [ ] 已决定使用哪种方案（推荐 ECS）
- [ ] 已编写基本测试用例
- [ ] 已启用 ASAN/TSAN（如果可用）

开发过程中：

- [ ] 禁止在抽象基类中保存裸指针
- [ ] 所有资源使用 shared_ptr 管理
- [ ] 禁止在析构函数中调用虚函数
- [ ] 禁止默认的拷贝/移动构造
- [ ] 每次修改后运行测试

完成后：

- [ ] 所有测试通过
- [ ] 无内存泄漏（ASAN）
- [ ] 无线程错误（TSAN）
- [ ] 压力测试 24 小时无崩溃
- [ ] 更新文档

---

## 🎯 推荐实施路线

### 阶段 1: 修复基础问题（1-2 天）

1. 修复 `Renderer::GetContext()` 等方法
2. 修复移动操作的线程检查
3. 运行现有测试确保无回归

### 阶段 2: 实现 ECS 框架（3-5 天）

1. 实现 `Entity` 和 `Component` 基础类
2. 实现 `RenderSystem`
3. 编写单元测试
4. 迁移一个简单的渲染对象到 ECS

### 阶段 3: 逐步迁移（1-2 周）

1. 迁移所有渲染对象到 ECS
2. 删除旧的继承体系（如果有）
3. 优化性能（批量渲染等）
4. 完善文档

### 阶段 4: 稳定性测试（1 周）

1. 多线程压力测试
2. 长时间运行测试
3. 内存泄漏检测
4. 性能对比

---

## 📚 参考资料

### 推荐阅读

1. **C++ Core Guidelines**  
   https://isocpp.github.io/CppCoreGuidelines/
   
2. **Effective Modern C++** by Scott Meyers  
   特别是关于智能指针和移动语义的章节
   
3. **Game Programming Patterns** by Robert Nystrom  
   特别是 Component 和 Object Pool 模式
   
4. **Data-Oriented Design**  
   https://www.dataorienteddesign.com/

### 开源 ECS 实现参考

1. **EnTT** - https://github.com/skypjack/entt  
   现代 C++ ECS 库，性能极高
   
2. **Flecs** - https://github.com/SanderMertens/flecs  
   功能完整的 ECS 框架

---

## 💡 总结

### 核心原则

1. **不保存裸指针** - 永远使用 shared_ptr
2. **不信任生命周期** - 总是假设对象可能已失效
3. **显式优于隐式** - 明确传递依赖，不隐式保存
4. **组合优于继承** - 优先使用 ECS 而非虚函数

### 最后的忠告

如果您仍然决定使用传统的继承：

1. **必须**使用 `std::enable_shared_from_this`
2. **必须**禁止拷贝和移动
3. **必须**使用延迟删除
4. **必须**进行充分的测试

但我强烈建议：**使用 ECS**。它更安全、更快、更灵活，也是现代游戏引擎的标准做法。

---

祝您开发顺利！如有疑问，请参考 CODE_EVALUATION_REPORT.md 中的详细分析。

