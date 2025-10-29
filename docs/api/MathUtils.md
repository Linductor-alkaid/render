# MathUtils API 参考

[返回 API 首页](README.md)

---

## 概述

`MathUtils` 命名空间提供了一套完整的数学工具函数，包括角度转换、数值工具、向量操作、四元数操作和矩阵变换等功能。

**头文件**: `render/math_utils.h`  
**命名空间**: `Render::MathUtils`

---

## 常量定义

### 数学常量

```cpp
namespace MathUtils {
    constexpr float PI = 3.14159265358979323846f;        // 圆周率
    constexpr float TWO_PI = 6.28318530717958647692f;    // 2π
    constexpr float HALF_PI = 1.57079632679489661923f;   // π/2
    constexpr float DEG2RAD = 0.01745329251994329577f;   // 度转弧度系数
    constexpr float RAD2DEG = 57.2957795130823208768f;   // 弧度转度系数
    constexpr float EPSILON = 1e-6f;                     // 浮点数精度阈值
}
```

---

## 角度转换

### DegreesToRadians

将角度转换为弧度。

```cpp
inline float DegreesToRadians(float degrees);
```

**参数**:
- `degrees` - 角度值

**返回值**: 对应的弧度值

**示例**:
```cpp
float radians = MathUtils::DegreesToRadians(90.0f);  // 1.5708
```

---

### RadiansToDegrees

将弧度转换为角度。

```cpp
inline float RadiansToDegrees(float radians);
```

**参数**:
- `radians` - 弧度值

**返回值**: 对应的角度值

**示例**:
```cpp
float degrees = MathUtils::RadiansToDegrees(MathUtils::PI);  // 180.0
```

---

## 数值工具

### Clamp

将值限制在指定范围内。

```cpp
inline float Clamp(float value, float min, float max);
```

**参数**:
- `value` - 要限制的值
- `min` - 最小值
- `max` - 最大值

**返回值**: 限制后的值

**示例**:
```cpp
float result = MathUtils::Clamp(5.0f, 0.0f, 10.0f);  // 5.0
float result2 = MathUtils::Clamp(-1.0f, 0.0f, 10.0f);  // 0.0
```

---

### Lerp

线性插值。

```cpp
inline float Lerp(float a, float b, float t);
```

**参数**:
- `a` - 起始值
- `b` - 结束值
- `t` - 插值参数（0-1）

**返回值**: 插值结果

**示例**:
```cpp
float result = MathUtils::Lerp(0.0f, 10.0f, 0.5f);  // 5.0
```

---

### Smoothstep

平滑插值（S曲线）。

```cpp
inline float Smoothstep(float edge0, float edge1, float x);
```

**参数**:
- `edge0` - 下边界
- `edge1` - 上边界
- `x` - 插值位置

**返回值**: 平滑插值结果

**说明**: 使用 3x^2 - 2x^3 插值曲线，结果更平滑。

---

## 向量工具

### Lerp (Vector3)

向量线性插值。

```cpp
inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
```

**参数**:
- `a` - 起始向量
- `b` - 结束向量
- `t` - 插值参数（0-1）

**返回值**: 插值后的向量

---

### Distance

计算两点之间的距离。

```cpp
inline float Distance(const Vector3& a, const Vector3& b);
```

**参数**:
- `a` - 第一个点
- `b` - 第二个点

**返回值**: 两点之间的距离

---

### DistanceSquared

计算两点之间距离的平方。

```cpp
inline float DistanceSquared(const Vector3& a, const Vector3& b);
```

**说明**: 避免开方运算，用于距离比较时更高效。

---

### IsNormalized

检查向量是否已归一化。

```cpp
inline bool IsNormalized(const Vector3& v, float epsilon = EPSILON);
```

**参数**:
- `v` - 要检查的向量
- `epsilon` - 精度阈值（可选）

**返回值**: 如果向量已归一化返回 true

**性能提示**: 用于优化，避免重复归一化。

