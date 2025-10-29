# Mesh API 参考

[返回 API 首页](README.md)

---

## 概述

`Mesh` 类管理顶点数据、索引数据和 OpenGL 缓冲区对象（VAO/VBO/EBO），提供便捷的网格创建、更新和渲染接口。

**头文件**: `render/mesh.h`  
**命名空间**: `Render`

### 线程安全

- ✅ **所有公共方法都是线程安全的**
- 使用互斥锁保护所有成员变量的访问
- 支持多线程并发读取和修改网格数据
- ⚠️ **注意**：OpenGL 调用必须在创建上下文的线程中执行
- 建议在主渲染线程调用 `Upload()`、`Draw()` 等涉及 OpenGL 的方法
- 数据设置方法（`SetVertices()`、`SetIndices()` 等）可以在任意线程调用

---

## 类定义

```cpp
class Mesh {
public:
    Mesh();
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    ~Mesh();
    
    void SetVertices(const std::vector<Vertex>& vertices);
    void SetIndices(const std::vector<uint32_t>& indices);
    void SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void UpdateVertices(const std::vector<Vertex>& vertices, size_t offset = 0);
    
    void Upload();
    void Draw(DrawMode mode = DrawMode::Triangles) const;
    void DrawInstanced(uint32_t instanceCount, DrawMode mode = DrawMode::Triangles) const;
    void Clear();
    
    const std::vector<Vertex>& GetVertices() const;
    const std::vector<uint32_t>& GetIndices() const;
    size_t GetVertexCount() const;
    size_t GetIndexCount() const;
    size_t GetTriangleCount() const;
    bool IsUploaded() const;
    
    AABB CalculateBounds() const;
    void RecalculateNormals();
    void RecalculateTangents();
};
```

---

## 数据结构

### Vertex

顶点数据结构，包含完整的顶点属性。

```cpp
struct Vertex {
    Vector3 position;   // 位置
    Vector2 texCoord;   // 纹理坐标
    Vector3 normal;     // 法线
    Color color;        // 顶点颜色
    
    Vertex();
    Vertex(const Vector3& pos);
    Vertex(const Vector3& pos, const Vector2& uv, const Vector3& norm, const Color& col);
};
```

**成员说明**:
- `position` - 顶点位置（3D 空间坐标）
- `texCoord` - 纹理坐标（UV 坐标，范围 0-1）
- `normal` - 顶点法线（用于光照计算）
- `color` - 顶点颜色（RGBA）

**顶点布局**:
- Location 0: Position (vec3)
- Location 1: TexCoord (vec2)
- Location 2: Normal (vec3)
- Location 3: Color (vec4)

---

## 枚举类型

### DrawMode

网格绘制模式。

```cpp
enum class DrawMode {
    Triangles,      // 三角形列表
    TriangleStrip,  // 三角形带
    TriangleFan,    // 三角形扇
    Lines,          // 线段列表
    LineStrip,      // 线段带
    LineLoop,       // 线段环
    Points          // 点
};
```

---

## 构造和析构

### Mesh (默认构造)

创建空网格。

```cpp
Mesh();
```

**说明**: 创建未初始化的空网格对象。

---

### Mesh (带数据构造)

创建并初始化网格。

```cpp
Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
```

**参数**:
- `vertices` - 顶点数组
- `indices` - 索引数组

**说明**: 创建网格并设置数据，但不上传到 GPU（需调用 `Upload()`）。

---

### ~Mesh

析构函数。

```cpp
~Mesh();
```

**说明**: 自动清理 GPU 资源（VAO/VBO/EBO）。

---

## 数据设置方法

### SetVertices

设置顶点数据。

```cpp
void SetVertices(const std::vector<Vertex>& vertices);
```

**参数**:
- `vertices` - 顶点数组

**说明**: 设置网格的顶点数据。修改后需要重新调用 `Upload()`。

---

### SetIndices

设置索引数据。

```cpp
void SetIndices(const std::vector<uint32_t>& indices);
```

**参数**:
- `indices` - 索引数组

**说明**: 设置网格的索引数据。如果不设置索引，将使用顶点数组直接绘制。

---

### SetData

同时设置顶点和索引数据。

```cpp
void SetData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
```

**参数**:
- `vertices` - 顶点数组
- `indices` - 索引数组

**说明**: 便捷方法，一次性设置顶点和索引数据。

---

### UpdateVertices

部分更新顶点数据。

```cpp
void UpdateVertices(const std::vector<Vertex>& vertices, size_t offset = 0);
```

**参数**:
- `vertices` - 新的顶点数据
- `offset` - 起始偏移量（顶点索引，默认 0）

**说明**: 仅在网格已上传到 GPU 后使用。用于动态更新顶点数据（如动画）。

**示例**:
```cpp
// 更新前 10 个顶点
std::vector<Vertex> newVertices(10);
// ... 填充数据 ...
mesh->UpdateVertices(newVertices, 0);
```

---

## GPU 管理方法

### Upload

上传网格数据到 GPU。

```cpp
void Upload();
```

**说明**: 创建 VAO/VBO/EBO 并将顶点和索引数据上传到 GPU。必须在渲染前调用。

**注意**: 
- 如果已经上传过，会先清理旧资源
- 上传后可以修改 CPU 端数据，但需要重新 Upload 或使用 UpdateVertices

---

### Draw

绘制网格。

```cpp
void Draw(DrawMode mode = DrawMode::Triangles) const;
```

**参数**:
- `mode` - 绘制模式（默认为三角形）

**说明**: 使用当前绑定的着色器绘制网格。

**前提条件**:
- 网格必须已上传（`IsUploaded() == true`）
- 着色器必须已绑定

---

### DrawInstanced

实例化绘制。

