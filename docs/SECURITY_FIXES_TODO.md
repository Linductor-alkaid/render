# Core模块安全性修复 TODO 清单

> **生成时间**: 2025-11-01  
> **基于**: Core模块安全性检查报告  
> **状态**: 待修复

---

## 📋 修复优先级说明

- 🔴 **P0 - 严重**: 必须立即修复，存在严重安全隐患
- 🟡 **P1 - 重要**: 建议尽快修复，可能导致问题
- 🟢 **P2 - 改进**: 优化建议，提升代码质量

---

## 🔴 P0 - 严重问题（必须修复）

### 1. Transform类的线程安全问题

**文件**: `src/core/transform.cpp`, `include/render/transform.h`  
**问题描述**: 在`GetWorldPosition()`, `GetWorldRotation()`, `GetWorldScale()`等方法中，读取父指针后释放锁，然后访问父对象，存在数据竞争。

**问题代码** (transform.cpp:48-74):
```cpp
Vector3 Transform::GetWorldPosition() const {
    Transform* parent = nullptr;
    Vector3 localPos;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        parent = m_parent;  // ⚠️ 读取裸指针
        localPos = m_position;
    }
    // 锁释放后使用parent - 数据竞争！
    if (parent) {
        Vector3 parentPos = parent->GetWorldPosition();  // ⚠️ 危险！
    }
}
```

**修复方案A** - 使用 shared_ptr（推荐）:

**步骤**:
1. 修改 `transform.h` (第295行):
   ```cpp
   // 旧代码
   Transform* m_parent;  // 裸指针
   
   // 新代码
   std::weak_ptr<Transform> m_parent;  // 使用 weak_ptr 避免循环引用
   ```

2. 修改 `transform.h` (第244行) SetParent方法签名:
   ```cpp
   // 旧代码
   void SetParent(Transform* parent);
   
   // 新代码
   void SetParent(std::shared_ptr<Transform> parent);
   ```

3. 修改 `transform.cpp` GetWorldPosition实现:
   ```cpp
   Vector3 Transform::GetWorldPosition() const {
       std::shared_ptr<Transform> parent;
       Vector3 localPos;
       {
           std::lock_guard<std::mutex> lock(m_mutex);
           parent = m_parent.lock();  // 从 weak_ptr 获取 shared_ptr
           localPos = m_position;
       }
       // 现在安全了，parent不会被提前销毁
       if (parent) {
           Vector3 parentPos = parent->GetWorldPosition();
           Quaternion parentRot = parent->GetWorldRotation();
           Vector3 parentScale = parent->GetWorldScale();
           
           Vector3 scaledPos(
               localPos.x() * parentScale.x(),
               localPos.y() * parentScale.y(),
               localPos.z() * parentScale.z()
           );
           
           return parentPos + parentRot * scaledPos;
       }
       return localPos;
   }
   ```

4. 同样修改以下方法:
   - `GetWorldRotation()` (transform.cpp:138-154)
   - `GetWorldScale()` (transform.cpp:255-276)
   - `TranslateWorld()` (transform.cpp:82-104)
   - `RotateAroundWorld()` (transform.cpp:169-202)
   - `LookAt()` (transform.cpp:204-237)
   - `GetWorldMatrix()` (transform.cpp:316-333)

**修复方案B** - 使用递归锁（备选）:

如果不想改变指针类型，可以使用递归锁：

1. 修改 `transform.h` (第311行):
   ```cpp
   // 旧代码
   mutable std::mutex m_mutex;
   
   // 新代码
   mutable std::recursive_mutex m_mutex;
   ```

