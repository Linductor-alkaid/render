# Camera API 参考

[返回 API 首页](README.md)

---

## 概述

`Camera` 类提供了完整的3D相机功能，包括透视和正交投影、视图变换、视锥体裁剪等。支持多种相机控制模式（第一人称、轨道、第三人称）。

**头文件**: `render/camera.h`  
**命名空间**: `Render`

**线程安全**: ✅ 是（使用互斥锁和原子操作优化）

---

## 类定义

```cpp
class Camera {
public:
    Camera();
    ~Camera() = default;
    
    // 投影设置
    void SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
    void SetOrthographic(float left, float right, float bottom, float top, 
                        float nearPlane, float farPlane);
    void SetOrthographic(float width, float height, float nearPlane, float farPlane);
    
    ProjectionType GetProjectionType() const;
    void SetFieldOfView(float fovYDegrees);
    float GetFieldOfView() const;
    void SetAspectRatio(float aspect);
    float GetAspectRatio() const;
    void SetNearPlane(float nearPlane);
    float GetNearPlane() const;
    void SetFarPlane(float farPlane);
    float GetFarPlane() const;
    
    // 变换操作
    void SetPosition(const Vector3& position);
    Vector3 GetPosition() const;
    void SetRotation(const Quaternion& rotation);
    Quaternion GetRotation() const;
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
    void Translate(const Vector3& translation);
    void TranslateWorld(const Vector3& translation);
    void Rotate(const Quaternion& rotation);
    void RotateAround(const Vector3& axis, float angleDegrees);
    
    // 方向向量
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;
    
    // 矩阵操作
    Matrix4 GetViewMatrix() const;
    Matrix4 GetProjectionMatrix() const;
    Matrix4 GetViewProjectionMatrix() const;
    
    // 视锥体操作
    const Frustum& GetFrustum() const;
    void UpdateFrustum();
    
    // 坐标变换
    Ray ScreenToWorldRay(float screenX, float screenY, 
                        float screenWidth, float screenHeight) const;
    bool WorldToScreen(const Vector3& worldPos, float screenWidth, float screenHeight,
                      float& outScreenX, float& outScreenY) const;
    
    // 其他
    Transform& GetTransform();
    const Transform& GetTransform() const;
};
```

---

## 投影类型

### ProjectionType 枚举

```cpp
enum class ProjectionType {
    Perspective,    // 透视投影
    Orthographic    // 正交投影
};
```

---

## 构造函数

### Camera()

创建默认相机。

```cpp
Camera();
```

**默认配置**:
- 投影类型：透视投影
- 视场角：60度
- 宽高比：16:9
- 近裁剪面：0.1
- 远裁剪面：1000.0
- 位置：(0, 0, 10)
- 朝向：看向原点

---

## 投影设置

### SetPerspective

设置透视投影。

```cpp
void SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
```

**参数**:
- `fovYDegrees` - 垂直视场角（度数）
- `aspect` - 宽高比（width / height）
- `nearPlane` - 近裁剪面距离
- `farPlane` - 远裁剪面距离

**示例**:
```cpp
camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
```

**线程安全**: ✅ 是

---

### SetOrthographic

设置正交投影。

```cpp
// 方式1：指定六个边界
void SetOrthographic(float left, float right, float bottom, float top, 
                    float nearPlane, float farPlane);

// 方式2：指定宽度和高度（自动居中）
void SetOrthographic(float width, float height, float nearPlane, float farPlane);
```

**参数**:
- `left` - 左边界
- `right` - 右边界
- `bottom` - 下边界
- `top` - 上边界
- `width` - 宽度
- `height` - 高度
- `nearPlane` - 近裁剪面
- `farPlane` - 远裁剪面

**示例**:
```cpp
// 方式1
camera.SetOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

// 方式2（更简单）
camera.SetOrthographic(20.0f, 20.0f, 0.1f, 100.0f);
```

**线程安全**: ✅ 是

---

### GetProjectionType

获取当前投影类型。

```cpp
ProjectionType GetProjectionType() const;
```

**返回值**: 投影类型

**线程安全**: ✅ 是

---

### SetFieldOfView / GetFieldOfView

设置/获取视场角（仅透视投影）。

