# Types API 参考

[返回 API 首页](README.md)

---

## 概述

定义引擎使用的数学类型和基础类型。

**头文件**: `render/types.h`  
**命名空间**: `Render`

---

## 数学类型（基于 Eigen）

### Vector2 / Vector3 / Vector4

```cpp
using Vector2 = Eigen::Vector2f;  // 2D 向量
using Vector3 = Eigen::Vector3f;  // 3D 向量
using Vector4 = Eigen::Vector4f;  // 4D 向量
```

**示例**:
```cpp
Vector3 position(1.0f, 2.0f, 3.0f);
Vector3 direction = Vector3(0.0f, 1.0f, 0.0f).normalized();
float length = position.norm();
Vector3 cross = Vector3::UnitX().cross(Vector3::UnitY());
```

---

### Matrix3 / Matrix4

```cpp
using Matrix3 = Eigen::Matrix3f;  // 3x3 矩阵
using Matrix4 = Eigen::Matrix4f;  // 4x4 矩阵
```

**示例**:
```cpp
// 单位矩阵
Matrix4 identity = Matrix4::Identity();

// 平移矩阵
Matrix4 translation = Matrix4::Identity();
translation(0, 3) = 5.0f;  // X 平移
translation(1, 3) = 2.0f;  // Y 平移

// 矩阵乘法
Matrix4 result = projMatrix * viewMatrix * modelMatrix;
```

---

### Quaternion

```cpp
using Quaternion = Eigen::Quaternionf;  // 四元数
```

**示例**:
```cpp
Quaternion rot = Quaternion(Eigen::AngleAxisf(angle, Vector3::UnitY()));
Matrix3 rotMatrix = rot.toRotationMatrix();
```

---

## 颜色类型

### Color

RGBA 颜色（每个分量 0.0~1.0）。

```cpp
struct Color {
    float r, g, b, a;
    
    Color(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
    
    static Color Red;
    static Color Green;
    static Color Blue;
    static Color White;
    static Color Black;
    static Color Yellow;
    static Color Cyan;
    static Color Magenta;
};
```

**示例**:
```cpp
Color red(1.0f, 0.0f, 0.0f, 1.0f);
Color semiTransparent(1.0f, 1.0f, 1.0f, 0.5f);

// 预定义颜色
Color white = Color::White;
Color black = Color::Black;
```

---

## 几何类型

### Rect

2D 矩形。

```cpp
struct Rect {
    float x, y, width, height;
    
    bool Contains(float px, float py) const;
    bool Intersects(const Rect& other) const;
};
```

---

### AABB

轴对齐包围盒。

```cpp
struct AABB {
    Vector3 min;
    Vector3 max;
    
    Vector3 GetCenter() const;
    Vector3 GetSize() const;
    bool Contains(const Vector3& point) const;
    bool Intersects(const AABB& other) const;
};
```

---

## 使用示例

```cpp
#include "render/types.h"

// 向量运算
Vector3 a(1.0f, 0.0f, 0.0f);
Vector3 b(0.0f, 1.0f, 0.0f);
Vector3 c = a.cross(b);  // (0, 0, 1)
float dot = a.dot(b);    // 0

// 矩阵
Matrix4 proj = Matrix4::Identity();
Matrix4 view = Matrix4::Identity();
Matrix4 mvp = proj * view;

// 颜色
Color orange(1.0f, 0.5f, 0.0f, 1.0f);
uniformMgr->SetColor("color", orange);

// 包围盒
AABB box;
box.min = Vector3(-1.0f, -1.0f, -1.0f);
box.max = Vector3(1.0f, 1.0f, 1.0f);
Vector3 center = box.GetCenter();  // (0, 0, 0)
```

---

[上一篇: FileUtils](FileUtils.md) | [返回 API 首页](README.md)

