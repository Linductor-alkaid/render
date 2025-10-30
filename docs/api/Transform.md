# Transform API 参考

[返回 API 首页](README.md)

---

## 概述

`Transform` 类表示3D空间中的变换（位置、旋转、缩放），支持本地变换和世界变换，以及父子关系的变换层级。

**头文件**: `render/transform.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class Transform {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    Transform();
    Transform(const Vector3& position, 
              const Quaternion& rotation = Quaternion::Identity(), 
              const Vector3& scale = Vector3::Ones());
    
    // 位置操作
    void SetPosition(const Vector3& position);
    const Vector3& GetPosition() const;
    Vector3 GetWorldPosition() const;
    void Translate(const Vector3& translation);
    void TranslateWorld(const Vector3& translation);
    
    // 旋转操作
    void SetRotation(const Quaternion& rotation);
    void SetRotationEuler(const Vector3& euler);
    void SetRotationEulerDegrees(const Vector3& euler);
    const Quaternion& GetRotation() const;
    Vector3 GetRotationEuler() const;
    Vector3 GetRotationEulerDegrees() const;
    Quaternion GetWorldRotation() const;
    void Rotate(const Quaternion& rotation);
    void RotateAround(const Vector3& axis, float angle);
    void RotateAroundWorld(const Vector3& axis, float angle);
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
    
    // 缩放操作
    void SetScale(const Vector3& scale);
    void SetScale(float scale);
    const Vector3& GetScale() const;
    Vector3 GetWorldScale() const;
    
    // 方向向量
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;
    
    // 矩阵操作
    Matrix4 GetLocalMatrix() const;
    Matrix4 GetWorldMatrix() const;
    void SetFromMatrix(const Matrix4& matrix);
    
    // 父子关系
    void SetParent(Transform* parent);
    Transform* GetParent() const;
    bool HasParent() const;
    
    // 坐标变换
    Vector3 TransformPoint(const Vector3& localPoint) const;
    Vector3 TransformDirection(const Vector3& localDirection) const;
    Vector3 InverseTransformPoint(const Vector3& worldPoint) const;
    Vector3 InverseTransformDirection(const Vector3& worldDirection) const;
    
    // 批量变换（性能优化）
    void TransformPoints(const std::vector<Vector3>& localPoints, 
                        std::vector<Vector3>& worldPoints) const;
    void TransformDirections(const std::vector<Vector3>& localDirections,
                            std::vector<Vector3>& worldDirections) const;
};
```

---

## 构造函数

### Transform()

创建单位变换。

```cpp
Transform();
```

**说明**: 位置为原点，无旋转，缩放为1。

---

### Transform(position, rotation, scale)

创建指定变换。

```cpp
Transform(const Vector3& position, 
          const Quaternion& rotation = Quaternion::Identity(), 
          const Vector3& scale = Vector3::Ones());
```

**参数**:
- `position` - 初始位置
- `rotation` - 初始旋转（可选）
- `scale` - 初始缩放（可选）

---

## 位置操作

### SetPosition

设置本地位置。

```cpp
void SetPosition(const Vector3& position);
```

**参数**:
- `position` - 新的本地位置

---

### GetPosition

获取本地位置。

```cpp
const Vector3& GetPosition() const;
```

**返回值**: 本地位置的引用

**性能**: O(1) - 直接返回引用

---

### GetWorldPosition

获取世界位置。

```cpp
Vector3 GetWorldPosition() const;
```

**返回值**: 世界空间中的位置

**实现**: 实时计算（递归读取父节点时不持锁），仅在读取本地数据时短暂加锁。
**并发**: 线程安全，无死锁。

---

### Translate

在本地空间平移。

```cpp
void Translate(const Vector3& translation);
```

**参数**:
- `translation` - 平移向量（本地空间）

---

### TranslateWorld

在世界空间平移。

```cpp
void TranslateWorld(const Vector3& translation);
```

**参数**:
- `translation` - 平移向量（世界空间）

---

## 旋转操作

### SetRotation

设置本地旋转（四元数）。

```cpp
void SetRotation(const Quaternion& rotation);
```

**参数**:
- `rotation` - 新的旋转（会自动归一化）

---

### SetRotationEuler

设置本地旋转（欧拉角，弧度）。

```cpp
void SetRotationEuler(const Vector3& euler);
```

**参数**:
- `euler` - 欧拉角（弧度）

---

### SetRotationEulerDegrees

设置本地旋转（欧拉角，度数）。

```cpp
void SetRotationEulerDegrees(const Vector3& euler);
```

**参数**:
- `euler` - 欧拉角（度数）

