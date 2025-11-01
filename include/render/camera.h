#pragma once

#include "types.h"
#include "math_utils.h"
#include "transform.h"
#include <mutex>
#include <atomic>

namespace Render {

/**
 * @brief 投影类型枚举
 */
enum class ProjectionType {
    Perspective,    // 透视投影
    Orthographic    // 正交投影
};

/**
 * @brief 视锥体结构
 * 
 * 用于视锥体裁剪和可见性检测
 */
struct Frustum {
    Plane planes[6];  // 6个裁剪平面：左、右、下、上、近、远
    
    /**
     * @brief 从视图投影矩阵提取视锥体
     * @param viewProjection 视图投影矩阵
     */
    void ExtractFromMatrix(const Matrix4& viewProjection);
    
    /**
     * @brief 检测点是否在视锥体内
     * @param point 待检测的点
     * @return 如果点在视锥体内返回 true
     */
    bool ContainsPoint(const Vector3& point) const;
    
    /**
     * @brief 检测球体是否与视锥体相交
     * @param center 球体中心
     * @param radius 球体半径
     * @return 如果相交返回 true
     */
    bool IntersectsSphere(const Vector3& center, float radius) const;
    
    /**
     * @brief 检测AABB是否与视锥体相交
     * @param aabb 轴对齐包围盒
     * @return 如果相交返回 true
     */
    bool IntersectsAABB(const AABB& aabb) const;
};

/**
 * @class Camera
 * @brief 相机类
 * 
 * 提供透视和正交投影，支持视图变换和视锥体裁剪。
 * 线程安全设计，可在多线程环境下使用。
 */
class Camera {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    /**
     * @brief 构造函数
     */
    Camera();
    
    /**
     * @brief 析构函数
     */
    ~Camera() = default;
    
    // ========================================================================
    // 投影设置
    // ========================================================================
    
    /**
     * @brief 设置透视投影
     * @param fovYDegrees 垂直视场角（度数）
     * @param aspect 宽高比
     * @param nearPlane 近裁剪面距离
     * @param farPlane 远裁剪面距离
     */
    void SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
    
    /**
     * @brief 设置正交投影
     * @param left 左边界
     * @param right 右边界
     * @param bottom 下边界
     * @param top 上边界
     * @param nearPlane 近裁剪面
     * @param farPlane 远裁剪面
     */
    void SetOrthographic(float left, float right, float bottom, float top, 
                        float nearPlane, float farPlane);
    
    /**
     * @brief 设置正交投影（简化版）
     * @param width 宽度
     * @param height 高度
     * @param nearPlane 近裁剪面
     * @param farPlane 远裁剪面
     */
    void SetOrthographic(float width, float height, float nearPlane, float farPlane);
    
    /**
     * @brief 获取投影类型
     */
    ProjectionType GetProjectionType() const;
    
    /**
     * @brief 设置视场角（仅透视投影）
     * @param fovYDegrees 垂直视场角（度数）
     */
    void SetFieldOfView(float fovYDegrees);
    
    /**
     * @brief 获取视场角（仅透视投影）
     */
    float GetFieldOfView() const;
    
    /**
     * @brief 设置宽高比
     * @param aspect 宽高比
     */
    void SetAspectRatio(float aspect);
    
    /**
     * @brief 获取宽高比
     */
    float GetAspectRatio() const;
    
    /**
     * @brief 设置近裁剪面
     * @param nearPlane 近裁剪面距离
     */
    void SetNearPlane(float nearPlane);
    
    /**
     * @brief 获取近裁剪面
     */
    float GetNearPlane() const;
    
    /**
     * @brief 设置远裁剪面
     * @param farPlane 远裁剪面距离
     */
    void SetFarPlane(float farPlane);
    
    /**
     * @brief 获取远裁剪面
     */
    float GetFarPlane() const;
    
    // ========================================================================
    // 变换操作
    // ========================================================================
    