2. 修改 `transform.cpp` GetWorldPosition实现，持锁访问父对象:
   ```cpp
   Vector3 Transform::GetWorldPosition() const {
       std::lock_guard<std::recursive_mutex> lock(m_mutex);
       
       if (m_parent) {
           // 递归调用，父对象有自己的递归锁
           Vector3 parentPos = m_parent->GetWorldPosition();
           Quaternion parentRot = m_parent->GetWorldRotation();
           Vector3 parentScale = m_parent->GetWorldScale();
           
           Vector3 scaledPos(
               m_position.x() * parentScale.x(),
               m_position.y() * parentScale.y(),
               m_position.z() * parentScale.z()
           );
           
           return parentPos + parentRot * scaledPos;
       }
       return m_position;
   }
   ```

**推荐**: 方案A（shared_ptr），更安全且避免循环引用

**影响范围**:
- `transform.h` - 成员变量类型修改
- `transform.cpp` - 8个方法需要修改
- `camera.h/cpp` - Camera类使用Transform，需要适配
- 所有使用Transform的代码

**估计工作量**: 4-6小时

---

### 2. ResourceManager::ForEach方法死锁风险

**文件**: `src/core/resource_manager.cpp`, `include/render/resource_manager.h`  
**问题描述**: `ForEach`系列方法在持锁状态下调用用户回调，如果回调中调用ResourceManager的其他方法会死锁。

**问题代码** (resource_manager.cpp:618-624):
```cpp
void ResourceManager::ForEachTexture(
    std::function<void(const std::string&, Ref<Texture>)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);  // 持有锁
    
    for (const auto& [name, entry] : m_textures) {
        callback(name, entry.resource);  // ⚠️ 回调可能再次获取m_mutex！
    }
}
```

**死锁场景**:
```cpp
// 用户代码
manager.ForEachTexture([&](const std::string& name, Ref<Texture> tex) {
    manager.RemoveTexture(name);  // ❌ 死锁！尝试再次获取m_mutex
});
```

**修复方案** - 快照模式:

修改 `resource_manager.cpp` 中的4个ForEach方法:

```cpp
void ResourceManager::ForEachTexture(
    std::function<void(const std::string&, Ref<Texture>)> callback) {
    // 步骤1: 创建快照（持锁）
    std::vector<std::pair<std::string, Ref<Texture>>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot.reserve(m_textures.size());
        for (const auto& [name, entry] : m_textures) {
            snapshot.emplace_back(name, entry.resource);
        }
    }  // 锁释放
    
    // 步骤2: 无锁调用回调
    for (const auto& [name, resource] : snapshot) {
        if (callback) {
            callback(name, resource);
        }
    }
}
```

**需要修改的方法**:
1. `ForEachTexture()` (resource_manager.cpp:618-624)
2. `ForEachMesh()` (resource_manager.cpp:626-632)
3. `ForEachMaterial()` (resource_manager.cpp:634-640)
4. `ForEachShader()` (resource_manager.cpp:642-648)

**注意事项**:
- 快照模式会增加内存开销（临时复制shared_ptr）
- 但避免了死锁，是线程安全的标准做法
- 回调看到的是快照时刻的数据，不是实时数据

**影响范围**:
- `resource_manager.cpp` - 4个方法
- 已有文档警告，修复后可移除警告

**估计工作量**: 1-2小时

---

## 🟡 P1 - 重要问题（建议修复）

### 3. Transform::m_parent悬空指针风险

**文件**: `include/render/transform.h`, `src/core/transform.cpp`  
**问题描述**: 如果父Transform对象被销毁，子对象的`m_parent`指针会变成悬空指针。

**问题场景**:
```cpp
Transform* child = new Transform();
{
    Transform* parent = new Transform();
    child->SetParent(parent);
    delete parent;  // ⚠️ parent被销毁
}
// child->m_parent 现在是悬空指针！
child->GetWorldPosition();  // ❌ 未定义行为
```

**修复方案** - 在P0-1修复时一并解决:

如果采用P0-1的方案A（shared_ptr），此问题自动解决。

**额外建议** - 添加子对象列表:

