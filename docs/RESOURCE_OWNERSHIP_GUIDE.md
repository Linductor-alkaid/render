# 资源所有权和循环引用指南

> **目的**: 防止内存泄漏和循环引用问题  
> **适用范围**: 所有使用智能指针管理资源的代码  
> **最后更新**: 2025-11-01

---

## 📋 目录

1. [核心原则](#核心原则)
2. [禁止的模式](#禁止的模式)
3. [推荐的模式](#推荐的模式)
4. [常见场景](#常见场景)
5. [检测和调试](#检测和调试)

---

## 🎯 核心原则

### 原则1: 单向所有权
**资源之间的引用应该形成有向无环图（DAG），不应该有循环**

```
✅ 正确：单向引用
Material → Texture
  ↓
Shader

❌ 错误：循环引用
Material → Texture
  ↑          ↓
Shader ←─────┘
```

### 原则2: 使用 weak_ptr 打破循环
**当必须有反向引用时，使用 `std::weak_ptr`**

```cpp
// 正确示例
class Parent {
    std::shared_ptr<Child> child;  // 父对象拥有子对象
};

class Child {
    std::weak_ptr<Parent> parent;  // 子对象不拥有父对象，使用weak_ptr
};
```

### 原则3: 清晰的生命周期管理
**明确每个对象的生命周期和所有者**

- **所有者 (Owner)**: 使用 `std::shared_ptr` 或 `std::unique_ptr`
- **观察者 (Observer)**: 使用 `std::weak_ptr` 或裸指针（需要保证安全性）

---

## ❌ 禁止的模式

### 模式1: 双向 shared_ptr 引用

```cpp
// ❌ 错误示例：Material和Texture互相持有shared_ptr
class Material {
    std::shared_ptr<Texture> m_texture;  // Material拥有Texture
    
    void SetTexture(std::shared_ptr<Texture> tex) {
        m_texture = tex;
        tex->SetOwnerMaterial(shared_from_this());  // ❌ 循环引用！
    }
};

class Texture {
    std::shared_ptr<Material> m_ownerMaterial;  // ❌ Texture也拥有Material
};
```

**问题**:
- Material和Texture的引用计数永远不会降到0
- 即使外部没有引用，它们也无法被释放
- 导致内存泄漏

### 模式2: 通过第三方对象的间接循环

```cpp
// ❌ 错误示例：A → B → C → A
std::shared_ptr<MaterialA> matA = CreateRef<MaterialA>();
std::shared_ptr<TextureB> texB = CreateRef<TextureB>();
std::shared_ptr<ShaderC> shaderC = CreateRef<ShaderC>();

matA->SetTexture(texB);
texB->SetShader(shaderC);
shaderC->SetMaterial(matA);  // ❌ 循环引用！
```

### 模式3: 容器中的循环引用

```cpp
// ❌ 错误示例：父子节点互相引用
class SceneNode {
    std::shared_ptr<SceneNode> m_parent;              // ❌ 错误
    std::vector<std::shared_ptr<SceneNode>> m_children;  // 正确
};
```

---

## ✅ 推荐的模式

### 模式1: 单向引用 + weak_ptr

```cpp
// ✅ 正确示例：使用weak_ptr打破循环
class Material {
    std::shared_ptr<Texture> m_texture;  // Material拥有Texture
    
    void SetTexture(std::shared_ptr<Texture> tex) {
        m_texture = tex;
        if (tex) {
            tex->SetOwnerMaterial(weak_from_this());  // ✅ 使用weak_ptr
        }
    }
};

class Texture {
    std::weak_ptr<Material> m_ownerMaterial;  // ✅ Texture不拥有Material
    
    void SetOwnerMaterial(std::weak_ptr<Material> mat) {
        m_ownerMaterial = mat;
    }
    
    std::shared_ptr<Material> GetOwnerMaterial() const {
        return m_ownerMaterial.lock();  // 安全地获取shared_ptr
    }
};
```

### 模式2: 仅保留必要的引用

```cpp
// ✅ 正确示例：Material引用Texture和Shader，但它们不引用Material
class Material {
    std::shared_ptr<Texture> m_diffuseTexture;
    std::shared_ptr<Texture> m_normalTexture;
    std::shared_ptr<Shader> m_shader;
    
    // Material拥有这些资源，但资源不需要知道谁在使用它们
};

class Texture {
    // 不需要知道谁在使用它
};

class Shader {
    // 不需要知道谁在使用它
};
```

### 模式3: 父子关系正确处理

```cpp
// ✅ 正确示例：父节点拥有子节点，子节点只观察父节点
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    void SetParent(std::shared_ptr<SceneNode> parent) {
        m_parent = parent;  // 使用weak_ptr
    }
    
    void AddChild(std::shared_ptr<SceneNode> child) {
        m_children.push_back(child);
        child->SetParent(shared_from_this());
    }
    
    std::shared_ptr<SceneNode> GetParent() const {
        return m_parent.lock();
    }
    
private:
    std::weak_ptr<SceneNode> m_parent;              // ✅ 使用weak_ptr
    std::vector<std::shared_ptr<SceneNode>> m_children;  // 拥有子节点
};
```

---

## 🔍 常见场景

### 场景1: Material和Texture

```cpp
// Material使用Texture
class Material {
    std::shared_ptr<Texture> m_diffuseTexture;
    std::shared_ptr<Texture> m_normalTexture;
    std::shared_ptr<Texture> m_specularTexture;
    
    // Material拥有Texture
    // Texture不需要知道Material
};
```

**理由**: Material依赖Texture存在，但Texture是独立的资源，可以被多个Material共享。

### 场景2: Mesh和Material

```cpp
// Mesh使用Material
class Mesh {
    std::shared_ptr<Material> m_material;
    
    // Mesh拥有Material
    // Material不需要知道Mesh
};
```

**理由**: Mesh依赖Material进行渲染，但Material可以被多个Mesh共享。

### 场景3: Transform父子关系

```cpp
// ✅ 已在Transform类中正确实现
class Transform {
    Transform* m_parent;  // 使用裸指针，由用户保证生命周期
    // 或者使用 std::weak_ptr<Transform> m_parent;
};
```

**注意**: 如果使用裸指针，调用者需要保证父对象的生命周期比子对象长。

### 场景4: Camera和Transform

```cpp
// Camera拥有一个Transform
class Camera {
    Transform m_transform;  // 值语义，Camera拥有Transform
    
    // 不要让Transform反向引用Camera
};
```

---

## 🛠️ 检测和调试

### 方法1: 使用智能指针诊断

```cpp
// 检查引用计数
void DiagnoseReferences() {
    Ref<Material> mat = GetMaterial("test");
    
    std::cout << "Material引用计数: " << mat.use_count() << std::endl;
    
    // 引用计数应该是预期值
    // 如果引用计数异常高，可能存在循环引用
}
```

### 方法2: 使用 ResourceManager 统计

```cpp
// 查看资源数量
ResourceStats stats = ResourceManager::GetInstance().GetStats();
std::cout << "纹理数量: " << stats.textureCount << std::endl;
std::cout << "网格数量: " << stats.meshCount << std::endl;

// 清理未使用的资源
size_t cleaned = ResourceManager::GetInstance().CleanupUnused(60);
std::cout << "清理了 " << cleaned << " 个资源" << std::endl;

// 如果资源数量不减少，可能存在循环引用
```

### 方法3: 使用内存分析工具

**Windows**: 
- Visual Studio Memory Profiler
- Visual Leak Detector (VLD)

**Linux**:
- Valgrind
- AddressSanitizer

```bash
# Linux示例：使用Valgrind检测内存泄漏
valgrind --leak-check=full --show-leak-kinds=all ./RenderEngine
```

### 方法4: 手动循环检测（高级）

如果需要自动检测循环引用，可以实现深度优先搜索：

```cpp
class CircularReferenceDetector {
public:
    bool HasCircularReference(const std::string& resourceName) {
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recursionStack;
        return DetectCycle(resourceName, visited, recursionStack);
    }
    
private:
    bool DetectCycle(const std::string& name,
                     std::unordered_set<std::string>& visited,
                     std::unordered_set<std::string>& recursionStack) {
        if (recursionStack.find(name) != recursionStack.end()) {
            return true;  // 发现循环
        }
        
        if (visited.find(name) != visited.end()) {
            return false;  // 已访问过，无循环
        }
        
        visited.insert(name);
        recursionStack.insert(name);
        
        // 检查此资源的所有依赖
        for (const auto& dep : GetDependencies(name)) {
            if (DetectCycle(dep, visited, recursionStack)) {
                return true;
            }
        }
        
        recursionStack.erase(name);
        return false;
    }
    
    std::vector<std::string> GetDependencies(const std::string& name) {
        // 实现：获取资源的所有依赖项
        // 例如：Material → {Texture, Shader}
        std::vector<std::string> deps;
        // ... 实现细节
        return deps;
    }
};
```

---

## 📝 检查清单

在添加新的资源引用前，请检查：

- [ ] 引用是否会形成循环？
- [ ] 是否可以使用 `weak_ptr` 而不是 `shared_ptr`？
- [ ] 资源的所有权关系是否清晰？
- [ ] 是否添加了必要的生命周期管理？
- [ ] 是否在文档中说明了所有权规则？

---

## ⚠️ 常见错误和解决方案

### 错误1: 在回调中捕获 shared_ptr

```cpp
// ❌ 错误
auto material = CreateRef<Material>();
renderer->SetOnRenderCallback([material]() {
    // 回调持有material的shared_ptr
    material->Render();  // 可能导致material无法释放
});

// ✅ 正确
auto material = CreateRef<Material>();
std::weak_ptr<Material> weakMat = material;
renderer->SetOnRenderCallback([weakMat]() {
    if (auto mat = weakMat.lock()) {
        mat->Render();
    }
});
```

### 错误2: 缓存中持有 shared_ptr

```cpp
// ❌ 错误：缓存持有shared_ptr，资源永不释放
class ResourceCache {
    std::map<std::string, std::shared_ptr<Resource>> m_cache;
};

// ✅ 正确：使用weak_ptr或实现LRU策略
class ResourceCache {
    std::map<std::string, std::weak_ptr<Resource>> m_cache;
    
    std::shared_ptr<Resource> Get(const std::string& name) {
        auto it = m_cache.find(name);
        if (it != m_cache.end()) {
            return it->second.lock();  // 可能返回nullptr
        }
        return nullptr;
    }
};
```

### 错误3: 全局单例持有资源

```cpp
// ❌ 错误：单例持有shared_ptr，程序结束时才释放
class GlobalResourceRegistry {
public:
    static GlobalResourceRegistry& Instance() {
        static GlobalResourceRegistry instance;
        return instance;
    }
    
    void Register(std::shared_ptr<Resource> res) {
        m_resources.push_back(res);  // ❌ 永不释放
    }
    
private:
    std::vector<std::shared_ptr<Resource>> m_resources;
};

// ✅ 正确：使用ResourceManager并主动清理
// 或使用weak_ptr允许资源被释放
```

---

## 📚 参考资料

- [C++ Core Guidelines - R.24: Use std::weak_ptr to break cycles of shared_ptrs](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r24-use-stdweak_ptr-to-break-cycles-of-shared_ptrs)
- [Effective Modern C++ - Item 20: Use std::weak_ptr for std::shared_ptr-like pointers that can dangle](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- 项目文档：[资源管理器设计](./RESOURCE_MANAGER_DESIGN.md)

---

## 🔄 更新日志

| 日期 | 版本 | 更改内容 |
|------|------|----------|
| 2025-11-01 | 1.0 | 初始版本，定义核心原则和常见模式 |

---

**维护者**: Linductor
**联系方式**: 如有疑问，请参考 [CONTRIBUTING.md](./CONTRIBUTING.md)

