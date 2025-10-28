# MeshLoader API 参考

[返回 API 首页](README.md)

---

## 概述

`MeshLoader` 提供两大功能：
1. **外部模型文件加载** - 支持 OBJ, FBX, GLTF/GLB, Collada, Blender, PMX/PMD (MMD), 3DS, PLY, STL 等格式
2. **基本几何形状生成** - 创建立方体、球体、圆柱等基本几何形状

所有生成/加载的网格都已自动上传到 GPU，可直接使用。

**头文件**: `render/mesh_loader.h`  
**命名空间**: `Render`  
**依赖**: Assimp 5.x（用于文件加载）

### ✨ 功能特性

**✅ 几何数据加载**：
- 顶点位置、法线、UV 坐标、顶点颜色
- 多网格模型支持
- 自动三角化和法线生成
- 网格优化（顶点合并、缓存优化）

**✅ 材质和纹理加载**（v0.5.0 新增）：
- 材质属性：环境色、漫反射、镜面反射、自发光
- 物理参数：光泽度、不透明度、金属度、粗糙度
- 纹理贴图：漫反射、镜面反射、法线、AO、自发光
- 自动透明材质检测和混合模式设置
- 与 Material 和 TextureLoader 无缝集成
- **纹理智能缓存**：使用完整路径作为纹理标识，自动去重
- **路径兼容性**：避免 `std::filesystem` 的中文路径问题，使用简单字符串操作

**❌ 尚未实现**：
- 骨骼和蒙皮权重
- 动画数据
- PMX/MMD 特殊效果（Toon 着色、Sphere Map）

### 线程安全

- ✅ **所有静态方法都是线程安全的**（因为返回的 `Mesh` 对象本身是线程安全的）
- ⚠️ **注意**：方法内部会调用 `Upload()`（涉及 OpenGL 调用），必须在创建 OpenGL 上下文的线程中调用
- 建议在主渲染线程调用 `MeshLoader` 的加载/创建方法
- 如需在工作线程创建网格数据，请使用 `Mesh` 构造函数创建网格对象，然后在渲染线程调用 `Upload()`

---

## 类定义

```cpp
class MeshLoader {
public:
    // ========================================================================
    // 文件加载功能
    // ========================================================================
    
    static std::vector<Ref<Mesh>> LoadFromFile(
        const std::string& filepath,
        bool flipUVs = true
    );
    
    static Ref<Mesh> LoadMeshFromFile(
        const std::string& filepath,
        uint32_t meshIndex = 0,
        bool flipUVs = true
    );
    
    // ========================================================================
    // 基本几何形状生成
    // ========================================================================
    
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

## 文件加载方法

### LoadFromFile

从文件加载 3D 模型（可能包含多个网格）。

```cpp
static std::vector<Ref<Mesh>> LoadFromFile(
    const std::string& filepath,
    bool flipUVs = true
);
```

**参数**:
- `filepath` - 模型文件路径（相对或绝对路径）
- `flipUVs` - 是否翻转 UV 坐标（默认 `true`，适用于 OpenGL）

**返回值**:
- 网格列表（如果加载失败返回空列表）

**支持的格式**:
- `.obj` - Wavefront OBJ（最常用，推荐）
- `.fbx` - Autodesk FBX
- `.gltf`, `.glb` - GL Transmission Format（现代标准，推荐）
- `.dae` - Collada
- `.blend` - Blender 原生格式
- `.pmx`, `.pmd` - MikuMikuDance (MMD) 模型格式
- `.3ds` - 3D Studio
- `.ply` - Polygon File Format
- `.stl` - Stereolithography

**特性**:
- ✅ 自动三角化（所有多边形转换为三角形）
- ✅ 自动生成法线（如果文件中不包含）
- ✅ 自动优化（合并相同顶点、改善缓存局部性）
- ✅ 自动上传到 GPU（返回的网格可直接渲染）
- ✅ 支持多网格模型（返回网格列表）

**当前限制**:
- ⚠️ 仅提取几何数据（位置、法线、UV、顶点颜色）
- ⚠️ 不加载材质和纹理（需要手动设置着色器颜色）
- ⚠️ 不支持骨骼动画
- ⚠️ PMX 模型的特殊效果（Toon、Sphere Map）暂不支持

**使用场景**: 加载外部 3D 模型文件

**示例**:
```cpp
// 加载模型文件
auto meshes = MeshLoader::LoadFromFile("models/character.fbx");

if (meshes.empty()) {
    LOG_ERROR("Failed to load model");
    return;
}