    /**
     * @brief 设置相机位置
     * @param position 世界空间位置
     */
    void SetPosition(const Vector3& position);
    
    /**
     * @brief 获取相机位置
     */
    Vector3 GetPosition() const;
    
    /**
     * @brief 设置相机旋转
     * @param rotation 旋转四元数
     */
    void SetRotation(const Quaternion& rotation);
    
    /**
     * @brief 获取相机旋转
     */
    Quaternion GetRotation() const;
    
    /**
     * @brief 相机朝向目标点
     * @param target 目标位置
     * @param up 上方向（默认为世界上方）
     */
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY());
    
    /**
     * @brief 平移相机（本地空间）
     * @param translation 平移向量
     */
    void Translate(const Vector3& translation);
    
    /**
     * @brief 平移相机（世界空间）
     * @param translation 平移向量
     */
    void TranslateWorld(const Vector3& translation);
    
    /**
     * @brief 旋转相机
     * @param rotation 旋转增量
     */
    void Rotate(const Quaternion& rotation);
    
    /**
     * @brief 围绕轴旋转相机
     * @param axis 旋转轴（本地空间）
     * @param angleDegrees 旋转角度（度数）
     */
    void RotateAround(const Vector3& axis, float angleDegrees);
    
    // ========================================================================
    // 方向向量
    // ========================================================================
    
    /**
     * @brief 获取前方向（相机朝向）
     */
    Vector3 GetForward() const;
    
    /**
     * @brief 获取右方向
     */
    Vector3 GetRight() const;
    
    /**
     * @brief 获取上方向
     */
    Vector3 GetUp() const;
    
    // ========================================================================
    // 矩阵操作
    // ========================================================================
    
    /**
     * @brief 获取视图矩阵
     * @return 视图矩阵
     */
    Matrix4 GetViewMatrix() const;
    
    /**
     * @brief 获取投影矩阵
     * @return 投影矩阵
     */
    Matrix4 GetProjectionMatrix() const;
    
    /**
     * @brief 获取视图投影矩阵
     * @return 视图投影矩阵
     */
    Matrix4 GetViewProjectionMatrix() const;
    
    // ========================================================================
    // 视锥体操作
    // ========================================================================
    
    /**
     * @brief 获取视锥体
     * @return 视锥体
     */
    const Frustum& GetFrustum() const;
    
    /**
     * @brief 更新视锥体（通常在投影或视图改变后自动调用）
     */
    void UpdateFrustum();
    
    // ========================================================================
    // 坐标变换
    // ========================================================================
    
    /**
     * @brief 屏幕坐标转世界射线
     * @param screenX 屏幕X坐标（像素）
     * @param screenY 屏幕Y坐标（像素）
     * @param screenWidth 屏幕宽度（像素）
     * @param screenHeight 屏幕高度（像素）
     * @return 世界空间射线
     */
    Ray ScreenToWorldRay(float screenX, float screenY, 
                        float screenWidth, float screenHeight) const;
    
    /**
     * @brief 世界坐标转屏幕坐标
     * @param worldPos 世界空间位置
     * @param screenWidth 屏幕宽度（像素）
     * @param screenHeight 屏幕高度（像素）
     * @param outScreenX 输出：屏幕X坐标
     * @param outScreenY 输出：屏幕Y坐标
     * @return 如果点在相机前方返回 true
     */
    bool WorldToScreen(const Vector3& worldPos, float screenWidth, float screenHeight,
                      float& outScreenX, float& outScreenY) const;
    
    // ========================================================================
    // 其他
    // ========================================================================
    
    /**
     * @brief 获取变换对象（可用于高级操作）
     */
    Transform& GetTransform() { return m_transform; }
    const Transform& GetTransform() const { return m_transform; }
    
private:
    // 更新投影矩阵（线程安全）
    void UpdateProjectionMatrix();
    
    // 标记缓存失效
    void MarkViewDirty();
    void MarkProjectionDirty();
    