```cpp
void DrawInstanced(uint32_t instanceCount, DrawMode mode = DrawMode::Triangles) const;
```

**参数**:
- `instanceCount` - 实例数量
- `mode` - 绘制模式（默认为三角形）

**说明**: 使用实例化渲染绘制多个相同的网格。

---

### Clear

清理 GPU 资源。

```cpp
void Clear();
```

**说明**: 释放 VAO/VBO/EBO 等 GPU 资源。析构时自动调用。

---

## 查询方法

### GetVertices

获取顶点数据。

```cpp
const std::vector<Vertex>& GetVertices() const;
```

**返回**: 顶点数组的常量引用

---

### GetIndices

获取索引数据。

```cpp
const std::vector<uint32_t>& GetIndices() const;
```

**返回**: 索引数组的常量引用

---

### GetVertexCount

获取顶点数量。

```cpp
size_t GetVertexCount() const;
```

**返回**: 顶点数量

---

### GetIndexCount

获取索引数量。

```cpp
size_t GetIndexCount() const;
```

**返回**: 索引数量

---

### GetTriangleCount

获取三角形数量。

```cpp
size_t GetTriangleCount() const;
```

**返回**: 三角形数量（索引数 / 3）

---

### IsUploaded

检查是否已上传到 GPU。

```cpp
bool IsUploaded() const;
```

**返回**: 
- `true` - 已上传到 GPU
- `false` - 未上传

---

## 工具方法

### CalculateBounds

计算包围盒。

```cpp
AABB CalculateBounds() const;
```

**返回**: 轴对齐包围盒（AABB）

**说明**: 根据所有顶点位置计算包围盒。

---

### RecalculateNormals

重新计算法线。

```cpp
void RecalculateNormals();
```

**说明**: 
- 基于三角形面计算法线
- 使用面积加权平均
- 自动更新 GPU 数据（如果已上传）

**用途**: 当手动修改顶点位置后，需要重新计算法线。

---

### RecalculateTangents

重新计算切线。

```cpp
void RecalculateTangents();
```

**说明**: 计算切线和副切线（用于法线贴图）。

**注意**: 当前版本暂未实现。

---

## 使用示例

### 基本使用

```cpp
#include <render/mesh.h>

// 创建顶点数据
std::vector<Vertex> vertices = {
    Vertex(Vector3(-0.5f, -0.5f, 0.0f), Vector2(0, 1), Vector3(0, 0, 1), Color::White()),
    Vertex(Vector3( 0.5f, -0.5f, 0.0f), Vector2(1, 1), Vector3(0, 0, 1), Color::White()),
    Vertex(Vector3( 0.0f,  0.5f, 0.0f), Vector2(0.5f, 0), Vector3(0, 0, 1), Color::White())
};

// 创建索引数据
std::vector<uint32_t> indices = { 0, 1, 2 };

// 创建网格
auto mesh = CreateRef<Mesh>(vertices, indices);
mesh->Upload();

// 渲染
shader->Bind();
// ... 设置 uniforms ...
mesh->Draw();
shader->Unbind();
```

---

### 动态更新

```cpp
// 创建动态网格
auto mesh = CreateRef<Mesh>(vertices, indices);
mesh->Upload();

// 每帧更新顶点
void Update(float time) {
    std::vector<Vertex> newVertices = vertices;
    
    // 修改顶点位置（如波浪效果）
    for (size_t i = 0; i < newVertices.size(); ++i) {
        newVertices[i].position.y() = std::sin(time + i * 0.5f) * 0.1f;
    }
    
    // 更新 GPU 数据
    mesh->UpdateVertices(newVertices);
}
```

---

### 计算包围盒

```cpp
auto mesh = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f);

AABB bounds = mesh->CalculateBounds();
Vector3 center = bounds.GetCenter();
Vector3 size = bounds.GetSize();

Logger::Info("Bounds: center=" + std::to_string(center.x()) + "," + 
             std::to_string(center.y()) + "," + std::to_string(center.z()));
```

---

### 重新计算法线

```cpp
// 手动创建网格
auto mesh = CreateRef<Mesh>();
std::vector<Vertex> vertices = { /* ... */ };
std::vector<uint32_t> indices = { /* ... */ };

mesh->SetData(vertices, indices);
mesh->RecalculateNormals();  // 根据三角形计算法线
mesh->Upload();
```

---

### 使用不同绘制模式

```cpp
// 渲染为线框
mesh->Draw(DrawMode::Lines);

// 渲染为点
mesh->Draw(DrawMode::Points);

// 渲染为三角形带
mesh->Draw(DrawMode::TriangleStrip);
```

---

### 实例化渲染

```cpp
// 设置实例化 uniforms（在着色器中）
shader->Bind();
// ... 设置变换矩阵数组 ...

// 一次绘制 100 个实例
mesh->DrawInstanced(100);
```

---

## 性能建议

1. **静态网格**: 创建后立即 `Upload()`，渲染时直接 `Draw()`
2. **动态网格**: 使用 `UpdateVertices()` 而不是重新 `Upload()`
3. **大量网格**: 考虑使用实例化渲染（`DrawInstanced()`）
4. **索引缓冲**: 优先使用索引绘制，减少顶点数据
5. **顶点数据**: 只包含必要的属性，减少内存和带宽

---

## 相关类型

- [MeshLoader](MeshLoader.md) - 几何形状生成器
- [Types](Types.md) - Vector3, Vector2, Color 等类型
- [Shader](Shader.md) - 着色器系统

---

## 示例程序

完整示例请参考：
- [06_mesh_test.cpp](../../examples/06_mesh_test.cpp) - 网格系统测试

---

[上一篇: TextureLoader](TextureLoader.md) | [下一篇: MeshLoader](MeshLoader.md) | [返回 API 首页](README.md)

