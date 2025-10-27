# MeshLoader API 参考

[返回 API 首页](README.md)

---

## 概述

`MeshLoader` 提供创建基本几何形状的工具函数，所有生成的网格都已自动上传到 GPU，可直接使用。

**头文件**: `render/mesh_loader.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class MeshLoader {
public:
    static Ref<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f,
                                 uint32_t widthSegments = 1, uint32_t heightSegments = 1,
                                 const Color& color = Color::White());
    
    static Ref<Mesh> CreateCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f,
                                const Color& color = Color::White());
    
    static Ref<Mesh> CreateSphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16,
                                  const Color& color = Color::White());
    
    static Ref<Mesh> CreateCylinder(float radiusTop = 0.5f, float radiusBottom = 0.5f,
                                    float height = 1.0f, uint32_t segments = 32,
                                    const Color& color = Color::White());
    
    static Ref<Mesh> CreateCone(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32,
                                const Color& color = Color::White());
    
    static Ref<Mesh> CreateTorus(float majorRadius = 1.0f, float minorRadius = 0.3f,
                                 uint32_t majorSegments = 32, uint32_t minorSegments = 16,
                                 const Color& color = Color::White());
    
    static Ref<Mesh> CreateCapsule(float radius = 0.5f, float height = 1.0f,
                                   uint32_t segments = 32, uint32_t rings = 8,
                                   const Color& color = Color::White());
    
    static Ref<Mesh> CreateQuad(float width = 1.0f, float height = 1.0f,
                                const Color& color = Color::White());
    
    static Ref<Mesh> CreateTriangle(float size = 1.0f, const Color& color = Color::White());
    
    static Ref<Mesh> CreateCircle(float radius = 0.5f, uint32_t segments = 32,
                                  const Color& color = Color::White());
};
```

---

## 几何形状生成方法

### CreatePlane

创建平面网格。

```cpp
static Ref<Mesh> CreatePlane(
    float width = 1.0f,
    float height = 1.0f,
    uint32_t widthSegments = 1,
    uint32_t heightSegments = 1,
    const Color& color = Color::White()
);
```

**参数**:
- `width` - 平面宽度（X 轴方向）
- `height` - 平面高度（Z 轴方向）
- `widthSegments` - 宽度分段数（最小 1）
- `heightSegments` - 高度分段数（最小 1）
- `color` - 顶点颜色

**特征**:
- 位于 XZ 平面（水平面）
- 法线向上（+Y 方向）
- 中心位于原点
- UV 坐标：(0,0) 到 (1,1)

**使用场景**: 地面、水面、平台

**示例**:
```cpp
// 简单平面
auto plane = MeshLoader::CreatePlane(10.0f, 10.0f);

// 细分平面（用于地形）
auto terrain = MeshLoader::CreatePlane(100.0f, 100.0f, 50, 50);
```

---

### CreateCube

创建立方体网格。

```cpp
static Ref<Mesh> CreateCube(
    float width = 1.0f,
    float height = 1.0f,
    float depth = 1.0f,
    const Color& color = Color::White()
);
```

**参数**:
- `width` - 宽度（X 轴）
- `height` - 高度（Y 轴）
- `depth` - 深度（Z 轴）
- `color` - 顶点颜色

**特征**:
- 中心位于原点
- 每个面有独立的法线和 UV
- 24 个顶点（每面 4 个）
- 36 个索引（12 个三角形）

**使用场景**: 箱子、建筑、占位符

**示例**:
```cpp
// 单位立方体
auto cube = MeshLoader::CreateCube();

// 自定义尺寸
auto box = MeshLoader::CreateCube(2.0f, 1.0f, 1.5f, Color::Red());
```

---

### CreateSphere

创建球体网格。

```cpp
static Ref<Mesh> CreateSphere(
    float radius = 0.5f,
    uint32_t segments = 32,
    uint32_t rings = 16,
    const Color& color = Color::White()
);
```

**参数**:
- `radius` - 球体半径
- `segments` - 水平分段数（经度，最小 3）
- `rings` - 垂直分段数（纬度，最小 2）
- `color` - 顶点颜色

**特征**:
- 中心位于原点
- 使用球面坐标生成
- 法线指向外侧
- UV 坐标环绕球面

**使用场景**: 球体物体、天空球、行星

**示例**:
```cpp
// 低精度球体（性能优先）
auto lowSphere = MeshLoader::CreateSphere(1.0f, 16, 8);

// 高精度球体（质量优先）
auto highSphere = MeshLoader::CreateSphere(1.0f, 64, 32);
```

---

### CreateCylinder

创建圆柱体网格。

```cpp
static Ref<Mesh> CreateCylinder(
    float radiusTop = 0.5f,
    float radiusBottom = 0.5f,
    float height = 1.0f,
    uint32_t segments = 32,
    const Color& color = Color::White()
);
```

**参数**:
- `radiusTop` - 顶部半径
- `radiusBottom` - 底部半径
- `height` - 高度（Y 轴）
- `segments` - 圆周分段数（最小 3）
- `color` - 顶点颜色