```cpp
void SetFieldOfView(float fovYDegrees);
float GetFieldOfView() const;
```

**参数**:
- `fovYDegrees` - 垂直视场角（度数）

**注意**: 仅在透视投影模式下有效。

**线程安全**: ✅ 是

---

### SetAspectRatio / GetAspectRatio

设置/获取宽高比。

```cpp
void SetAspectRatio(float aspect);
float GetAspectRatio() const;
```

**参数**:
- `aspect` - 宽高比（width / height）

**线程安全**: ✅ 是

---

### SetNearPlane / GetNearPlane / SetFarPlane / GetFarPlane

设置/获取裁剪面距离。

```cpp
void SetNearPlane(float nearPlane);
float GetNearPlane() const;
void SetFarPlane(float farPlane);
float GetFarPlane() const;
```

**线程安全**: ✅ 是

---

## 变换操作

### SetPosition / GetPosition

设置/获取相机位置。

```cpp
void SetPosition(const Vector3& position);
Vector3 GetPosition() const;
```

**示例**:
```cpp
camera.SetPosition(Vector3(0.0f, 5.0f, 10.0f));
Vector3 pos = camera.GetPosition();
```

**线程安全**: ✅ 是（内部使用 Transform）

---

### SetRotation / GetRotation

设置/获取相机旋转。

```cpp
void SetRotation(const Quaternion& rotation);
Quaternion GetRotation() const;
```

**线程安全**: ✅ 是

---

### LookAt

使相机朝向目标点。

```cpp
void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
```

**参数**:
- `target` - 目标点（世界坐标）
- `up` - 上方向（默认为世界上方）

**示例**:
```cpp
camera.SetPosition(Vector3(10.0f, 5.0f, 10.0f));
camera.LookAt(Vector3::Zero());  // 朝向原点
```

**线程安全**: ✅ 是

---

### Translate / TranslateWorld

平移相机。

```cpp
void Translate(const Vector3& translation);       // 本地空间
void TranslateWorld(const Vector3& translation);  // 世界空间
```

**参数**:
- `translation` - 平移向量

**示例**:
```cpp
// 本地空间平移（相对于相机朝向）
camera.Translate(Vector3(0.0f, 0.0f, 1.0f));  // 向前移动

// 世界空间平移
camera.TranslateWorld(Vector3(1.0f, 0.0f, 0.0f));  // 向世界X轴正方向移动
```

**线程安全**: ✅ 是

---

### Rotate / RotateAround

旋转相机。

```cpp
void Rotate(const Quaternion& rotation);
void RotateAround(const Vector3& axis, float angleDegrees);
```

**参数**:
- `rotation` - 旋转增量（四元数）
- `axis` - 旋转轴
- `angleDegrees` - 旋转角度（度数）

**线程安全**: ✅ 是

---

## 方向向量

### GetForward / GetRight / GetUp

获取相机的方向向量。

```cpp
Vector3 GetForward() const;  // 前方向（相机朝向）
Vector3 GetRight() const;    // 右方向
Vector3 GetUp() const;       // 上方向
```

**返回值**: 单位方向向量

**示例**:
```cpp
Vector3 forward = camera.GetForward();
Vector3 moveDir = forward * moveSpeed * deltaTime;
camera.TranslateWorld(moveDir);
```

**线程安全**: ✅ 是

---

## 矩阵操作

### GetViewMatrix

获取视图矩阵。

```cpp
Matrix4 GetViewMatrix() const;
```

**返回值**: 4x4 视图矩阵

**说明**: 视图矩阵是相机变换矩阵的逆矩阵，用于将世界空间变换到相机空间。

**性能**: 使用缓存机制，重复调用很快。

**线程安全**: ✅ 是

---

### GetProjectionMatrix

获取投影矩阵。

```cpp
Matrix4 GetProjectionMatrix() const;
```

**返回值**: 4x4 投影矩阵

**说明**: 将相机空间变换到裁剪空间。

**性能**: 使用缓存。

**线程安全**: ✅ 是

---

### GetViewProjectionMatrix

获取视图投影矩阵。

```cpp
Matrix4 GetViewProjectionMatrix() const;
```

**返回值**: 4x4 视图投影矩阵

