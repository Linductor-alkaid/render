# 网格系统线程安全指南

本文档说明了网格管理系统（`Mesh` 和 `MeshLoader` 类）的线程安全特性和最佳实践。

---

## 概述

从版本 1.0 开始，网格管理系统已实现完整的线程安全支持，允许多线程并发访问和修改网格数据。

---

## 线程安全特性

### Mesh 类

`Mesh` 类的所有公共方法都是线程安全的：

✅ **线程安全方法**：
- `SetVertices()` - 设置顶点数据
- `SetIndices()` - 设置索引数据
- `SetData()` - 同时设置顶点和索引数据
- `UpdateVertices()` - 部分更新顶点数据
- `Upload()` - 上传数据到 GPU
- `Draw()` - 绘制网格
- `DrawInstanced()` - 实例化绘制
- `Clear()` - 清理 GPU 资源
- `GetVertices()` - 获取顶点数据
- `GetIndices()` - 获取索引数据
- `GetVertexCount()` - 获取顶点数量
- `GetIndexCount()` - 获取索引数量
- `GetTriangleCount()` - 获取三角形数量
- `IsUploaded()` - 检查是否已上传
- `CalculateBounds()` - 计算包围盒
- `RecalculateNormals()` - 重新计算法线
- `RecalculateTangents()` - 重新计算切线

**实现机制**：
- 使用 `std::mutex` 保护所有成员变量
- 所有方法内部使用 `std::lock_guard` 自动管理锁
- 移动构造和移动赋值使用 `std::scoped_lock` 避免死锁

### MeshLoader 类

`MeshLoader` 的所有静态方法都是线程安全的（因为返回的 `Mesh` 对象本身是线程安全的）。

✅ **线程安全方法**：
- `CreatePlane()` - 创建平面
- `CreateCube()` - 创建立方体
- `CreateSphere()` - 创建球体
- `CreateCylinder()` - 创建圆柱体
- `CreateCone()` - 创建圆锥体
- `CreateTorus()` - 创建圆环
- `CreateCapsule()` - 创建胶囊体
- `CreateQuad()` - 创建四边形
- `CreateTriangle()` - 创建三角形
- `CreateCircle()` - 创建圆形

---

## OpenGL 上下文限制

⚠️ **重要提示**：虽然网格数据访问是线程安全的，但 OpenGL 调用必须在创建上下文的线程中执行。

### 需要 OpenGL 上下文的方法

以下方法涉及 OpenGL 调用，**必须**在主渲染线程中调用：
- `Mesh::Upload()` - 创建/更新 VAO/VBO/EBO
- `Mesh::Draw()` - 绘制网格
- `Mesh::DrawInstanced()` - 实例化绘制
- `Mesh::Clear()` - 删除 OpenGL 资源
- `Mesh::UpdateVertices()` - 更新 GPU 端顶点数据
- `Mesh::RecalculateNormals()` - 如果网格已上传，会更新 GPU 数据
- `MeshLoader::Create*()` - 所有创建方法内部调用 `Upload()`

### 可在任意线程调用的方法

以下方法仅操作 CPU 端数据，可以在任意线程中调用：
- `Mesh::SetVertices()` - 设置顶点数据（CPU 端）
- `Mesh::SetIndices()` - 设置索引数据（CPU 端）
- `Mesh::SetData()` - 设置网格数据（CPU 端）
- `Mesh::GetVertices()` - 读取顶点数据
- `Mesh::GetIndices()` - 读取索引数据
- `Mesh::Get*Count()` - 读取数量信息
- `Mesh::CalculateBounds()` - 计算包围盒

---

## 最佳实践

### 1. 主线程渲染模式

**推荐**：在主渲染线程创建和渲染网格，工作线程仅修改数据。

```cpp
// 主线程
auto mesh = MeshLoader::CreateCube();  // 创建并上传

// 工作线程
void WorkerThread(Ref<Mesh>& mesh) {
    std::vector<Vertex> newVertices = GenerateVertices();
    mesh->SetVertices(newVertices);  // 仅修改 CPU 端数据
}

// 主线程渲染循环
while (running) {
    // 如果数据被修改，重新上传
    if (!mesh->IsUploaded()) {
        mesh->Upload();  // 在主线程上传到 GPU
    }
    mesh->Draw();  // 在主线程绘制
}
```

### 2. 延迟上传模式