**特征**:
- 中心位于原点，沿 Y 轴延伸
- 包含顶部和底部盖子
- 法线根据面方向自动计算
- 如果 `radiusTop != radiusBottom`，可创建圆台

**使用场景**: 柱子、管道、圆台

**示例**:
```cpp
// 标准圆柱
auto cylinder = MeshLoader::CreateCylinder(0.5f, 0.5f, 2.0f, 32);

// 圆台（顶部小，底部大）
auto frustum = MeshLoader::CreateCylinder(0.3f, 0.6f, 1.5f, 32);
```

---

### CreateCone

创建圆锥体网格。

```cpp
static Ref<Mesh> CreateCone(
    float radius = 0.5f,
    float height = 1.0f,
    uint32_t segments = 32,
    const Color& color = Color::White()
);
```

**参数**:
- `radius` - 底部半径
- `height` - 高度（Y 轴）
- `segments` - 圆周分段数（最小 3）
- `color` - 顶点颜色

**特征**:
- 底部中心位于原点
- 顶点沿 +Y 方向
- 包含底部盖子

**使用场景**: 圆锥、箭头、尖顶

**示例**:
```cpp
auto cone = MeshLoader::CreateCone(0.5f, 1.5f, 24);
```

---

### CreateTorus

创建圆环网格。

```cpp
static Ref<Mesh> CreateTorus(
    float majorRadius = 1.0f,
    float minorRadius = 0.3f,
    uint32_t majorSegments = 32,
    uint32_t minorSegments = 16,
    const Color& color = Color::White()
);
```

**参数**:
- `majorRadius` - 大圆半径（环中心到管中心）
- `minorRadius` - 小圆半径（管的半径）
- `majorSegments` - 大圆分段数（最小 3）
- `minorSegments` - 小圆分段数（最小 3）
- `color` - 顶点颜色

**特征**:
- 位于 XZ 平面
- 中心位于原点
- 法线指向管表面外侧

**使用场景**: 甜甜圈、轮胎、环形物体

**示例**:
```cpp
// 标准圆环
auto torus = MeshLoader::CreateTorus(1.0f, 0.3f, 48, 24);

// 细环
auto ring = MeshLoader::CreateTorus(2.0f, 0.1f, 64, 8);
```

---

### CreateCapsule

创建胶囊体网格。

```cpp
static Ref<Mesh> CreateCapsule(
    float radius = 0.5f,
    float height = 1.0f,
    uint32_t segments = 32,
    uint32_t rings = 8,
    const Color& color = Color::White()
);
```

**参数**:
- `radius` - 半径
- `height` - 中间圆柱部分的高度（不含两端半球）
- `segments` - 圆周分段数（最小 3）
- `rings` - 半球纬度分段数（最小 1）
- `color` - 顶点颜色

**特征**:
- 沿 Y 轴，中心在原点
- 由中间圆柱和两端半球组成
- 平滑过渡的法线

**使用场景**: 角色碰撞体、药丸形状

**示例**:
```cpp
auto capsule = MeshLoader::CreateCapsule(0.5f, 1.0f, 32, 8);
```

---

### CreateQuad

创建四边形网格。

```cpp
static Ref<Mesh> CreateQuad(
    float width = 1.0f,
    float height = 1.0f,
    const Color& color = Color::White()
);
```

**参数**:
- `width` - 宽度（X 轴）
- `height` - 高度（Y 轴）
- `color` - 顶点颜色

**特征**:
- 位于 XY 平面
- 法线朝向 +Z
- 中心位于原点
- 4 个顶点，2 个三角形

**使用场景**: UI 元素、精灵、公告板

**示例**:
```cpp
// 单位四边形
auto quad = MeshLoader::CreateQuad();

// 矩形
auto rect = MeshLoader::CreateQuad(2.0f, 1.0f);
```

---

### CreateTriangle

创建三角形网格。

```cpp
static Ref<Mesh> CreateTriangle(
    float size = 1.0f,
    const Color& color = Color::White()
);
```

**参数**:
- `size` - 边长（等边三角形）
- `color` - 顶点颜色

**特征**:
- 等边三角形
- 位于 XY 平面
- 法线朝向 +Z
- 中心位于原点

**使用场景**: 简单几何、指示器

**示例**:
```cpp
auto triangle = MeshLoader::CreateTriangle(1.0f);
```

---

### CreateCircle

创建圆形网格。

```cpp
static Ref<Mesh> CreateCircle(
    float radius = 0.5f,
    uint32_t segments = 32,
    const Color& color = Color::White()
);
```

**参数**:
- `radius` - 半径
- `segments` - 圆周分段数（最小 3）
- `color` - 顶点颜色

**特征**:
- 位于 XY 平面
- 法线朝向 +Z
- 中心位于原点
- 使用三角形扇形渲染

**使用场景**: 圆形 UI、光晕效果

**示例**:
```cpp
// 低精度圆
auto circle = MeshLoader::CreateCircle(1.0f, 16);

// 高精度圆
auto smoothCircle = MeshLoader::CreateCircle(1.0f, 64);
```

---

## 使用示例

### 基本使用