// 渲染所有网格
shader->Bind();
for (auto& mesh : meshes) {
    // 设置 uniforms...
    mesh->Draw();
}
```

---

### LoadMeshFromFile

从文件加载单个网格。

```cpp
static Ref<Mesh> LoadMeshFromFile(
    const std::string& filepath,
    uint32_t meshIndex = 0,
    bool flipUVs = true
);
```

**参数**:
- `filepath` - 模型文件路径
- `meshIndex` - 网格索引（默认 0，第一个网格）
- `flipUVs` - 是否翻转 UV 坐标（默认 `true`）

**返回值**:
- 网格对象（如果加载失败返回 `nullptr`）

**特性**:
- 如果模型包含多个网格，只返回指定索引的网格
- 如果索引超出范围，会返回第一个网格并输出警告

**使用场景**: 加载只包含单个网格的模型，或只需要模型中的特定网格

**示例**:
```cpp
// 加载第一个网格
auto mesh = MeshLoader::LoadMeshFromFile("models/cube.obj");

if (!mesh) {
    LOG_ERROR("Failed to load mesh");
    return;
}

// 直接渲染
shader->Bind();
mesh->Draw();
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

## 文件加载使用示例

### 基本加载

```cpp
#include <render/mesh_loader.h>

// 加载模型文件（自动上传到 GPU）
auto meshes = MeshLoader::LoadFromFile("models/my_model.obj");

// 设置着色器和渲染状态
shader->Use();

// 设置统一的材质颜色（因为暂不支持材质提取）
auto* uniformMgr = shader->GetUniformManager();
uniformMgr->SetColor("uColor", Color(0.8f, 0.8f, 0.9f, 1.0f));
uniformMgr->SetVector3("uLightDir", Vector3(-0.3f, -0.8f, -0.5f).normalized());

// 渲染所有网格（多网格模型的各部件会叠加形成完整模型）
for (auto& mesh : meshes) {
    mesh->Draw();
}

shader->Unuse();
```

---

### 加载多种格式

```cpp
// Wavefront OBJ（最简单，推荐）
auto objMeshes = MeshLoader::LoadFromFile("models/cube.obj");

// Autodesk FBX（常用于游戏资产）
auto fbxMeshes = MeshLoader::LoadFromFile("models/character.fbx");

// GLTF（现代标准）
auto gltfMeshes = MeshLoader::LoadFromFile("models/scene.gltf");
auto glbMeshes = MeshLoader::LoadFromFile("models/scene.glb");

// MikuMikuDance PMX（日系角色模型）
auto pmxMeshes = MeshLoader::LoadFromFile("models/miku.pmx");

// Blender 原生格式
auto blendMeshes = MeshLoader::LoadFromFile("models/asset.blend");

// 注意：所有格式都只提取几何数据，材质和纹理需要手动设置
```

---

### 完整示例：PMX 模型查看器

这是一个完整的示例，展示如何加载和渲染 PMX 模型：

```cpp
#include <render/renderer.h>
#include <render/logger.h>
#include <render/mesh_loader.h>
#include <render/shader_cache.h>
#include <SDL3/SDL.h>
#include <cmath>

using namespace Render;

int main() {
    // 初始化渲染器
    Renderer renderer;
    renderer.Initialize("PMX 模型查看器", 800, 600);
    
    // 设置渲染状态
    auto* state = renderer.GetRenderState();
    state->SetDepthTest(true);
    state->SetCullFace(CullFace::Back);
    state->SetClearColor(Color(0.1f, 0.1f, 0.15f, 1.0f));
    
    // 加载着色器
    auto& shaderCache = ShaderCache::GetInstance();
    auto shader = shaderCache.LoadShader("mesh_test", 
        "shaders/mesh_test.vert", "shaders/mesh_test.frag");
    
    // 加载 PMX 模型
    auto meshes = MeshLoader::LoadFromFile("models/miku.pmx");
    if (meshes.empty()) {
        // 回退到立方体
        meshes.push_back(MeshLoader::CreateCube());
    }
    
    // PMX 模型缩放（根据实际大小调整）
    float scale = 0.08f;
    Matrix4 scaleMatrix = Matrix4::Identity();
    scaleMatrix(0, 0) = scale;
    scaleMatrix(1, 1) = scale;
    scaleMatrix(2, 2) = scale;
    
    // 自动计算相机位置（基于模型包围盒）
    Vector3 center, modelSize;
    // ... 计算模型包围盒 ...
    
    // 主循环
    bool running = true;
    float rotation = 0.0f;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || 
                (event.type == SDL_EVENT_KEY_DOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        
        renderer.BeginFrame();
        renderer.Clear();
        
        // 更新旋转
        rotation += 0.01f;
        Matrix4 rotateMatrix = Matrix4::Identity()
            .rotate(Vector3(0, 1, 0), rotation);
        
        // 计算 MVP 矩阵
        Matrix4 mvp = /* ... 你的 MVP 计算 ... */;
        
        // 渲染
        shader->Use();
        auto* uniformMgr = shader->GetUniformManager();
        uniformMgr->SetMatrix4("uMVP", mvp);
        uniformMgr->SetColor("uColor", Color::White());
        
        for (auto& mesh : meshes) {
            mesh->Draw();
        }
        
        shader->Unuse();
        renderer.EndFrame();
    }
    
    return 0;
}
```

