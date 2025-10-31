#include "render/camera.h"
#include "render/logger.h"
#include "render/error.h"
#include <cmath>
#include <algorithm>

namespace Render {

// ============================================================================
// Frustum 实现
// ============================================================================

void Frustum::ExtractFromMatrix(const Matrix4& viewProjection) {
    // 从视图投影矩阵提取6个裁剪平面
    // 使用 Gribb-Hartmann 方法
    
    // 左平面：第4列 + 第1列
    planes[0].normal = Vector3(
        viewProjection(3, 0) + viewProjection(0, 0),
        viewProjection(3, 1) + viewProjection(0, 1),
        viewProjection(3, 2) + viewProjection(0, 2)
    );
    planes[0].distance = viewProjection(3, 3) + viewProjection(0, 3);
    
    // 右平面：第4列 - 第1列
    planes[1].normal = Vector3(
        viewProjection(3, 0) - viewProjection(0, 0),
        viewProjection(3, 1) - viewProjection(0, 1),
        viewProjection(3, 2) - viewProjection(0, 2)
    );
    planes[1].distance = viewProjection(3, 3) - viewProjection(0, 3);
    
    // 下平面：第4列 + 第2列
    planes[2].normal = Vector3(
        viewProjection(3, 0) + viewProjection(1, 0),
        viewProjection(3, 1) + viewProjection(1, 1),
        viewProjection(3, 2) + viewProjection(1, 2)
    );
    planes[2].distance = viewProjection(3, 3) + viewProjection(1, 3);
    
    // 上平面：第4列 - 第2列
    planes[3].normal = Vector3(
        viewProjection(3, 0) - viewProjection(1, 0),
        viewProjection(3, 1) - viewProjection(1, 1),
        viewProjection(3, 2) - viewProjection(1, 2)
    );
    planes[3].distance = viewProjection(3, 3) - viewProjection(1, 3);
    
    // 近平面：第4列 + 第3列
    planes[4].normal = Vector3(
        viewProjection(3, 0) + viewProjection(2, 0),
        viewProjection(3, 1) + viewProjection(2, 1),
        viewProjection(3, 2) + viewProjection(2, 2)
    );
    planes[4].distance = viewProjection(3, 3) + viewProjection(2, 3);
    
    // 远平面：第4列 - 第3列
    planes[5].normal = Vector3(
        viewProjection(3, 0) - viewProjection(2, 0),
        viewProjection(3, 1) - viewProjection(2, 1),
        viewProjection(3, 2) - viewProjection(2, 2)
    );
    planes[5].distance = viewProjection(3, 3) - viewProjection(2, 3);
    
    // 归一化所有平面
    for (int i = 0; i < 6; ++i) {
        float length = planes[i].normal.norm();
        if (length > MathUtils::EPSILON) {
            planes[i].normal /= length;
            planes[i].distance /= length;
        }
    }
}

bool Frustum::ContainsPoint(const Vector3& point) const {
    for (int i = 0; i < 6; ++i) {
        if (planes[i].GetDistance(point) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::IntersectsSphere(const Vector3& center, float radius) const {
    for (int i = 0; i < 6; ++i) {
        float distance = planes[i].GetDistance(center);
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

bool Frustum::IntersectsAABB(const AABB& aabb) const {
    for (int i = 0; i < 6; ++i) {
        const Plane& plane = planes[i];
        
        // 找到AABB相对于平面的正向顶点
        Vector3 positiveVertex(
            plane.normal.x() >= 0 ? aabb.max.x() : aabb.min.x(),
            plane.normal.y() >= 0 ? aabb.max.y() : aabb.min.y(),
            plane.normal.z() >= 0 ? aabb.max.z() : aabb.min.z()
        );
        
        if (plane.GetDistance(positiveVertex) < 0.0f) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Camera 实现
// ============================================================================

Camera::Camera()
    : m_projectionType(ProjectionType::Perspective)
    , m_fovYDegrees(60.0f)
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
    , m_orthoLeft(-10.0f)
    , m_orthoRight(10.0f)
    , m_orthoBottom(-10.0f)
    , m_orthoTop(10.0f)
    , m_projectionMatrix(Matrix4::Identity())
    , m_viewMatrix(Matrix4::Identity())
    , m_viewProjectionMatrix(Matrix4::Identity())
    , m_viewDirty(true)
    , m_projectionDirty(true)
    , m_viewProjectionDirty(true)
    , m_frustumDirty(true)
{
    // 初始化默认位置和朝向
    m_transform.SetPosition(Vector3(0.0f, 0.0f, 10.0f));
    m_transform.LookAt(Vector3::Zero());
    
    UpdateProjectionMatrix();
}

// ============================================================================
// 投影设置
// ============================================================================

void Camera::SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) {
    // 参数验证
    if (fovYDegrees <= 0.0f || fovYDegrees >= 180.0f) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Camera::SetPerspective: FOV 超出有效范围 (0, 180): " + 
                                   std::to_string(fovYDegrees)));
        fovYDegrees = std::clamp(fovYDegrees, 1.0f, 179.0f);
    }
    
    if (aspect <= 0.0f) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "Camera::SetPerspective: 宽高比必须大于 0: " + 
                                   std::to_string(aspect)));
        aspect = 1.0f;
    }
    
    if (nearPlane <= 0.0f || farPlane <= nearPlane) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "Camera::SetPerspective: 裁剪面参数无效 (near: " + 
                                   std::to_string(nearPlane) + ", far: " + std::to_string(farPlane) + ")"));
        nearPlane = std::max(0.01f, nearPlane);
        farPlane = std::max(nearPlane + 1.0f, farPlane);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_projectionType = ProjectionType::Perspective;
    m_fovYDegrees = fovYDegrees;
    m_aspectRatio = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    
    MarkProjectionDirty();
    UpdateProjectionMatrix();
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, 
                            float nearPlane, float farPlane) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_projectionType = ProjectionType::Orthographic;
    m_orthoLeft = left;
    m_orthoRight = right;
    m_orthoBottom = bottom;
    m_orthoTop = top;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    
    MarkProjectionDirty();
    UpdateProjectionMatrix();
}