**说明**: `projection * view`，用于一次性从世界空间变换到裁剪空间。

**性能**: 使用缓存。

**线程安全**: ✅ 是

---

## 视锥体操作

### Frustum 结构

```cpp
struct Frustum {
    Plane planes[6];  // 6个裁剪平面：左、右、下、上、近、远
    
    void ExtractFromMatrix(const Matrix4& viewProjection);
    bool ContainsPoint(const Vector3& point) const;
    bool IntersectsSphere(const Vector3& center, float radius) const;
    bool IntersectsAABB(const AABB& aabb) const;
};
```

---

### GetFrustum

获取视锥体。

```cpp
const Frustum& GetFrustum() const;
```

**返回值**: 视锥体引用

**说明**: 自动更新，可用于视锥体裁剪优化。

**线程安全**: ✅ 是

---

### UpdateFrustum

手动更新视锥体。

```cpp
void UpdateFrustum();
```

**说明**: 通常不需要手动调用，视锥体会自动更新。

**线程安全**: ✅ 是

---

## 坐标变换

### ScreenToWorldRay

屏幕坐标转世界射线。

```cpp
Ray ScreenToWorldRay(float screenX, float screenY, 
                    float screenWidth, float screenHeight) const;
```

**参数**:
- `screenX` - 屏幕X坐标（像素）
- `screenY` - 屏幕Y坐标（像素）
- `screenWidth` - 屏幕宽度（像素）
- `screenHeight` - 屏幕高度（像素）

**返回值**: 从相机发出的射线

**用途**: 鼠标拾取、点击检测等。

**安全性增强（2025-11-02）**:
- ✅ 自动检查投影矩阵和视图矩阵的可逆性（行列式）
- ✅ 检查齐次坐标转换的有效性
- ✅ 检查射线方向的有效性
- ✅ 失败时返回安全的默认射线（相机位置 + 前方向）
- ✅ 不会因为奇异矩阵而崩溃

**示例**:
```cpp
// 鼠标点击拾取
int mouseX, mouseY;
SDL_GetMouseState(&mouseX, &mouseY);

Ray ray = camera.ScreenToWorldRay(mouseX, mouseY, 1280.0f, 720.0f);

// 检测与物体相交
float t;
if (ray.IntersectPlane(groundPlane, t)) {
    Vector3 hitPoint = ray.GetPoint(t);
    Logger::Info("点击位置: ({}, {}, {})", hitPoint.x(), hitPoint.y(), hitPoint.z());
}
```

**线程安全**: ✅ 是

---

### WorldToScreen

世界坐标转屏幕坐标。

```cpp
bool WorldToScreen(const Vector3& worldPos, float screenWidth, float screenHeight,
                  float& outScreenX, float& outScreenY) const;
```

**参数**:
- `worldPos` - 世界空间位置
- `screenWidth` - 屏幕宽度（像素）
- `screenHeight` - 屏幕高度（像素）
- `outScreenX` - 输出：屏幕X坐标
- `outScreenY` - 输出：屏幕Y坐标

**返回值**: 如果点在相机前方返回 true，否则返回 false

**用途**: 3D物体标签、UI元素定位等。

**示例**:
```cpp
Vector3 objectPos(5.0f, 2.0f, 0.0f);
float screenX, screenY;

if (camera.WorldToScreen(objectPos, 1280.0f, 720.0f, screenX, screenY)) {
    // 在屏幕位置 (screenX, screenY) 绘制标签
    DrawLabel(screenX, screenY, "Object");
}
```

**线程安全**: ✅ 是

---

## 相机控制器

### CameraController 基类

```cpp
class CameraController {
public:
    CameraController(Camera& camera);  // 使用引用，不是指针
    virtual ~CameraController() = default;
    
    virtual void Update(float deltaTime) = 0;
    virtual void OnMouseMove(float deltaX, float deltaY) = 0;
    virtual void OnMouseScroll(float delta) = 0;
    
    void SetMoveSpeed(float speed);
    float GetMoveSpeed() const;
    void SetRotateSpeed(float speed);
    float GetRotateSpeed() const;
    
    Camera& GetCamera();
    const Camera& GetCamera() const;
};
```

