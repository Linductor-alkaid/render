# Transform 统一错误处理使用指南

## 📖 概述

Transform 类现在集成了项目统一的错误处理系统，提供两种错误处理方式：

1. **静默失败**（兼容旧代码）：`SetPosition()`, `SetRotation()`, 等
2. **显式错误检查**（推荐新代码）：`TrySetPosition()`, `TrySetRotation()`, 等

---

## ✨ 新增 Transform 错误码

### 错误类别
```cpp
ErrorCategory::Transform = 7000
```

### 错误码列表

| 错误码 | 名称 | 说明 |
|--------|------|------|
| `TransformCircularReference` | 循环引用 | 尝试创建循环父子关系 |
| `TransformSelfReference` | 自引用 | 尝试将自己设为父对象 |
| `TransformHierarchyTooDeep` | 层级过深 | 父对象层级超过 1000 层 |
| `TransformParentDestroyed` | 父对象已销毁 | 父对象已被销毁 |
| `TransformObjectDestroyed` | 对象已销毁 | 当前对象已被销毁 |
| `TransformInvalidPosition` | 无效位置 | 位置包含 NaN/Inf |
| `TransformInvalidRotation` | 无效旋转 | 旋转四元数无效 |
| `TransformInvalidScale` | 无效缩放 | 缩放值无效（过小/过大/NaN） |
| `TransformInvalidMatrix` | 无效矩阵 | 矩阵包含 NaN/Inf |
| `TransformLockTimeout` | 锁超时 | 获取锁超时（预留） |

---

## 🎯 Result 返回类型

### 定义
```cpp
struct Transform::Result {
    ErrorCode code;         // 错误码
    std::string message;    // 错误消息
    
    // 检查方法
    explicit operator bool() const;  // 可用于 if 判断
    bool Ok() const;                 // 操作是否成功
    bool Failed() const;             // 操作是否失败
    
    // 静态工厂方法
    static Result Success();
    static Result Failure(ErrorCode code, const std::string& msg);
};
```

---

## 📝 使用示例

### 1. 基础用法 - 静默失败（兼容旧代码）

```cpp
#include "render/transform.h"

void OldStyleCode() {
    Transform transform;
    
    // 旧代码继续工作，错误会自动记录到日志
    transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    transform.SetRotation(Quaternion::Identity());
    transform.SetScale(Vector3::Ones());
    
    Transform parent;
    bool success = transform.SetParent(&parent);  // 返回 bool
    if (!success) {
        // 失败处理（但不知道具体原因）
    }
}
```

### 2. 显式错误检查（推荐）

```cpp
#include "render/transform.h"
#include <iostream>

void NewStyleCode() {
    Transform transform;
    
    // 方式 1: 简单判断
    auto result = transform.TrySetPosition(Vector3(1.0f, 2.0f, 3.0f));
    if (result.Ok()) {
        std::cout << "位置设置成功" << std::endl;
    } else {
        std::cerr << "位置设置失败: " << result.message << std::endl;
    }
    
    // 方式 2: 使用 operator bool
    if (auto result = transform.TrySetRotation(Quaternion::Identity())) {
        // 成功
    } else {
        // 失败，result.message 包含详细信息
        std::cerr << "错误: " << result.message << std::endl;
    }
}
```

### 3. 详细错误处理

```cpp
void DetailedErrorHandling() {
    Transform parent, child;
    
    auto result = child.TrySetParent(&parent);
    
    if (result.Failed()) {
        // 根据错误码进行不同处理
        switch (result.code) {
            case ErrorCode::TransformCircularReference:
                std::cerr << "循环引用: " << result.message << std::endl;
                // 可能需要重构层级结构
                break;
                
            case ErrorCode::TransformSelfReference:
                std::cerr << "自引用: " << result.message << std::endl;
                // 这是编程错误，需要修复
                break;
                
            case ErrorCode::TransformHierarchyTooDeep:
                std::cerr << "层级过深: " << result.message << std::endl;
                // 可能需要扁平化层级
                break;
                
            case ErrorCode::TransformParentDestroyed:
                std::cerr << "父对象已销毁: " << result.message << std::endl;
                // 生命周期管理问题
                break;
                
            default:
                std::cerr << "未知错误: " << result.message << std::endl;
                break;
        }
    } else {
        std::cout << "设置父对象成功" << std::endl;
    }
}
```