void Camera::SetOrthographic(float width, float height, float nearPlane, float farPlane) {
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
}

ProjectionType Camera::GetProjectionType() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_projectionType;
}

void Camera::SetFieldOfView(float fovYDegrees) {
    if (fovYDegrees <= 0.0f || fovYDegrees >= 180.0f) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Camera::SetFieldOfView: FOV 超出有效范围: " + 
                                   std::to_string(fovYDegrees)));
        fovYDegrees = std::clamp(fovYDegrees, 1.0f, 179.0f);
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_projectionType != ProjectionType::Perspective) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Camera::SetFieldOfView: 相机不是透视模式"));
        return;
    }
    
    m_fovYDegrees = fovYDegrees;
    MarkProjectionDirty();
    UpdateProjectionMatrix();
}

float Camera::GetFieldOfView() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fovYDegrees;
}

void Camera::SetAspectRatio(float aspect) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aspectRatio = aspect;
    
    if (m_projectionType == ProjectionType::Perspective) {
        MarkProjectionDirty();
        UpdateProjectionMatrix();
    }
}

float Camera::GetAspectRatio() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_aspectRatio;
}

void Camera::SetNearPlane(float nearPlane) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nearPlane = nearPlane;
    MarkProjectionDirty();
    UpdateProjectionMatrix();
}

float Camera::GetNearPlane() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_nearPlane;
}

void Camera::SetFarPlane(float farPlane) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_farPlane = farPlane;
    MarkProjectionDirty();
    UpdateProjectionMatrix();
}

float Camera::GetFarPlane() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_farPlane;
}

// ============================================================================
// 变换操作
// ============================================================================