**重要变更（2025-11-02）**:
- ✅ 构造函数改用 `Camera&`（引用）而非 `Camera*`（指针）
- ✅ 消除了空指针风险
- ✅ 语义更清晰：控制器不拥有 Camera 的所有权
- ⚠️ 调用者必须确保 Camera 对象的生命周期长于控制器

**线程安全**: ⚠️ 控制器本身不是线程安全的，应在同一线程中使用（但 Camera 本身是线程安全的）

---

### FirstPersonCameraController

第一人称相机控制器（FPS风格）。

```cpp
class FirstPersonCameraController : public CameraController {
public:
    FirstPersonCameraController(Camera& camera);  // 使用引用
    
    // 移动控制
    void SetMoveForward(bool active);
    void SetMoveBackward(bool active);
    void SetMoveLeft(bool active);
    void SetMoveRight(bool active);
    void SetMoveUp(bool active);
    void SetMoveDown(bool active);
    
    // 鼠标灵敏度
    void SetMouseSensitivity(float sensitivity);
    float GetMouseSensitivity() const;
};
```

**特点**:
- WASD 移动
- QE 上下移动
- 鼠标控制视角
- 俯仰角限制在 [-89°, 89°]

**示例**:
```cpp
Camera camera;
FirstPersonCameraController controller(camera);  // 传引用，不是指针
controller.SetMoveSpeed(10.0f);
controller.SetMouseSensitivity(0.15f);

// 游戏循环
while (running) {
    // 处理输入
    if (keyPressed[SDLK_W]) controller.SetMoveForward(true);
    if (keyPressed[SDLK_S]) controller.SetMoveBackward(true);
    // ...
    
    // 更新
    controller.Update(deltaTime);
}
```

**注意**:
- ✅ 正确：`FirstPersonCameraController controller(camera);`
- ❌ 错误：`FirstPersonCameraController controller(&camera);` （虽然会自动转换，但不推荐）

---

### OrbitCameraController

轨道相机控制器（3D建模软件风格）。

```cpp
class OrbitCameraController : public CameraController {
public:
    OrbitCameraController(Camera& camera, const Vector3& target = Vector3::Zero());
    
    void SetTarget(const Vector3& target);
    Vector3 GetTarget() const;
    void SetDistance(float distance);
    float GetDistance() const;
    void SetDistanceRange(float minDist, float maxDist);
    void SetMouseSensitivity(float sensitivity);
    void SetZoomSensitivity(float sensitivity);
};
```

**特点**:
- 围绕目标点旋转
- 鼠标拖动旋转视角
- 滚轮缩放距离
- 俯仰角限制

**示例**:
```cpp
Camera camera;
OrbitCameraController controller(camera, Vector3::Zero());  // 传引用
controller.SetDistance(15.0f);
controller.SetDistanceRange(5.0f, 50.0f);
controller.SetMouseSensitivity(0.3f);

// 鼠标拖动
if (mousePressed) {
    controller.OnMouseMove(mouseDeltaX, mouseDeltaY);
}

// 滚轮缩放
controller.OnMouseScroll(wheelDelta);
```

---

### ThirdPersonCameraController

第三人称相机控制器。

```cpp
class ThirdPersonCameraController : public CameraController {
public:
    ThirdPersonCameraController(Camera& camera);  // 使用引用
    
    void SetTarget(const Vector3& target);
    Vector3 GetTarget() const;
    void SetOffset(const Vector3& offset);
    Vector3 GetOffset() const;
    void SetDistance(float distance);
    float GetDistance() const;
    void SetSmoothness(float smoothness);  // 0-1
    float GetSmoothness() const;
    void SetMouseSensitivity(float sensitivity);
};
```

**特点**:
- 跟随目标
- 平滑插值
- 可调整偏移和距离
- 鼠标控制视角

**示例**:
```cpp
Camera camera;
ThirdPersonCameraController controller(camera);  // 传引用
controller.SetDistance(10.0f);
controller.SetOffset(Vector3(0.0f, 2.0f, 0.0f));
controller.SetSmoothness(0.1f);

// 更新目标位置（玩家位置）
controller.SetTarget(playerPosition);

// 每帧更新
controller.Update(deltaTime);  // 自动平滑跟随
```

---

## 使用示例