    // 更新缓存的矩阵（线程安全，内部使用）
    void UpdateViewMatrix() const;
    void UpdateViewProjectionMatrix() const;
    
private:
    // 变换
    Transform m_transform;
    
    // 投影参数
    ProjectionType m_projectionType;
    float m_fovYDegrees;         // 透视：视场角（度数）
    float m_aspectRatio;         // 宽高比
    float m_nearPlane;           // 近裁剪面
    float m_farPlane;            // 远裁剪面
    
    // 正交投影参数
    float m_orthoLeft, m_orthoRight;
    float m_orthoBottom, m_orthoTop;
    
    // 缓存的矩阵
    mutable Matrix4 m_projectionMatrix;
    mutable Matrix4 m_viewMatrix;
    mutable Matrix4 m_viewProjectionMatrix;
    
    // 视锥体
    mutable Frustum m_frustum;
    
    // 脏标志（使用原子操作以支持 double-checked locking）
    mutable std::atomic<bool> m_viewDirty;
    mutable std::atomic<bool> m_projectionDirty;
    mutable std::atomic<bool> m_viewProjectionDirty;
    mutable std::atomic<bool> m_frustumDirty;
    
    // 线程安全
    mutable std::mutex m_mutex;
};

/**
 * @brief 相机控制器基类
 * 
 * @note 线程安全：控制器不是线程安全的，应在同一线程中使用
 */
class CameraController {
public:
    /**
     * @brief 构造函数
     * @param camera 相机引用（不能为nullptr）
     * @throw std::invalid_argument 如果camera为nullptr
     */
    CameraController(Camera* camera) : m_camera(camera) {
        if (!camera) {
            throw std::invalid_argument("CameraController: camera cannot be nullptr");
        }
    }
    virtual ~CameraController() = default;
    
    /**
     * @brief 更新控制器（每帧调用）
     * @param deltaTime 帧间隔时间（秒）
     */
    virtual void Update(float deltaTime) = 0;
    
    /**
     * @brief 处理鼠标移动
     * @param deltaX 鼠标X轴增量
     * @param deltaY 鼠标Y轴增量
     */
    virtual void OnMouseMove(float deltaX, float deltaY) = 0;
    
    /**
     * @brief 处理鼠标滚轮
     * @param delta 滚轮增量
     */
    virtual void OnMouseScroll(float delta) = 0;
    
    /**
     * @brief 设置移动速度
     */
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    float GetMoveSpeed() const { return m_moveSpeed; }
    
    /**
     * @brief 设置旋转速度
     */
    void SetRotateSpeed(float speed) { m_rotateSpeed = speed; }
    float GetRotateSpeed() const { return m_rotateSpeed; }
    
    Camera* GetCamera() { return m_camera; }
    const Camera* GetCamera() const { return m_camera; }
    
protected:
    Camera* m_camera;
    float m_moveSpeed = 5.0f;      // 移动速度（单位/秒）
    float m_rotateSpeed = 90.0f;   // 旋转速度（度/秒）
};

/**
 * @brief 第一人称相机控制器
 * 
 * 典型FPS游戏风格的相机控制
 */
class FirstPersonCameraController : public CameraController {
public:
    FirstPersonCameraController(Camera* camera);
    
    void Update(float deltaTime) override;
    void OnMouseMove(float deltaX, float deltaY) override;
    void OnMouseScroll(float delta) override;
    
    // 移动控制（WASD + QE）
    void SetMoveForward(bool active) { m_moveForward = active; }
    void SetMoveBackward(bool active) { m_moveBackward = active; }
    void SetMoveLeft(bool active) { m_moveLeft = active; }
    void SetMoveRight(bool active) { m_moveRight = active; }
    void SetMoveUp(bool active) { m_moveUp = active; }
    void SetMoveDown(bool active) { m_moveDown = active; }
    