### 4. 数值验证

```cpp
void ValidateTransformValues() {
    Transform transform;
    
    // 测试无效位置（NaN）
    auto result1 = transform.TrySetPosition(Vector3(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f));
    assert(result1.code == ErrorCode::TransformInvalidPosition);
    
    // 测试无效旋转（零四元数）
    auto result2 = transform.TrySetRotation(Quaternion(0, 0, 0, 0));
    assert(result2.code == ErrorCode::TransformInvalidRotation);
    
    // 测试无效缩放（过小）
    auto result3 = transform.TrySetScale(Vector3(1e-10f, 1.0f, 1.0f));
    assert(result3.code == ErrorCode::TransformInvalidScale);
    
    // 测试无效缩放（过大）
    auto result4 = transform.TrySetScale(Vector3(1e10f, 1.0f, 1.0f));
    assert(result4.code == ErrorCode::TransformInvalidScale);
}
```

### 5. 矩阵分解

```cpp
void MatrixDecomposition() {
    Transform transform;
    
    Matrix4 matrix = Matrix4::Identity();
    // 构造一个有效的变换矩阵
    
    auto result = transform.TrySetFromMatrix(matrix);
    if (result.Ok()) {
        std::cout << "矩阵分解成功" << std::endl;
    } else {
        std::cerr << "矩阵分解失败: " << result.message << std::endl;
        // 可能的原因：矩阵包含 NaN/Inf，或分解后的值无效
    }
}
```

### 6. 循环引用检测

```cpp
void CircularReferenceDetection() {
    Transform a, b, c;
    
    // 创建链: a -> b -> c
    assert(b.TrySetParent(&a).Ok());
    assert(c.TrySetParent(&b).Ok());
    
    // 尝试创建循环: c -> b -> a -> c
    auto result = a.TrySetParent(&c);
    assert(!result.Ok());
    assert(result.code == ErrorCode::TransformCircularReference);
    std::cout << "成功检测到循环引用: " << result.message << std::endl;
}
```

---

## 🔧 错误处理回调

### 全局错误监听

```cpp
#include "render/error.h"

void SetupErrorCallback() {
    auto& errorHandler = ErrorHandler::GetInstance();
    
    // 添加自定义错误回调
    size_t callbackId = errorHandler.AddCallback([](const RenderError& error) {
        if (error.GetCategory() == ErrorCategory::Transform) {
            // 只处理 Transform 相关错误
            std::cout << "[Transform Error] " 
                      << error.GetMessage() << std::endl;
            
            // 可以记录到文件、发送到服务器等
        }
    });
    
    // 使用完后移除
    // errorHandler.RemoveCallback(callbackId);
}
```

### 错误统计

```cpp
void CheckErrorStats() {
    auto& errorHandler = ErrorHandler::GetInstance();
    auto stats = errorHandler.GetStats();
    
    std::cout << "错误统计:\n"
              << "  信息: " << stats.infoCount << "\n"
              << "  警告: " << stats.warningCount << "\n"
              << "  错误: " << stats.errorCount << "\n"
              << "  严重: " << stats.criticalCount << "\n"
              << "  总计: " << stats.totalCount << std::endl;
    
    // 重置统计
    errorHandler.ResetStats();
}
```

---

## 📊 完整示例