**示例**:
```cpp
transform.SetRotationEulerDegrees(Vector3(0.0f, 45.0f, 0.0f));  // 绕Y轴旋转45度
```

---

### GetRotation

获取本地旋转。

```cpp
const Quaternion& GetRotation() const;
```

**返回值**: 本地旋转四元数的引用

---

### GetWorldRotation

获取世界旋转。

```cpp
Quaternion GetWorldRotation() const;
```

**返回值**: 世界空间中的旋转

**实现**: 实时计算（递归读取父节点时不持锁），仅在读取本地数据时短暂加锁。
**并发**: 线程安全，无死锁。

---

### Rotate

在本地空间旋转。

```cpp
void Rotate(const Quaternion& rotation);
```

**参数**:
- `rotation` - 旋转增量

---

### RotateAround

围绕轴旋转（本地空间）。

```cpp
void RotateAround(const Vector3& axis, float angle);
```

**参数**:
- `axis` - 旋转轴
- `angle` - 旋转角度（弧度）

---

### RotateAroundWorld

围绕轴旋转（世界空间）。

```cpp
void RotateAroundWorld(const Vector3& axis, float angle);
```

**参数**:
- `axis` - 旋转轴（世界空间）
- `angle` - 旋转角度（弧度）

---

### LookAt

使变换朝向目标点。

```cpp
void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
```

**参数**:
- `target` - 目标点（世界坐标）
- `up` - 上方向（可选）

**示例**:
```cpp
transform.LookAt(Vector3(0.0f, 0.0f, 0.0f));  // 朝向原点
```

---

## 缩放操作

### SetScale(Vector3)

设置本地缩放。

```cpp
void SetScale(const Vector3& scale);
```

**参数**:
- `scale` - 缩放向量（x, y, z）

---

### SetScale(float)

设置统一缩放。

```cpp
void SetScale(float scale);
```

**参数**:
- `scale` - 统一缩放值

**示例**:
```cpp
transform.SetScale(2.0f);  // 各轴都缩放2倍
```

---

### GetScale

获取本地缩放。

```cpp
const Vector3& GetScale() const;
```

**返回值**: 本地缩放的引用

---

### GetWorldScale

获取世界缩放。

```cpp
Vector3 GetWorldScale() const;
```

**返回值**: 世界空间中的缩放

---

## 方向向量

### GetForward

获取前方向（本地空间）。

```cpp
Vector3 GetForward() const;
```

**返回值**: 前方向向量（单位向量）

**默认**: Z 轴正方向

---

### GetRight

获取右方向（本地空间）。

```cpp
Vector3 GetRight() const;
```

**返回值**: 右方向向量（单位向量）

**默认**: X 轴正方向

---

### GetUp

获取上方向（本地空间）。

```cpp
Vector3 GetUp() const;
```

**返回值**: 上方向向量（单位向量）

**默认**: Y 轴正方向

---

## 矩阵操作

### GetLocalMatrix

获取本地变换矩阵。

```cpp
Matrix4 GetLocalMatrix() const;
```

**返回值**: 本地变换矩阵

**实现**: 实时计算（读取本地 TRS 并计算矩阵），仅在读取本地数据时短暂加锁。
**并发**: 线程安全。

---

### GetWorldMatrix

获取世界变换矩阵。

```cpp
Matrix4 GetWorldMatrix() const;
```

**返回值**: 世界变换矩阵

**实现**: 实时计算（先计算本地矩阵，再组合父矩阵）。读取父节点期间不持锁，避免锁级联。
**并发**: 线程安全，无死锁。

---

### SetFromMatrix

从矩阵设置变换。

```cpp
void SetFromMatrix(const Matrix4& matrix);
```

**参数**:
- `matrix` - 变换矩阵

**说明**: 自动分解为 TRS 分量。

---

## 父子关系

### SetParent

设置父变换。

```cpp
void SetParent(Transform* parent);
```

**参数**:
- `parent` - 父变换指针（nullptr 表示无父对象）

**说明**: 建立变换层级关系。

---

### GetParent

获取父变换。

```cpp
Transform* GetParent() const;
```

**返回值**: 父变换指针（可能为 nullptr）

---

### HasParent

检查是否有父变换。

```cpp
bool HasParent() const;
```

**返回值**: 如果有父变换返回 true

---

## 坐标变换

### TransformPoint

将点从本地空间变换到世界空间。

```cpp
Vector3 TransformPoint(const Vector3& localPoint) const;
```

**参数**:
- `localPoint` - 本地空间的点

**返回值**: 世界空间的点