### 基础相机设置

```cpp
using namespace Render;

// 创建相机
Camera camera;
camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
camera.SetPosition(Vector3(0.0f, 5.0f, 10.0f));
camera.LookAt(Vector3::Zero());

// 渲染时使用
Matrix4 view = camera.GetViewMatrix();
Matrix4 projection = camera.GetProjectionMatrix();

shader->Use();
shader->SetMatrix4("uView", view);
shader->SetMatrix4("uProjection", projection);
```

---

### 透视投影 vs 正交投影

```cpp
// 透视投影（3D游戏）
camera.SetPerspective(60.0f, aspect, 0.1f, 1000.0f);

// 正交投影（2D游戏、UI）
camera.SetOrthographic(screenWidth, screenHeight, -1.0f, 1.0f);

// 运行时切换
if (keyPressed[SDLK_P]) {
    if (camera.GetProjectionType() == ProjectionType::Perspective) {
        camera.SetOrthographic(20.0f, 15.0f, 0.1f, 1000.0f);
    } else {
        camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }
}
```

---

### 第一人称相机示例

```cpp
Camera camera;
FirstPersonCameraController controller(camera);  // 使用引用
controller.SetMoveSpeed(10.0f);

while (running) {
    // 输入处理
    if (keyDown[SDLK_W]) controller.SetMoveForward(true);
    if (keyDown[SDLK_S]) controller.SetMoveBackward(true);
    if (keyDown[SDLK_A]) controller.SetMoveLeft(true);
    if (keyDown[SDLK_D]) controller.SetMoveRight(true);
    
    if (mousePressed) {
        controller.OnMouseMove(mouseDeltaX, mouseDeltaY);
    }
    
    // 更新
    controller.Update(deltaTime);
    
    // 渲染
    RenderScene(camera);
}
```

---

### 视锥体裁剪优化

```cpp
const Frustum& frustum = camera.GetFrustum();

// 检测物体是否可见
for (auto& object : objects) {
    AABB bounds = object.GetBounds();
    
    if (frustum.IntersectsAABB(bounds)) {
        // 可见，渲染
        object.Render();
    }
    // 不可见，跳过渲染（节省性能）
}
```

---

### 鼠标拾取示例

```cpp
// 鼠标点击事件
if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    // 屏幕转世界射线
    Ray ray = camera.ScreenToWorldRay(mouseX, mouseY, 
                                      screenWidth, screenHeight);
    
    // 检测与地面相交
    Plane groundPlane(Vector3::UnitY(), 0.0f);
    float t;
    if (ray.IntersectPlane(groundPlane, t)) {
        Vector3 hitPoint = ray.GetPoint(t);
        Logger::Info("点击地面位置: ({}, {}, {})", 
                    hitPoint.x(), hitPoint.y(), hitPoint.z());
        
        // 在点击位置放置物体
        PlaceObject(hitPoint);
    }
}
```

---

## 性能特性

### 缓存机制

相机系统使用智能缓存：

1. **视图矩阵缓存**: 只在变换改变时重新计算
2. **投影矩阵缓存**: 只在投影参数改变时重新计算
3. **视图投影矩阵缓存**: 复用上述两个矩阵
4. **视锥体缓存**: 只在矩阵改变时更新

**性能提升**: 相比每次重算，缓存可提升 **10-50倍** 性能。

---

### 脏标志机制

相机使用 **原子脏标志 + Double-Checked Locking** 优化：

```cpp
// 修改操作只标记脏标志（原子操作，无锁）
camera.SetPosition(pos);      // 标记视图矩阵为 dirty
camera.SetRotation(rot);      // 标记视图矩阵为 dirty

// 获取时才真正计算
Matrix4 view = camera.GetViewMatrix();  // 计算并缓存
Matrix4 view2 = camera.GetViewMatrix(); // 直接返回缓存（极快，无锁检查）
```

**优化说明**:
- 使用 `std::atomic<bool>` 存储脏标志
- 采用 Double-Checked Locking 模式
- 第一次检查脏标志时无需加锁（快速路径）
- 只有在需要更新时才加锁（慢速路径）
- 多线程环境下避免不必要的锁竞争
- 符合 C++ 内存模型，使用 `memory_order_acquire` 和 `memory_order_release`