**推荐**：在工作线程创建网格数据，在主线程上传。

```cpp
// 工作线程
Ref<Mesh> CreateMeshInWorkerThread() {
    std::vector<Vertex> vertices = GenerateComplexGeometry();
    std::vector<uint32_t> indices = GenerateIndices();
    
    // 创建网格但不上传
    auto mesh = CreateRef<Mesh>(vertices, indices);
    return mesh;  // 返回未上传的网格
}

// 主线程
auto mesh = CreateMeshInWorkerThread();
mesh->Upload();  // 在主线程上传到 GPU
mesh->Draw();    // 绘制
```

### 3. 读取密集型模式

如果多个线程需要频繁读取网格数据：

```cpp
// 多个线程可以安全地并发读取
void ReaderThread(const Ref<Mesh>& mesh) {
    size_t vertexCount = mesh->GetVertexCount();
    const auto& vertices = mesh->GetVertices();
    AABB bounds = mesh->CalculateBounds();
    // ... 处理数据
}

// 主线程
std::thread t1(ReaderThread, mesh);
std::thread t2(ReaderThread, mesh);
std::thread t3(ReaderThread, mesh);
// 所有线程可以安全地并发读取
```

---

## 常见陷阱与解决方案

### 陷阱 1：在工作线程调用 OpenGL 方法

❌ **错误示例**：
```cpp
void WorkerThread(Ref<Mesh>& mesh) {
    mesh->SetVertices(newVertices);
    mesh->Upload();  // 错误！在非 OpenGL 线程调用
}
```

✅ **正确示例**：
```cpp
void WorkerThread(Ref<Mesh>& mesh, std::atomic<bool>& needsUpload) {
    mesh->SetVertices(newVertices);
    needsUpload = true;  // 设置标志
}

// 主线程
if (needsUpload.exchange(false)) {
    mesh->Upload();  // 在主线程上传
}
```

### 陷阱 2：保持对返回引用的长期引用

❌ **风险示例**：
```cpp
const std::vector<Vertex>& vertices = mesh->GetVertices();
// 长时间持有引用，期间其他线程可能修改数据
DoSomethingWithVertices(vertices);  // 可能访问已修改的数据
```

✅ **正确示例**：
```cpp
// 复制数据到局部变量
std::vector<Vertex> vertices = mesh->GetVertices();
// 现在可以安全地使用，不受其他线程影响
DoSomethingWithVertices(vertices);
```

### 陷阱 3：假设操作是原子的

❌ **错误示例**：
```cpp
// 错误：两次调用之间数据可能被其他线程修改
if (mesh->GetVertexCount() > 0) {
    const auto& vertices = mesh->GetVertices();  // 可能已经被清空
    UseVertex(vertices[0]);  // 潜在的越界访问
}
```

✅ **正确示例**：
```cpp
// 一次性获取数据
std::vector<Vertex> vertices = mesh->GetVertices();
if (!vertices.empty()) {
    UseVertex(vertices[0]);  // 安全
}
```

---

## 性能考虑

### 锁竞争

由于所有方法都受互斥锁保护，高频率的并发访问可能导致锁竞争：

- **读多写少**：如果多个线程主要是读取数据，考虑使用读写锁（未来优化）
- **频繁修改**：避免在渲染循环中每帧都修改网格数据
- **批量操作**：使用 `SetData()` 一次性设置顶点和索引，而不是分别调用

### 数据复制

- `GetVertices()` 和 `GetIndices()` 返回 const 引用，但在持有锁期间
- 如需长期使用，应复制数据到局部变量
- 避免频繁调用 getter 方法

---

## 测试示例

项目提供了完整的线程安全测试示例：`examples/10_mesh_thread_safe_test.cpp`

该示例演示了：
- 多个读取线程并发读取网格数据
- 一个写入线程持续修改网格数据
- 主线程渲染网格

运行测试：
```bash
cd build/bin/Release
./10_mesh_thread_safe_test
```

---

## 总结

| 特性 | 支持 | 说明 |
|------|------|------|
| 多线程读取 | ✅ | 完全支持，无限制 |
| 多线程写入 | ✅ | 完全支持，自动同步 |
| 并发读写 | ✅ | 完全支持，互斥锁保护 |
| OpenGL 调用 | ⚠️ | 必须在主线程 |
| 性能开销 | ⚠️ | 锁竞争下有少量开销 |

---

[返回文档首页](README.md)