void Camera::SetPosition(const Vector3& position) {
    m_transform.SetPosition(position);
    MarkViewDirty();
}

Vector3 Camera::GetPosition() const {
    return m_transform.GetPosition();
}

void Camera::SetRotation(const Quaternion& rotation) {
    m_transform.SetRotation(rotation);
    MarkViewDirty();
}

Quaternion Camera::GetRotation() const {
    return m_transform.GetRotation();
}

void Camera::LookAt(const Vector3& target, const Vector3& up) {
    m_transform.LookAt(target, up);
    MarkViewDirty();
}

void Camera::Translate(const Vector3& translation) {
    m_transform.Translate(translation);
    MarkViewDirty();
}

void Camera::TranslateWorld(const Vector3& translation) {
    m_transform.TranslateWorld(translation);
    MarkViewDirty();
}

void Camera::Rotate(const Quaternion& rotation) {
    m_transform.Rotate(rotation);
    MarkViewDirty();
}

void Camera::RotateAround(const Vector3& axis, float angleDegrees) {
    m_transform.RotateAround(axis, MathUtils::DegreesToRadians(angleDegrees));
    MarkViewDirty();
}

// ============================================================================
// 方向向量
// ============================================================================

Vector3 Camera::GetForward() const {
    return m_transform.GetForward();
}

Vector3 Camera::GetRight() const {
    return m_transform.GetRight();
}

Vector3 Camera::GetUp() const {
    return m_transform.GetUp();
}

// ============================================================================
// 矩阵操作
// ============================================================================

Matrix4 Camera::GetViewMatrix() const {
    // Double-checked locking: 先检查脏标志（无锁），再加锁更新
    if (m_viewDirty.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 再次检查，避免多个线程重复更新
        if (m_viewDirty.load(std::memory_order_relaxed)) {
            UpdateViewMatrix();
        }
    }
    
    return m_viewMatrix;
}

Matrix4 Camera::GetProjectionMatrix() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_projectionMatrix;
}

Matrix4 Camera::GetViewProjectionMatrix() const {
    // Double-checked locking: 先检查脏标志（无锁），再加锁更新
    if (m_viewDirty.load(std::memory_order_acquire) || 
        m_viewProjectionDirty.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 再次检查并更新视图矩阵（如果需要）
        if (m_viewDirty.load(std::memory_order_relaxed)) {
            UpdateViewMatrix();
        }
        
        // 再次检查并更新视图投影矩阵（如果需要）
        if (m_viewProjectionDirty.load(std::memory_order_relaxed)) {
            UpdateViewProjectionMatrix();
        }
    }
    
    return m_viewProjectionMatrix;
}

// ============================================================================
// 视锥体操作
// ============================================================================

const Frustum& Camera::GetFrustum() const {
    // Double-checked locking: 先检查脏标志（无锁），再加锁更新
    if (m_frustumDirty.load(std::memory_order_acquire) || 
        m_viewDirty.load(std::memory_order_acquire) || 
        m_projectionDirty.load(std::memory_order_acquire) || 
        m_viewProjectionDirty.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 再次检查并更新（如果需要）
        if (m_viewDirty.load(std::memory_order_relaxed)) {
            UpdateViewMatrix();
        }
        if (m_viewProjectionDirty.load(std::memory_order_relaxed)) {
            UpdateViewProjectionMatrix();
        }
        
        // 再次检查并更新视锥体
        if (m_frustumDirty.load(std::memory_order_relaxed)) {
            m_frustum.ExtractFromMatrix(m_viewProjectionMatrix);
            m_frustumDirty.store(false, std::memory_order_release);
        }
    }
    
    return m_frustum;
}

void Camera::UpdateFrustum() {
    // 使用原子操作更新脏标志，不需要锁
    m_frustumDirty.store(true, std::memory_order_release);
}

// ============================================================================
// 坐标变换
// ============================================================================