    // 设置鼠标灵敏度
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    float GetMouseSensitivity() const { return m_mouseSensitivity; }
    
private:
    // 输入状态
    bool m_moveForward = false;
    bool m_moveBackward = false;
    bool m_moveLeft = false;
    bool m_moveRight = false;
    bool m_moveUp = false;
    bool m_moveDown = false;
    
    // 旋转角度
    float m_yaw = 0.0f;    // 偏航角（度）
    float m_pitch = 0.0f;  // 俯仰角（度）
    
    float m_mouseSensitivity = 0.1f;
};

/**
 * @brief 轨道相机控制器
 * 
 * 围绕目标点旋转的相机，类似3D建模软件
 */
class OrbitCameraController : public CameraController {
    // 确保正确的内存对齐以使用 Eigen 的 SIMD 优化
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    OrbitCameraController(Camera* camera, const Vector3& target = Vector3::Zero());
    
    void Update(float deltaTime) override;
    void OnMouseMove(float deltaX, float deltaY) override;
    void OnMouseScroll(float delta) override;
    
    // 设置目标点
    void SetTarget(const Vector3& target);
    Vector3 GetTarget() const { return m_target; }
    
    // 设置距离
    void SetDistance(float distance);
    float GetDistance() const { return m_distance; }
    
    // 设置距离限制
    void SetDistanceRange(float minDist, float maxDist) {
        m_minDistance = minDist;
        m_maxDistance = maxDist;
    }
    
    // 设置鼠标灵敏度
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    float GetMouseSensitivity() const { return m_mouseSensitivity; }
    
    // 设置缩放灵敏度
    void SetZoomSensitivity(float sensitivity) { m_zoomSensitivity = sensitivity; }
    float GetZoomSensitivity() const { return m_zoomSensitivity; }
    
private:
    void UpdateCameraPosition();
    
    Vector3 m_target;
    float m_distance = 10.0f;
    float m_minDistance = 1.0f;
    float m_maxDistance = 100.0f;
    
    float m_yaw = 0.0f;    // 偏航角（度）
    float m_pitch = 30.0f; // 俯仰角（度）
    
    float m_mouseSensitivity = 0.2f;
    float m_zoomSensitivity = 1.0f;
};

/**
 * @brief 第三人称相机控制器
 * 
 * 跟随目标的相机，用于第三人称游戏
 */
class ThirdPersonCameraController : public CameraController {
    // 确保正确的内存对齐以使用 Eigen 的 SIMD 优化
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    ThirdPersonCameraController(Camera* camera);
    
    void Update(float deltaTime) override;
    void OnMouseMove(float deltaX, float deltaY) override;
    void OnMouseScroll(float delta) override;
    
    // 设置跟随目标
    void SetTarget(const Vector3& target);
    Vector3 GetTarget() const { return m_target; }
    
    // 设置跟随偏移
    void SetOffset(const Vector3& offset) { m_offset = offset; }
    Vector3 GetOffset() const { return m_offset; }
    
    // 设置距离
    void SetDistance(float distance);
    float GetDistance() const { return m_distance; }
    
    // 设置跟随平滑度（0-1，越大越平滑）
    void SetSmoothness(float smoothness) { m_smoothness = MathUtils::Clamp(smoothness, 0.0f, 1.0f); }
    float GetSmoothness() const { return m_smoothness; }
    
    // 设置鼠标灵敏度
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    float GetMouseSensitivity() const { return m_mouseSensitivity; }
    
private:
    void UpdateCameraPosition(float deltaTime);
    
    Vector3 m_target;
    Vector3 m_offset = Vector3(0.0f, 2.0f, 0.0f);  // 相对于目标的偏移
    float m_distance = 5.0f;
    
    float m_yaw = 0.0f;
    float m_pitch = 15.0f;
    
    float m_smoothness = 0.1f;
    float m_mouseSensitivity = 0.2f;
    
    // 当前相机位置（用于平滑插值）
    Vector3 m_currentPosition;
};

} // namespace Render