**说明**: 考虑位置、旋转和缩放。

---

### TransformDirection

将方向从本地空间变换到世界空间。

```cpp
Vector3 TransformDirection(const Vector3& localDirection) const;
```

**参数**:
- `localDirection` - 本地空间的方向

**返回值**: 世界空间的方向

**说明**: 只考虑旋转，不考虑位置和缩放。

---

### InverseTransformPoint

将点从世界空间变换到本地空间。

```cpp
Vector3 InverseTransformPoint(const Vector3& worldPoint) const;
```

**参数**:
- `worldPoint` - 世界空间的点

**返回值**: 本地空间的点

---

### InverseTransformDirection

将方向从世界空间变换到本地空间。

```cpp
Vector3 InverseTransformDirection(const Vector3& worldDirection) const;
```

**参数**:
- `worldDirection` - 世界空间的方向

**返回值**: 本地空间的方向

---

## 批量变换操作

### TransformPoints

批量变换点从本地空间到世界空间。

```cpp
void TransformPoints(const std::vector<Vector3>& localPoints, 
                    std::vector<Vector3>& worldPoints) const;
```

**参数**:
- `localPoints` - 本地空间的点数组
- `worldPoints` - 输出的世界空间点数组

**性能优化**:
- 自动检测数据量
- >5000 个点时使用 OpenMP 并行处理
- 矩阵只获取一次

**示例**:
```cpp
std::vector<Vector3> localPoints = { /* ... */ };
std::vector<Vector3> worldPoints;
transform.TransformPoints(localPoints, worldPoints);  // 高效批量处理
```

---

### TransformDirections

批量变换方向从本地空间到世界空间。

```cpp
void TransformDirections(const std::vector<Vector3>& localDirections,
                        std::vector<Vector3>& worldDirections) const;
```

**参数**:
- `localDirections` - 本地空间的方向数组
- `worldDirections` - 输出的世界空间方向数组

**性能**: 大批量时自动并行处理。

---

## 使用示例

### 基础变换操作

```cpp
using namespace Render;

// 创建 Transform
Transform transform;
transform.SetPosition(Vector3(10.0f, 5.0f, 0.0f));
transform.SetRotationEulerDegrees(Vector3(0.0f, 45.0f, 0.0f));
transform.SetScale(2.0f);

// 查询变换信息
Vector3 pos = transform.GetPosition();        // 本地位置
Quaternion rot = transform.GetRotation();     // 本地旋转
Vector3 scale = transform.GetScale();          // 本地缩放
Matrix4 matrix = transform.GetLocalMatrix();   // 本地矩阵
```

### 方向操作

```cpp
// 获取方向向量
Vector3 forward = transform.GetForward();  // 前方向
Vector3 right = transform.GetRight();      // 右方向
Vector3 up = transform.GetUp();            // 上方向

// 朝向目标
transform.LookAt(Vector3(0.0f, 0.0f, 0.0f));
```

### 父子关系

```cpp
// 创建父对象
Transform parent;
parent.SetPosition(Vector3(10.0f, 0.0f, 0.0f));
parent.SetRotationEulerDegrees(Vector3(0.0f, 90.0f, 0.0f));

// 创建子对象
Transform child;
child.SetPosition(Vector3(5.0f, 0.0f, 0.0f));  // 相对于父对象
child.SetParent(&parent);

// 查询世界变换
Vector3 childWorldPos = child.GetWorldPosition();  // 考虑父对象的变换
Quaternion childWorldRot = child.GetWorldRotation();
```

### 坐标变换

```cpp
// 单个点变换
Vector3 localPoint(1.0f, 0.0f, 0.0f);
Vector3 worldPoint = transform.TransformPoint(localPoint);

// 方向变换（不受位置影响）
Vector3 localDir = Vector3::UnitX();
Vector3 worldDir = transform.TransformDirection(localDir);

// 反向变换
Vector3 localAgain = transform.InverseTransformPoint(worldPoint);
```

### 批量变换（高性能）

```cpp
// 准备大量数据
std::vector<Vector3> localPoints(10000);
for (int i = 0; i < 10000; ++i) {
    localPoints[i] = Vector3(i * 0.1f, 0.0f, 0.0f);
}

// 批量变换
std::vector<Vector3> worldPoints;
transform.TransformPoints(localPoints, worldPoints);

// 注意：大于5000个点时自动使用并行处理
```

---

## 性能特性

### 线程安全优先的实现