Ray Camera::ScreenToWorldRay(float screenX, float screenY, 
                             float screenWidth, float screenHeight) const {
    // 转换到NDC空间（-1 到 1）
    float ndcX = (2.0f * screenX) / screenWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * screenY) / screenHeight;  // Y轴翻转
    
    // 获取矩阵
    Matrix4 projection = GetProjectionMatrix();
    Matrix4 view = GetViewMatrix();
    
    // 计算逆矩阵
    Matrix4 invProj = projection.inverse();
    Matrix4 invView = view.inverse();
    
    // NDC -> 裁剪空间
    Vector4 clipNear(ndcX, ndcY, -1.0f, 1.0f);
    Vector4 clipFar(ndcX, ndcY, 1.0f, 1.0f);
    
    // 裁剪空间 -> 视图空间
    Vector4 viewNear = invProj * clipNear;
    Vector4 viewFar = invProj * clipFar;
    viewNear /= viewNear.w();
    viewFar /= viewFar.w();
    
    // 视图空间 -> 世界空间
    Vector4 worldNear = invView * viewNear;
    Vector4 worldFar = invView * viewFar;
    
    Vector3 origin(worldNear.x(), worldNear.y(), worldNear.z());
    Vector3 target(worldFar.x(), worldFar.y(), worldFar.z());
    Vector3 direction = (target - origin).normalized();
    
    return Ray(origin, direction);
}

bool Camera::WorldToScreen(const Vector3& worldPos, float screenWidth, float screenHeight,
                          float& outScreenX, float& outScreenY) const {
    Matrix4 viewProj = GetViewProjectionMatrix();
    
    // 世界空间 -> 裁剪空间
    Vector4 clipPos = viewProj * Vector4(worldPos.x(), worldPos.y(), worldPos.z(), 1.0f);
    
    // 检查是否在相机前方
    if (clipPos.w() <= 0.0f) {
        return false;
    }
    
    // 裁剪空间 -> NDC
    Vector3 ndcPos(clipPos.x() / clipPos.w(), clipPos.y() / clipPos.w(), clipPos.z() / clipPos.w());
    
    // NDC -> 屏幕空间
    outScreenX = (ndcPos.x() + 1.0f) * 0.5f * screenWidth;
    outScreenY = (1.0f - ndcPos.y()) * 0.5f * screenHeight;  // Y轴翻转
    
    return true;
}

// ============================================================================
// 私有方法
// ============================================================================

void Camera::UpdateProjectionMatrix() {
    // 注意：调用者必须已经持有锁
    
    if (m_projectionType == ProjectionType::Perspective) {
        m_projectionMatrix = MathUtils::PerspectiveDegrees(
            m_fovYDegrees, m_aspectRatio, m_nearPlane, m_farPlane
        );
    } else {
        m_projectionMatrix = MathUtils::Orthographic(
            m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop,
            m_nearPlane, m_farPlane
        );
    }
    
    // 使用原子操作更新脏标志
    m_projectionDirty.store(false, std::memory_order_release);
    m_viewProjectionDirty.store(true, std::memory_order_release);
    m_frustumDirty.store(true, std::memory_order_release);
}

void Camera::MarkViewDirty() {
    // 使用原子操作更新脏标志，不需要锁
    m_viewDirty.store(true, std::memory_order_release);
    m_viewProjectionDirty.store(true, std::memory_order_release);
    m_frustumDirty.store(true, std::memory_order_release);
}

void Camera::MarkProjectionDirty() {
    // 使用原子操作更新脏标志（注意：调用者已经持有锁）
    m_projectionDirty.store(true, std::memory_order_release);
    m_viewProjectionDirty.store(true, std::memory_order_release);
    m_frustumDirty.store(true, std::memory_order_release);
}