在 `transform.h` 中添加：
```cpp
private:
    std::vector<Transform*> m_children;  // 或 std::vector<std::weak_ptr<Transform>>
    
    // 在SetParent时维护
    void SetParent(std::shared_ptr<Transform> parent) {
        // 从旧父对象移除
        if (auto oldParent = m_parent.lock()) {
            oldParent->RemoveChild(this);
        }
        
        // 添加到新父对象
        m_parent = parent;
        if (parent) {
            parent->AddChild(this);
        }
    }
```

在析构函数中清理：
```cpp
~Transform() {
    // 通知所有子对象
    for (auto* child : m_children) {
        if (child) {
            child->m_parent.reset();
        }
    }
}
```

**影响范围**:
- 与P0-1一起修复
- 如果添加子对象列表，需要额外工作

**估计工作量**: 包含在P0-1中 (或额外2-3小时如果添加子对象管理)

---

### 4. 潜在的循环引用内存泄漏

**文件**: `src/core/resource_manager.cpp`, `include/render/resource_manager.h`  
**问题描述**: 资源之间可能存在循环引用（如Material引用Texture，反之亦然），导致内存泄漏。

**问题场景**:
```cpp
Ref<Material> mat = CreateRef<Material>();
Ref<Texture> tex = CreateRef<Texture>();
mat->SetTexture(tex);
tex->SetUserData(mat);  // 循环引用！
// mat和tex的引用计数永远不会降到0
```

**修复方案A** - 文档约束（最简单）:

在 `docs/DEVELOPMENT_GUIDE.md` 中添加：

```markdown
## 资源所有权规则

### 禁止的模式
❌ **禁止**: 资源之间的循环引用
```cpp
// 错误示例
material->SetTexture(texture);
texture->SetMaterial(material);  // 循环引用！
```

### 推荐的模式
✅ **推荐**: 单向引用
```cpp
// 正确示例：Material引用Texture，但Texture不引用Material
material->SetTexture(texture);
```

✅ **推荐**: 使用weak_ptr打破循环
```cpp
class Texture {
    std::weak_ptr<Material> m_ownerMaterial;  // 使用weak_ptr
};
```
```

**修复方案B** - 添加循环检测（更安全）:

在 `resource_manager.h` 中添加：

```cpp
class ResourceManager {
public:
    /**
     * @brief 检测资源依赖图中的循环引用
     * @return 发现的循环引用列表
     */
    std::vector<std::string> DetectCircularReferences();
    
    /**
     * @brief 打印资源依赖关系
     */
    void PrintDependencyGraph();
    
private:
    // 递归检测循环
    bool HasCycle(const std::string& resourceName, 
                  std::unordered_set<std::string>& visited,
                  std::unordered_set<std::string>& recursionStack);
};
```

在 `resource_manager.cpp` 中实现：

```cpp
std::vector<std::string> ResourceManager::DetectCircularReferences() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cycles;
    
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;
    
    // 检查所有资源
    for (const auto& [name, entry] : m_materials) {
        if (HasCycle(name, visited, recursionStack)) {
            cycles.push_back(name);
        }
    }
    
    return cycles;
}

bool ResourceManager::HasCycle(const std::string& resourceName,
                               std::unordered_set<std::string>& visited,
                               std::unordered_set<std::string>& recursionStack) {
    if (recursionStack.find(resourceName) != recursionStack.end()) {
        return true;  // 发现循环
    }
    
    if (visited.find(resourceName) != visited.end()) {
        return false;  // 已访问过，无循环
    }
    
    visited.insert(resourceName);
    recursionStack.insert(resourceName);
    
    // 检查此资源的依赖
    // TODO: 实现具体的依赖关系检查
    
    recursionStack.erase(resourceName);
    return false;
}
```

**推荐**: 先采用方案A（文档约束），如有需要再实施方案B

**影响范围**:
- 方案A: 仅文档
- 方案B: resource_manager.h/cpp，需要深入理解资源依赖

**估计工作量**: 
- 方案A: 0.5小时
- 方案B: 6-8小时

---

### 5. CameraController空指针检查