---

## 线程安全

✅ **Camera 类是线程安全的**

- 所有公共方法都使用互斥锁保护
- 可以在多个线程中安全调用
- 注意：OpenGL 渲染必须在主线程

**性能优化**:
- 使用原子操作优化脏标志检查
- 采用 Double-Checked Locking 减少锁竞争
- 只读操作（缓存命中时）几乎无锁开销
- 多线程环境下性能优异

**内存模型保证**:
- 使用 `std::atomic<bool>` 确保可见性
- `memory_order_acquire` / `memory_order_release` 确保正确同步
- 符合 C++ 内存模型标准

**示例**:
```cpp
// 线程1：更新相机
std::thread updateThread([&camera]() {
    camera.SetPosition(newPosition);  // 安全，原子标记脏标志
});

// 主线程：渲染
Matrix4 view = camera.GetViewMatrix();  // 安全，快速路径无锁检查

// 多个线程同时读取（常见场景）
std::thread t1([&camera]() {
    Matrix4 v1 = camera.GetViewMatrix();  // 快速，无锁检查
});
std::thread t2([&camera]() {
    Matrix4 v2 = camera.GetViewMatrix();  // 快速，无锁检查
});
```

---

## 最佳实践

### 1. 窗口大小改变时更新宽高比

```cpp
void OnWindowResize(int width, int height) {
    float aspect = (float)width / (float)height;
    camera.SetAspectRatio(aspect);
}
```

### 2. 选择合适的裁剪面距离

```cpp
// ✓ 好：合理的范围
camera.SetPerspective(60.0f, aspect, 0.1f, 1000.0f);

// ✗ 避免：近裁剪面太近或远裁剪面太远
camera.SetPerspective(60.0f, aspect, 0.001f, 1000000.0f);  // Z-fighting
```

### 3. 使用视锥体裁剪优化渲染

```cpp
// 大场景中，只渲染可见物体
const Frustum& frustum = camera.GetFrustum();
for (auto& obj : objects) {
    if (frustum.IntersectsSphere(obj.position, obj.radius)) {
        obj.Render();
    }
}
```

### 4. 相机平滑移动

```cpp
// 使用插值实现平滑相机
Vector3 currentPos = camera.GetPosition();
Vector3 targetPos = player.GetPosition() + offset;
Vector3 newPos = MathUtils::Lerp(currentPos, targetPos, smoothFactor * deltaTime);
camera.SetPosition(newPos);
```

### 5. 相机控制器的生命周期管理

```cpp
// ✅ 正确：Camera 对象的生命周期长于控制器
{
    Camera camera;
    FirstPersonCameraController controller(camera);
    // 使用 controller...
}  // camera 和 controller 同时销毁，安全

// ✅ 正确：使用智能指针
auto camera = std::make_shared<Camera>();
auto controller = std::make_unique<FirstPersonCameraController>(*camera);

// ❌ 错误：Camera 对象先销毁
FirstPersonCameraController* CreateController() {
    Camera camera;  // 栈上对象
    return new FirstPersonCameraController(camera);  // 悬空引用！
}

// ✅ 正确：使用堆分配或确保生命周期
Camera* CreateCameraAndController(FirstPersonCameraController** outController) {
    Camera* camera = new Camera();
    *outController = new FirstPersonCameraController(*camera);
    return camera;
}
```

### 6. 多相机模式切换

```cpp
Camera camera;
std::unique_ptr<CameraController> currentController;

// 切换到第一人称
void SwitchToFirstPerson() {
    currentController = std::make_unique<FirstPersonCameraController>(camera);
    currentController->SetMoveSpeed(10.0f);
}

// 切换到轨道
void SwitchToOrbit() {
    currentController = std::make_unique<OrbitCameraController>(camera);
}

// 每帧更新
if (currentController) {
    currentController->Update(deltaTime);
}
```

---

## 另请参阅

- [Transform API](Transform.md) - 变换系统
- [MathUtils API](MathUtils.md) - 数学工具
- [Types API](Types.md) - 基础类型（Ray, Plane, AABB等）

---

[上一篇：Transform](Transform.md) | [下一篇：Shader](Shader.md)