---

### SafeNormalize

安全归一化向量（优化版本）。

```cpp
inline Vector3 SafeNormalize(const Vector3& v);
```

**参数**:
- `v` - 要归一化的向量

**返回值**: 归一化后的向量（零向量返回 UnitX）

**性能优化**: 如果向量已归一化，直接返回，避免重复计算。

**示例**:
```cpp
Vector3 dir = MathUtils::SafeNormalize(someVector);  // 比 .normalized() 更快
```

---

### Project

将向量投影到另一向量上。

```cpp
inline Vector3 Project(const Vector3& vector, const Vector3& onNormal);
```

**参数**:
- `vector` - 要投影的向量
- `onNormal` - 投影目标向量

**返回值**: 投影结果

---

### Reflect

计算反射向量。

```cpp
inline Vector3 Reflect(const Vector3& vector, const Vector3& normal);
```

**参数**:
- `vector` - 入射向量
- `normal` - 反射面法向量

**返回值**: 反射向量

---

## 四元数工具

### AngleAxis

从旋转轴和角度创建四元数。

```cpp
inline Quaternion AngleAxis(float angle, const Vector3& axis);
```

**参数**:
- `angle` - 旋转角度（弧度）
- `axis` - 旋转轴（会自动归一化）

**返回值**: 表示旋转的四元数

**示例**:
```cpp
// 围绕 Y 轴旋转 90 度
Quaternion rot = MathUtils::AngleAxis(
    MathUtils::DegreesToRadians(90.0f), 
    Vector3::UnitY()
);
```

---

### FromEuler

从欧拉角创建四元数（弧度）。

```cpp
inline Quaternion FromEuler(float x, float y, float z);
```

**参数**:
- `x` - 绕 X 轴旋转角度（弧度）
- `y` - 绕 Y 轴旋转角度（弧度）
- `z` - 绕 Z 轴旋转角度（弧度）

**返回值**: 四元数

**旋转顺序**: XYZ

**性能优化**: 使用直接数学计算，比创建多个四元数快 40-50%。

---

### FromEulerDegrees

从欧拉角创建四元数（度数）。

```cpp
inline Quaternion FromEulerDegrees(float x, float y, float z);
```

**参数**:
- `x` - 绕 X 轴旋转角度（度数）
- `y` - 绕 Y 轴旋转角度（度数）
- `z` - 绕 Z 轴旋转角度（度数）

**返回值**: 四元数

**示例**:
```cpp
Quaternion rot = MathUtils::FromEulerDegrees(45.0f, 30.0f, 15.0f);
```

---

### ToEuler

将四元数转换为欧拉角（弧度）。

```cpp
inline Vector3 ToEuler(const Quaternion& q);
```

**参数**:
- `q` - 四元数

**返回值**: 欧拉角向量（x, y, z）（弧度）

---

### ToEulerDegrees

将四元数转换为欧拉角（度数）。

```cpp
inline Vector3 ToEulerDegrees(const Quaternion& q);
```

**参数**:
- `q` - 四元数

**返回值**: 欧拉角向量（x, y, z）（度数）

---

### Slerp

四元数球面线性插值。

```cpp
inline Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
```

**参数**:
- `a` - 起始四元数
- `b` - 结束四元数
- `t` - 插值参数（0-1）

**返回值**: 插值后的四元数

**说明**: 保持旋转速度恒定的插值方法。

---

### LookRotation

创建朝向指定方向的四元数。

```cpp
inline Quaternion LookRotation(const Vector3& forward, 
                               const Vector3& up = Vector3::UnitY());
```

**参数**:
- `forward` - 前方向（会自动归一化）
- `up` - 上方向（可选，默认为世界上方）

**返回值**: 表示朝向的四元数

**性能优化**: 使用 FromTwoVectors，比矩阵方法快 30-40%。

**示例**:
```cpp
// 创建朝向目标的旋转
Vector3 direction = (target - position).normalized();
Quaternion rot = MathUtils::LookRotation(direction);
```