**文件**: `include/render/camera.h`, `src/core/camera.cpp`  
**问题描述**: `CameraController`及其派生类接受Camera裸指针，但构造时不检查nullptr。

**问题代码** (camera.h:361):
```cpp
CameraController(Camera* camera) : m_camera(camera) {}
// 没有检查camera是否为nullptr
```

**修复方案**:

修改 `camera.h` 中的所有CameraController构造函数：

```cpp
// 基类
class CameraController {
public:
    CameraController(Camera* camera) : m_camera(camera) {
        if (!camera) {
            throw std::invalid_argument("Camera cannot be null");
        }
    }
    // ...
};

// FirstPersonCameraController
FirstPersonCameraController::FirstPersonCameraController(Camera* camera)
    : CameraController(camera)  // 基类会检查
{
    // 从相机当前旋转初始化偏航和俯仰角
    Vector3 forward = camera->GetForward();  // 现在安全
    // ...
}

// 其他派生类同样修改
```

**或者使用引用**（更好的方案）:

```cpp
class CameraController {
public:
    CameraController(Camera& camera) : m_camera(&camera) {}
    // 引用不能为nullptr，在类型层面保证安全
    
private:
    Camera* m_camera;  // 内部仍可使用指针
};
```

**推荐**: 使用引用参数

**影响范围**:
- `camera.h` - CameraController及3个派生类
- `camera.cpp` - 构造函数实现
- 使用CameraController的代码需要适配

**估计工作量**: 1-2小时

---

## 🟢 P2 - 改进建议（优化）

### 6. 添加RAII资源清理辅助类

**文件**: 新建 `include/render/scope_guard.h`  
**目的**: 提供自动清理机制，防止异常时的资源泄漏

**实现**:

```cpp
#pragma once

#include <functional>

namespace Render {

/**
 * @brief RAII风格的作用域守卫
 * 
 * 用于确保资源清理代码一定执行，即使发生异常
 */
class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> cleanup)
        : m_cleanup(std::move(cleanup))
        , m_active(true) {}
    
    ~ScopeGuard() {
        if (m_active && m_cleanup) {
            m_cleanup();
        }
    }
    
    // 禁止拷贝
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
    // 允许移动
    ScopeGuard(ScopeGuard&& other) noexcept
        : m_cleanup(std::move(other.m_cleanup))
        , m_active(other.m_active) {
        other.m_active = false;
    }
    
    // 取消清理
    void Dismiss() { m_active = false; }
    
private:
    std::function<void()> m_cleanup;
    bool m_active;
};

// 便捷宏
#define SCOPE_EXIT(code) \
    Render::ScopeGuard CONCAT(_scope_guard_, __LINE__)([&]() { code; })

} // namespace Render
```

**使用示例**:

```cpp
void SomeFunction() {
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    
    SCOPE_EXIT({
        if (buffer) {
            glDeleteBuffers(1, &buffer);
        }
    });
    
    // 即使下面的代码抛出异常，buffer也会被正确清理
    DoSomethingThatMightThrow();
}
```

**影响范围**: 新文件，不影响现有代码

**估计工作量**: 2小时

---

### 7. 改进错误处理机制

**文件**: `include/render/error.h`  
**目的**: 添加更多错误上下文信息

**改进建议**:

```cpp
// 添加错误堆栈跟踪
class RenderError {
public:
    void AddStackFrame(const char* file, int line, const char* function) {
        m_stackTrace.push_back({file, line, function});
    }
    
    std::string GetStackTrace() const {
        std::stringstream ss;
        for (const auto& frame : m_stackTrace) {
            ss << frame.file << ":" << frame.line 
               << " in " << frame.function << "\n";
        }
        return ss.str();
    }
    
private:
    struct StackFrame {
        const char* file;
        int line;
        const char* function;
    };
    std::vector<StackFrame> m_stackTrace;
};
```

**影响范围**: error.h，可选择性使用

**估计工作量**: 3-4小时

---