```cpp
#include <render/mesh_loader.h>

// 创建几何形状（自动上传到 GPU）
auto cube = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f);
auto sphere = MeshLoader::CreateSphere(0.5f, 32, 16);

// 直接渲染
shader->Bind();
// ... 设置 uniforms ...
cube->Draw();
sphere->Draw();
shader->Unbind();
```

---

### 创建多种形状

```cpp
std::vector<Ref<Mesh>> shapes;

// 添加各种形状
shapes.push_back(MeshLoader::CreateCube());
shapes.push_back(MeshLoader::CreateSphere(0.5f, 32, 16));
shapes.push_back(MeshLoader::CreateCylinder(0.4f, 0.4f, 1.0f, 32));
shapes.push_back(MeshLoader::CreateCone(0.5f, 1.0f, 32));
shapes.push_back(MeshLoader::CreateTorus(0.8f, 0.2f, 32, 16));

// 渲染所有形状
for (auto& shape : shapes) {
    // ... 设置变换 ...
    shape->Draw();
}
```

---

### 自定义颜色

```cpp
// 创建彩色立方体
auto redCube = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::Red());
auto greenSphere = MeshLoader::CreateSphere(0.5f, 32, 16, Color::Green());
auto blueCylinder = MeshLoader::CreateCylinder(0.4f, 0.4f, 1.0f, 32, Color::Blue());
```

---

### 细分级别控制

```cpp
// 低精度（性能优先）
auto lowSphere = MeshLoader::CreateSphere(1.0f, 8, 4);   // 快速渲染
auto lowCylinder = MeshLoader::CreateCylinder(0.5f, 0.5f, 1.0f, 8);

// 高精度（质量优先）
auto highSphere = MeshLoader::CreateSphere(1.0f, 64, 32);  // 平滑表面
auto highCylinder = MeshLoader::CreateCylinder(0.5f, 0.5f, 1.0f, 64);
```

---

### 创建地形网格

```cpp
// 创建细分的平面用于地形
auto terrain = MeshLoader::CreatePlane(100.0f, 100.0f, 100, 100);

// 修改顶点高度
auto& vertices = terrain->GetVertices();
for (auto& vertex : vertices) {
    // 生成高度（如柏林噪声）
    float height = GenerateHeight(vertex.position.x(), vertex.position.z());
    vertex.position.y() = height;
}

// 重新计算法线
terrain->RecalculateNormals();
terrain->Upload();
```

---

### 组合几何形状

```cpp
// 创建一个简单的雪人
auto body = MeshLoader::CreateSphere(1.0f, 32, 16);      // 身体
auto head = MeshLoader::CreateSphere(0.6f, 32, 16);      // 头部
auto nose = MeshLoader::CreateCone(0.1f, 0.3f, 16);      // 鼻子
auto hat = MeshLoader::CreateCylinder(0.5f, 0.5f, 0.2f, 32);  // 帽子

// 渲染时使用不同的变换矩阵
RenderMesh(body, Matrix4::Identity());
RenderMesh(head, TranslateMatrix(0, 1.5f, 0));
RenderMesh(nose, TranslateMatrix(0, 1.8f, 0.6f) * RotateMatrix(...));
RenderMesh(hat, TranslateMatrix(0, 2.2f, 0));
```

---

## 性能建议

1. **细分级别**: 
   - 远处物体使用低细分（segments=8-16）
   - 近处物体使用高细分（segments=32-64）

2. **缓存网格**: 
   - 相同形状重复使用同一个 Mesh 对象
   - 使用不同的变换矩阵渲染多个实例

3. **实例化渲染**: 
   - 大量相同网格使用 `DrawInstanced()`

4. **顶点数据**: 
   - 生成的网格包含完整顶点属性
   - 如果不需要某些属性（如颜色），着色器中可忽略

---

## 坐标系统

所有几何形状遵循以下坐标系统：

- **X 轴**: 右方向（红色）
- **Y 轴**: 上方向（绿色）
- **Z 轴**: 前方向（蓝色）

**朝向约定**:
- 平面: 法线向 +Y（向上）
- 四边形/三角形/圆形: 法线向 +Z（向前）
- 立方体: 每个面法线指向外侧
- 球体/圆柱: 法线指向表面外侧

---

## 纹理坐标

所有生成的网格都包含 UV 纹理坐标：

- **范围**: (0, 0) 到 (1, 1)
- **平面**: 均匀映射
- **立方体**: 每个面独立映射
- **球体**: 球面展开映射
- **圆柱**: 侧面环绕映射，盖子径向映射

---

## 扩展功能

未来版本计划支持：

- [ ] 从文件加载网格（OBJ, FBX, GLTF）
- [ ] 更复杂的几何形状（多面体、齿轮等）
- [ ] 程序化地形生成
- [ ] 网格合并和优化工具

---

## 相关类型

- [Mesh](Mesh.md) - 网格类
- [Types](Types.md) - Vector3, Color 等类型
- [Shader](Shader.md) - 着色器系统

---

## 示例程序

完整示例请参考：
- [06_mesh_test.cpp](../../examples/06_mesh_test.cpp) - 网格系统测试

---

[上一篇: Mesh](Mesh.md) | [返回 API 首页](README.md)