---

## 矩阵变换工具

### Translate

创建平移矩阵。

```cpp
inline Matrix4 Translate(const Vector3& translation);
```

**参数**:
- `translation` - 平移向量

**返回值**: 4x4 平移矩阵

---

### Rotate

创建旋转矩阵。

```cpp
inline Matrix4 Rotate(const Quaternion& rotation);
```

**参数**:
- `rotation` - 旋转四元数

**返回值**: 4x4 旋转矩阵

---

### Scale

创建缩放矩阵。

```cpp
inline Matrix4 Scale(const Vector3& scale);
```

**参数**:
- `scale` - 缩放向量

**返回值**: 4x4 缩放矩阵

---

### TRS

创建完整的变换矩阵（平移-旋转-缩放）。

```cpp
inline Matrix4 TRS(const Vector3& position, 
                   const Quaternion& rotation, 
                   const Vector3& scale);
```

**参数**:
- `position` - 位置
- `rotation` - 旋转
- `scale` - 缩放

**返回值**: 4x4 变换矩阵

**变换顺序**: 先缩放，再旋转，最后平移

**性能优化**: 使用 Eigen::Affine3f 内部优化。

**示例**:
```cpp
Vector3 pos(10.0f, 5.0f, 0.0f);
Quaternion rot = MathUtils::FromEulerDegrees(0.0f, 45.0f, 0.0f);
Vector3 scale(2.0f, 1.0f, 1.0f);
Matrix4 transform = MathUtils::TRS(pos, rot, scale);
```

---

### GetPosition / GetRotation / GetScale

从矩阵中提取变换分量。

```cpp
inline Vector3 GetPosition(const Matrix4& matrix);
inline Quaternion GetRotation(const Matrix4& matrix);
inline Vector3 GetScale(const Matrix4& matrix);
```

**参数**:
- `matrix` - 变换矩阵

**返回值**: 对应的变换分量

**注意**: GetRotation 会移除缩放的影响。

---

### DecomposeMatrix

分解变换矩阵为 TRS 分量。

```cpp
inline void DecomposeMatrix(const Matrix4& matrix, 
                           Vector3& position, 
                           Quaternion& rotation, 
                           Vector3& scale);
```

**参数**:
- `matrix` - 要分解的矩阵
- `position` - 输出：位置
- `rotation` - 输出：旋转
- `scale` - 输出：缩放

**示例**:
```cpp
Vector3 pos, scale;
Quaternion rot;
MathUtils::DecomposeMatrix(transformMatrix, pos, rot, scale);
```

---

## 投影矩阵

### Perspective

创建透视投影矩阵。

```cpp
inline Matrix4 Perspective(float fovY, float aspect, float near, float far);
```

**参数**:
- `fovY` - 垂直视场角（弧度）
- `aspect` - 宽高比
- `near` - 近裁剪面距离
- `far` - 远裁剪面距离

**返回值**: 透视投影矩阵

---

### PerspectiveDegrees

创建透视投影矩阵（度数版本）。

```cpp
inline Matrix4 PerspectiveDegrees(float fovYDegrees, float aspect, 
                                  float near, float far);
```

**示例**:
```cpp
Matrix4 proj = MathUtils::PerspectiveDegrees(60.0f, 16.0f/9.0f, 0.1f, 100.0f);
```

---

### Orthographic

创建正交投影矩阵。

```cpp
inline Matrix4 Orthographic(float left, float right, 
                           float bottom, float top, 
                           float near, float far);
```

**参数**:
- `left` - 左边界
- `right` - 右边界
- `bottom` - 下边界
- `top` - 上边界
- `near` - 近裁剪面
- `far` - 远裁剪面

**返回值**: 正交投影矩阵

---

### LookAt

创建视图矩阵。

```cpp
inline Matrix4 LookAt(const Vector3& eye, 
                     const Vector3& center, 
                     const Vector3& up);
```