更完整的实现请参考 `examples/11_model_loader_test.cpp`。

---

### 错误处理

```cpp
auto meshes = MeshLoader::LoadFromFile("models/model.obj");

if (meshes.empty()) {
    Logger::GetInstance().Error("Failed to load model");
    // 使用默认网格
    auto defaultMesh = MeshLoader::CreateCube();
    meshes.push_back(defaultMesh);
}

// 安全渲染
for (auto& mesh : meshes) {
    if (mesh && mesh->IsUploaded()) {
        mesh->Draw();
    }
}
```

---

### 加载并检查网格信息

```cpp
auto meshes = MeshLoader::LoadFromFile("models/complex_model.fbx");

for (size_t i = 0; i < meshes.size(); i++) {
    auto& mesh = meshes[i];
    
    LOG_INFO("Mesh " + std::to_string(i));
    LOG_INFO("  Vertices: " + std::to_string(mesh->GetVertexCount()));
    LOG_INFO("  Triangles: " + std::to_string(mesh->GetTriangleCount()));
    
    // 计算包围盒
    AABB bounds = mesh->CalculateBounds();
    // ... 用于剔除等
}
```

---

### 组合文件加载和几何生成

```cpp
// 尝试加载外部模型
auto meshes = MeshLoader::LoadFromFile("models/character.obj");

// 如果加载失败，使用程序生成的网格
if (meshes.empty()) {
    LOG_WARNING("Using fallback geometry");
    meshes.push_back(MeshLoader::CreateCapsule(0.5f, 1.8f));
}

// 添加地面
auto ground = MeshLoader::CreatePlane(50.0f, 50.0f, 10, 10);

// 渲染场景
for (auto& mesh : meshes) {
    mesh->Draw();
}
ground->Draw();
```

---

## PMX/MMD 模型特殊说明

**支持的 MMD 格式**：
- `.pmx` - PMX 2.0/2.1（推荐）
- `.pmd` - PMD（旧版）
- `.vmd` - 动作数据（暂不支持）

**当前 PMX 加载状态**：
- ✅ 几何数据（顶点、法线、UV）
- ✅ 多网格部件（头发、身体、衣服等）
- ❌ 材质和纹理（需要手动设置颜色）
- ❌ Toon 着色（卡通渲染效果）
- ❌ Sphere Map（环境映射）
- ❌ 骨骼和变形器
- ❌ 物理模拟

**渲染 PMX 的建议**：
```cpp
// 加载 PMX 模型
auto meshes = MeshLoader::LoadFromFile("models/miku.pmx");

// PMX 模型通常较大，需要缩放
float scale = 0.08f;  // 根据实际情况调整

// 渲染所有部件
shader->Use();
for (auto& mesh : meshes) {
    // 所有部件使用相同的变换矩阵
    uniformMgr->SetMatrix4("uMVP", mvpMatrix);
    uniformMgr->SetColor("uColor", Color::White());
    mesh->Draw();
}
```

---

## 扩展功能

**已完成**：
- [x] 从文件加载网格几何（OBJ, FBX, GLTF, PMX/PMD 等）✅

**计划中**：
- [ ] 材质信息提取（纹理路径、颜色、反射属性）
- [ ] 纹理自动加载和绑定
- [ ] PMX Toon 着色支持
- [ ] 骨骼动画支持
- [ ] 变形器（Morph）支持
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
- [06_mesh_test.cpp](../../examples/06_mesh_test.cpp) - 基本几何形状生成测试
- [11_model_loader_test.cpp](../../examples/11_model_loader_test.cpp) - 外部模型文件加载测试

---

## 获取测试模型

您可以从以下来源获取免费的 3D 模型用于测试：

1. **Blender** - 自带简单模型（立方体、球体等），导出为 OBJ/FBX/GLTF
2. **Sketchfab** - https://sketchfab.com/ （大量免费模型）
3. **Free3D** - https://free3d.com/
4. **TurboSquid** - https://www.turbosquid.com/ （有免费分类）
5. **glTF Sample Models** - https://github.com/KhronosGroup/glTF-Sample-Models （官方示例）

---

[上一篇: Mesh](Mesh.md) | [返回 API 首页](README.md)