void Camera::UpdateViewMatrix() const {
    // 注意：调用者必须已经持有锁
    
    // 相机的视图矩阵是其世界变换矩阵的逆矩阵
    Matrix4 worldMatrix = m_transform.GetWorldMatrix();
    m_viewMatrix = worldMatrix.inverse();
    
    // 使用原子操作更新脏标志
    m_viewDirty.store(false, std::memory_order_release);
    m_viewProjectionDirty.store(true, std::memory_order_release);
    m_frustumDirty.store(true, std::memory_order_release);
}

void Camera::UpdateViewProjectionMatrix() const {
    // 注意：调用者必须已经持有锁
    
    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
    
    // 使用原子操作更新脏标志
    m_viewProjectionDirty.store(false, std::memory_order_release);
    m_frustumDirty.store(true, std::memory_order_release);
}

// ============================================================================
// FirstPersonCameraController 实现
// ============================================================================

FirstPersonCameraController::FirstPersonCameraController(Camera* camera)
    : CameraController(camera)
{
    // 从相机当前旋转初始化偏航和俯仰角
    Vector3 forward = camera->GetForward();
    m_yaw = std::atan2(forward.x(), forward.z()) * MathUtils::RAD2DEG;
    m_pitch = std::asin(-forward.y()) * MathUtils::RAD2DEG;
}

void FirstPersonCameraController::Update(float deltaTime) {
    if (!m_camera) return;
    
    // 计算移动方向
    Vector3 movement = Vector3::Zero();
    
    if (m_moveForward) movement += m_camera->GetForward();
    if (m_moveBackward) movement -= m_camera->GetForward();
    if (m_moveRight) movement += m_camera->GetRight();
    if (m_moveLeft) movement -= m_camera->GetRight();
    if (m_moveUp) movement += Vector3::UnitY();
    if (m_moveDown) movement -= Vector3::UnitY();
    
    // 归一化并应用速度
    if (movement.squaredNorm() > MathUtils::EPSILON) {
        movement.normalize();
        movement *= m_moveSpeed * deltaTime;
        m_camera->TranslateWorld(movement);
    }
}

void FirstPersonCameraController::OnMouseMove(float deltaX, float deltaY) {
    if (!m_camera) return;
    
    // 更新旋转角度（调换左右方向）
    m_yaw -= deltaX * m_mouseSensitivity;  // 将+改为-，调换左右旋转方向
    m_pitch -= deltaY * m_mouseSensitivity;
    
    // 限制俯仰角
    m_pitch = MathUtils::Clamp(m_pitch, -89.0f, 89.0f);
    
    // 将欧拉角转换为四元数
    Quaternion rotation = MathUtils::FromEulerDegrees(m_pitch, m_yaw, 0.0f);
    m_camera->SetRotation(rotation);
}

void FirstPersonCameraController::OnMouseScroll(float delta) {
    // 第一人称相机通常不使用滚轮，但可以用来调整移动速度
    m_moveSpeed += delta * 0.5f;
    m_moveSpeed = MathUtils::Clamp(m_moveSpeed, 0.5f, 50.0f);
}

// ============================================================================
// OrbitCameraController 实现
// ============================================================================

OrbitCameraController::OrbitCameraController(Camera* camera, const Vector3& target)
    : CameraController(camera)
    , m_target(target)
{
    // 从当前相机位置计算初始距离和角度
    if (camera) {
        Vector3 cameraPos = camera->GetPosition();
        Vector3 offset = cameraPos - m_target;
        m_distance = offset.norm();
        
        if (m_distance > MathUtils::EPSILON) {
            m_yaw = std::atan2(offset.x(), offset.z()) * MathUtils::RAD2DEG;
            m_pitch = std::asin(offset.y() / m_distance) * MathUtils::RAD2DEG;
        }
    }
}

void OrbitCameraController::Update(float deltaTime) {
    // 轨道相机不需要在Update中做什么，所有更新在鼠标事件中完成
}