```cpp
#include "render/transform.h"
#include "render/error.h"
#include <iostream>
#include <memory>

class TransformManager {
public:
    bool CreateHierarchy() {
        root_ = std::make_unique<Transform>();
        child1_ = std::make_unique<Transform>();
        child2_ = std::make_unique<Transform>();
        
        // 使用显式错误检查
        auto result1 = child1_->TrySetParent(root_.get());
        if (!result1.Ok()) {
            std::cerr << "设置 child1 父对象失败: " 
                      << result1.message << std::endl;
            return false;
        }
        
        auto result2 = child2_->TrySetParent(root_.get());
        if (!result2.Ok()) {
            std::cerr << "设置 child2 父对象失败: " 
                      << result2.message << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool UpdateTransforms(float deltaTime) {
        // 更新根变换
        Vector3 newPos = root_->GetPosition() + Vector3(0, deltaTime, 0);
        
        auto result = root_->TrySetPosition(newPos);
        if (!result.Ok()) {
            // 记录错误但不中断
            std::cerr << "更新位置失败: " << result.message << std::endl;
            // 可以选择使用旧值继续
            return false;
        }
        
        return true;
    }
    
    void PrintHierarchy() const {
        std::cout << "Transform 层级结构:\n";
        root_->PrintHierarchy();
    }
    
private:
    std::unique_ptr<Transform> root_;
    std::unique_ptr<Transform> child1_;
    std::unique_ptr<Transform> child2_;
};

int main() {
    // 设置错误处理
    auto& errorHandler = ErrorHandler::GetInstance();
    errorHandler.AddCallback([](const RenderError& error) {
        if (error.GetCategory() == ErrorCategory::Transform) {
            std::cout << "[Transform] " << error.GetFullMessage() << std::endl;
        }
    });
    
    // 使用 TransformManager
    TransformManager manager;
    if (manager.CreateHierarchy()) {
        std::cout << "层级创建成功" << std::endl;
        manager.PrintHierarchy();
        
        // 模拟更新
        for (int i = 0; i < 10; ++i) {
            manager.UpdateTransforms(0.016f);  // 60 FPS
        }
    }
    
    // 打印统计
    auto stats = errorHandler.GetStats();
    std::cout << "\n错误统计: 总计 " << stats.totalCount 
              << " 个错误/警告" << std::endl;
    
    return 0;
}
```

---

## 🎯 最佳实践

### ✅ 推荐做法

1. **新代码使用 `Try*` 方法**
   ```cpp
   auto result = transform.TrySetPosition(pos);
   if (!result.Ok()) {
       // 处理错误
   }
   ```

2. **关键操作检查错误码**
   ```cpp
   auto result = child.TrySetParent(&parent);
   if (result.code == ErrorCode::TransformCircularReference) {
       // 特殊处理循环引用
   }
   ```

3. **UI/用户交互显示错误消息**
   ```cpp
   auto result = transform.TrySetPosition(userInput);
   if (!result.Ok()) {
       ShowErrorDialog(result.message);  // 向用户显示友好错误
   }
   ```

### ❌ 避免做法

1. **不要忽略 Result 返回值**
   ```cpp
   // BAD: 忽略返回值
   transform.TrySetPosition(pos);  // 没有检查结果
   
   // GOOD: 检查返回值
   auto result = transform.TrySetPosition(pos);
   if (!result.Ok()) {
       // 处理错误
   }
   ```

2. **不要混用两种方式**
   ```cpp
   // BAD: 不一致
   transform.SetPosition(pos);  // 静默失败
   auto result = transform.TrySetRotation(rot);  // 显式检查
   
   // GOOD: 保持一致
   auto r1 = transform.TrySetPosition(pos);
   auto r2 = transform.TrySetRotation(rot);
   ```

---

## 🔍 调试技巧

### 启用详细错误日志

```cpp
// 在程序启动时
ErrorHandler::GetInstance().SetGLErrorCheckEnabled(true);
ErrorHandler::GetInstance().SetEnabled(true);
```

### 断点调试

```cpp
auto result = transform.TrySetParent(&parent);
if (result.code == ErrorCode::TransformCircularReference) {
    // 在这里设置断点，查看调用栈
    __debugbreak();  // MSVC
    // __builtin_trap();  // GCC/Clang
}
```

---

## 📚 相关文档

- `error.h` - 错误处理系统完整定义
- `transform.h` - Transform 类接口
- `Transform_优化方案.md` - P2 阶段设计文档

---

## 🎉 总结

Transform 统一错误处理提供了：

✅ **类型安全的错误码**（ErrorCode 枚举）  
✅ **详细的错误消息**（Result.message）  
✅ **向后兼容**（旧代码无需修改）  
✅ **可选的显式检查**（新代码推荐使用）  
✅ **全局错误回调系统**（统一监控）  
✅ **零性能开销**（未使用 Try* 时）

现在可以更可靠地构建复杂的 Transform 层级结构！🚀