**参数**:
- `eye` - 相机位置
- `center` - 观察目标点
- `up` - 上方向

**返回值**: 视图矩阵

**示例**:
```cpp
Matrix4 view = MathUtils::LookAt(
    Vector3(0.0f, 5.0f, 10.0f),  // 相机位置
    Vector3(0.0f, 0.0f, 0.0f),   // 看向原点
    Vector3::UnitY()              // 上方向
);
```

---

## 使用示例

### 基础数学计算

```cpp
using namespace Render;
using namespace Render::MathUtils;

// 角度转换
float radians = DegreesToRadians(90.0f);
float degrees = RadiansToDegrees(PI);

// 数值工具
float clamped = Clamp(value, 0.0f, 1.0f);
float lerped = Lerp(startValue, endValue, 0.5f);

// 向量操作
float dist = Distance(pointA, pointB);
Vector3 projected = Project(vector, axis);
Vector3 reflected = Reflect(incidence, normal);
```

### 四元数操作

```cpp
// 创建旋转
Quaternion rot1 = AngleAxis(DegreesToRadians(45.0f), Vector3::UnitY());
Quaternion rot2 = FromEulerDegrees(45.0f, 30.0f, 15.0f);

// 朝向目标
Vector3 forward = (target - position).normalized();
Quaternion lookRot = LookRotation(forward);

// 四元数插值
Quaternion interpolated = Slerp(startRot, endRot, 0.5f);

// 转换为欧拉角
Vector3 euler = ToEulerDegrees(rot1);
```

### 矩阵变换

```cpp
// 创建变换矩阵
Vector3 pos(10.0f, 5.0f, 0.0f);
Quaternion rot = FromEulerDegrees(0.0f, 45.0f, 0.0f);
Vector3 scale(2.0f, 2.0f, 2.0f);
Matrix4 transform = TRS(pos, rot, scale);

// 分解矩阵
Vector3 extractedPos, extractedScale;
Quaternion extractedRot;
DecomposeMatrix(transform, extractedPos, extractedRot, extractedScale);

// 创建投影矩阵
Matrix4 projection = PerspectiveDegrees(60.0f, 16.0f/9.0f, 0.1f, 100.0f);
Matrix4 view = LookAt(cameraPos, targetPos, Vector3::UnitY());
```

### 性能优化技巧

```cpp
// 1. 使用 SafeNormalize 代替 normalized()（对可能已归一化的向量）
Vector3 dir = SafeNormalize(forward);  // 快 30%

// 2. 使用 DistanceSquared 比较距离（避免开方）
if (DistanceSquared(a, b) < radiusSquared) {
    // 在范围内
}

// 3. 缓存常用计算结果
Matrix4 proj = PerspectiveDegrees(60.0f, aspect, 0.1f, 100.0f);
// 多次使用 proj，而不是每次都重新计算
```

---

## 性能特性

### 优化亮点

- **FromEuler**: 直接数学计算，比临时对象方法快 **40-50%**
- **LookRotation**: 使用 FromTwoVectors，比矩阵方法快 **30-40%**
- **SafeNormalize**: 对已归一化向量快 **30%**
- **TRS**: 使用 Affine3f 优化，快 **10-20%**

### SIMD 加速

所有向量和矩阵运算都使用 Eigen 库，在启用 AVX2 时自动使用 SIMD 指令加速。

---

## 最佳实践

1. **使用 SafeNormalize**: 在可能已归一化的向量上使用
2. **缓存计算结果**: 投影矩阵等不变的计算结果应该缓存
3. **选择合适的函数**: 度数版本更直观，弧度版本更底层
4. **注意旋转顺序**: FromEuler 使用 XYZ 顺序

---

## 另请参阅

- [Transform API](Transform.md) - Transform 类
- [Types API](Types.md) - 基础数学类型

---

[上一篇：Types](Types.md) | [下一篇：Transform](Transform.md)