### 8. 添加线程安全性测试

**文件**: 新建 `tests/thread_safety_tests.cpp`  
**目的**: 自动化测试多线程场景

**测试用例**:

```cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(ThreadSafety, TransformConcurrentAccess) {
    auto transform = std::make_shared<Render::Transform>();
    
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // 启动多个线程同时访问
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            try {
                for (int j = 0; j < 1000; ++j) {
                    if (i % 2 == 0) {
                        transform->SetPosition(Render::Vector3(j, j, j));
                    } else {
                        auto pos = transform->GetWorldPosition();
                    }
                }
            } catch (...) {
                errors++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(errors, 0);
}

TEST(ThreadSafety, ResourceManagerConcurrentCleanup) {
    // 测试并发清理
    // ...
}
```

**影响范围**: 新文件

**估计工作量**: 4-6小时

---

## 📊 修复进度跟踪

| ID | 问题 | 优先级 | 状态 | 负责人 | 预计完成 |
|----|------|--------|------|--------|----------|
| P0-1 | Transform线程安全 | 🔴 严重 | ⏳ 待修复 | - | - |
| P0-2 | ForEach死锁 | 🔴 严重 | ⏳ 待修复 | - | - |
| P1-3 | m_parent悬空指针 | 🟡 重要 | ⏳ 待修复 | - | - |
| P1-4 | 循环引用检测 | 🟡 重要 | ⏳ 待修复 | - | - |
| P1-5 | CameraController空指针 | 🟡 重要 | ⏳ 待修复 | - | - |
| P2-6 | RAII辅助类 | 🟢 改进 | ⏳ 待实现 | - | - |
| P2-7 | 错误处理改进 | 🟢 改进 | ⏳ 待实现 | - | - |
| P2-8 | 线程安全测试 | 🟢 改进 | ⏳ 待实现 | - | - |

**状态说明**:
- ⏳ 待修复
- 🚧 进行中
- ✅ 已完成
- ❌ 已取消

---

## 🔄 修复流程建议

### 第一阶段：严重问题修复（1周）
1. 修复P0-1：Transform线程安全（2天）
2. 修复P0-2：ForEach死锁（1天）
3. 测试和验证（2天）
4. 代码审查（1天）
5. 合并主分支（1天）

### 第二阶段：重要问题修复（1周）
1. 修复P1-3：悬空指针（与P0-1一起）
2. 修复P1-4：循环引用文档（1天）
3. 修复P1-5：空指针检查（1天）
4. 测试和验证（2天）
5. 代码审查和合并（1天）

### 第三阶段：改进实施（2周，可选）
1. 实现P2-6：RAII辅助（1周）
2. 实现P2-7、P2-8：其他改进（1周）

---

## 📝 测试计划

### 单元测试
- [ ] Transform并发访问测试
- [ ] ResourceManager ForEach回调测试
- [ ] CameraController空指针测试
- [ ] 循环引用检测测试

### 集成测试
- [ ] 多线程场景下的资源管理
- [ ] 父子Transform层级并发访问
- [ ] 异常情况下的资源清理

### 压力测试
- [ ] 大量资源并发加载/卸载
- [ ] 深层Transform树结构访问
- [ ] 长时间运行内存泄漏检测

---

## 📚 相关文档

- [安全性检查报告](./SECURITY_OPTIMIZATION_SUMMARY.md)
- [线程安全指南](./THREAD_SAFETY.md)
- [开发指南](./DEVELOPMENT_GUIDE.md)

---

## 💡 注意事项

1. **向后兼容性**：某些修复可能破坏API兼容性，需要更新所有使用代码
2. **性能影响**：快照模式会增加内存开销，需要权衡
3. **测试覆盖**：每个修复都必须有对应的测试用例
4. **文档更新**：修复后及时更新相关文档
5. **Code Review**：严重问题修复必须经过至少2人审查

---

**最后更新**: 2025-11-01  
**维护者**: RenderEngine开发团队