- 当前实现以线程安全与无死锁为最高优先级，移除了运行时缓存依赖，改为实时计算。
- 读取父节点世界变换时不持任何本地锁，避免锁级联与递归死锁。
- 读取本地成员（位置/旋转/缩放/父指针）时使用 `std::mutex` 短暂加锁。

### Dirty Flag 说明

- 接口仍保留 `dirty` 标志用于向后兼容，但当前实现不依赖这些标志进行缓存刷新。
- 这意味着查询接口始终得到“最新状态”，无需担心缓存过期。

### 批量处理优化

- 小批量（<5000）: 串行处理
- 大批量（≥5000）: OpenMP 并行处理
- 自动选择最优策略

---

## 父子关系注意事项

### 变换传播

```cpp
// 父对象变换会影响子对象的世界变换
parent.SetPosition(newPos);
Vector3 childWorldPos = child.GetWorldPosition();  // 自动更新
```

### 性能考虑

```cpp
// ✓ 好：批量修改后一次性查询
transform.SetPosition(pos);
transform.SetRotation(rot);
transform.SetScale(scale);
Matrix4 mat = transform.GetWorldMatrix();  // 所有修改一次性计算

// ✗ 避免：频繁修改和查询交替
for (int i = 0; i < 1000; ++i) {
    transform.SetPosition(somePos);
    Vector3 pos = transform.GetWorldPosition();  // 每次都重算
}
```

### 层级深度

- 实时计算对中等层级仍有良好性能；无父对象时，世界变换 = 本地变换。

---

## 坐标系统

### 默认坐标系

- **X 轴**: 右方向（Right）
- **Y 轴**: 上方向（Up）
- **Z 轴**: 前方向（Forward）

### 旋转顺序

欧拉角旋转顺序：**XYZ**（Pitch-Yaw-Roll）

---

## 内存和性能

### 内存使用

- Transform 对象大小：约 **235 bytes**
- 包含多层缓存以提升性能
- 内存-性能权衡：完全值得

### 性能基准

| 操作 | 性能 | 说明 |
|------|------|------|
| GetWorldPosition/Rotation | 3.5ns | 缓存命中时 |
| SetPosition/Rotation | <10ns | 仅标记 dirty |
| GetLocalMatrix | 50-100ns | 首次计算 |
| GetWorldMatrix | 60-120ns | 考虑父节点 |
| TransformPoints (10000) | 1.5ms | 串行 |
| TransformPoints (10000) | 0.4-0.8ms | 并行（理论值） |

---

## 最佳实践

### 1. 利用缓存

```cpp
// ✓ 好：重复查询缓存生效
for (int i = 0; i < iterations; ++i) {
    Vector3 pos = transform.GetWorldPosition();  // 极快
}
```

### 2. 批量操作

```cpp
// ✓ 好：大批量使用批量接口
transform.TransformPoints(localPoints, worldPoints);

// ✗ 避免：大批量逐个处理
for (const auto& p : localPoints) {
    worldPoints.push_back(transform.TransformPoint(p));
}
```

### 3. 建立层级

```cpp
// ✓ 好：先设置变换，再建立父子关系
child.SetPosition(localPos);
child.SetRotation(localRot);
child.SetParent(&parent);

// ✗ 避免：建立关系后频繁修改
child.SetParent(&parent);
for (int i = 0; i < 100; ++i) {
    child.SetPosition(positions[i]);  // 每次触发世界变换更新
}
```

### 4. 方向 vs 点

```cpp
// 变换点（考虑位置）
Vector3 worldPos = transform.TransformPoint(localPos);

// 变换方向（只考虑旋转）
Vector3 worldDir = transform.TransformDirection(localDir);
```

---

## 线程安全

✅ **Transform 现已线程安全**

- 使用 `std::mutex` 对本地数据的读取/写入进行短暂加锁。
- 计算世界位置/旋转/缩放/矩阵时，递归访问父节点在“无锁”状态进行，避免死锁。
- 常见并发模式（多读、多写、混合读写、父子并发）均通过实测：
  - 测试3：混合读写（8读+2写，500次迭代）稳定完成，无死锁
  - 测试4：父子并发访问稳定完成，无死锁
  - 压力测试：>2000ms 内完成 4.25e7 次操作

**建议**:
- 在高频并发读取的场景，尽量批量获取（如一次取矩阵后用于多次计算）。
- 避免在外部长时间持有自己的互斥锁后再调用 Transform，以免形成外部锁顺序问题。

---

## 另请参阅

- [MathUtils API](MathUtils.md) - 数学工具函数
- [Types API](Types.md) - 基础数学类型

---

[上一篇：MathUtils](MathUtils.md) | [下一篇：Carmera](Carmera.md)