void OrbitCameraController::OnMouseMove(float deltaX, float deltaY) {
    if (!m_camera) return;
    
    // 更新旋转角度（调换左右方向）
    m_yaw -= deltaX * m_mouseSensitivity;  // 将+改为-，调换左右旋转方向
    m_pitch -= deltaY * m_mouseSensitivity;
    
    // 限制俯仰角
    m_pitch = MathUtils::Clamp(m_pitch, -89.0f, 89.0f);
    
    // 更新相机位置
    UpdateCameraPosition();
}

void OrbitCameraController::OnMouseScroll(float delta) {
    if (!m_camera) return;
    
    // 缩放距离
    m_distance -= delta * m_zoomSensitivity;
    m_distance = MathUtils::Clamp(m_distance, m_minDistance, m_maxDistance);
    
    // 更新相机位置
    UpdateCameraPosition();
}

void OrbitCameraController::SetTarget(const Vector3& target) {
    m_target = target;
    UpdateCameraPosition();
}

void OrbitCameraController::SetDistance(float distance) {
    m_distance = MathUtils::Clamp(distance, m_minDistance, m_maxDistance);
    UpdateCameraPosition();
}

void OrbitCameraController::UpdateCameraPosition() {
    if (!m_camera) return;
    
    // 计算相机位置
    float yawRad = m_yaw * MathUtils::DEG2RAD;
    float pitchRad = m_pitch * MathUtils::DEG2RAD;
    
    Vector3 offset(
        m_distance * std::cos(pitchRad) * std::sin(yawRad),
        m_distance * std::sin(pitchRad),
        m_distance * std::cos(pitchRad) * std::cos(yawRad)
    );
    
    m_camera->SetPosition(m_target + offset);
    m_camera->LookAt(m_target);
}

// ============================================================================
// ThirdPersonCameraController 实现
// ============================================================================

ThirdPersonCameraController::ThirdPersonCameraController(Camera* camera)
    : CameraController(camera)
{
    if (camera) {
        m_currentPosition = camera->GetPosition();
    }
}

void ThirdPersonCameraController::Update(float deltaTime) {
    if (!m_camera) return;
    
    UpdateCameraPosition(deltaTime);
}

void ThirdPersonCameraController::OnMouseMove(float deltaX, float deltaY) {
    // 更新旋转角度（调换左右方向）
    m_yaw -= deltaX * m_mouseSensitivity;  // 将+改为-，调换左右旋转方向
    m_pitch -= deltaY * m_mouseSensitivity;
    
    // 限制俯仰角
    m_pitch = MathUtils::Clamp(m_pitch, -89.0f, 89.0f);
}

void ThirdPersonCameraController::OnMouseScroll(float delta) {
    // 调整距离
    m_distance -= delta * 0.5f;
    m_distance = MathUtils::Clamp(m_distance, 1.0f, 20.0f);
}

void ThirdPersonCameraController::SetTarget(const Vector3& target) {
    m_target = target;
}

void ThirdPersonCameraController::SetDistance(float distance) {
    m_distance = MathUtils::Clamp(distance, 1.0f, 20.0f);
}

void ThirdPersonCameraController::UpdateCameraPosition(float deltaTime) {
    if (!m_camera) return;
    
    // 计算目标位置（带偏移）
    Vector3 targetPos = m_target + m_offset;
    
    // 计算相机偏移
    float yawRad = m_yaw * MathUtils::DEG2RAD;
    float pitchRad = m_pitch * MathUtils::DEG2RAD;
    
    Vector3 offset(
        m_distance * std::cos(pitchRad) * std::sin(yawRad),
        m_distance * std::sin(pitchRad),
        m_distance * std::cos(pitchRad) * std::cos(yawRad)
    );
    
    Vector3 desiredPosition = targetPos + offset;
    
    // 平滑插值到目标位置
    float smoothFactor = 1.0f - std::pow(m_smoothness, deltaTime);
    m_currentPosition = MathUtils::Lerp(m_currentPosition, desiredPosition, smoothFactor);
    
    // 更新相机
    m_camera->SetPosition(m_currentPosition);
    m_camera->LookAt(targetPos);
}

} // namespace Render

