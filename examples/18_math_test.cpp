/**
 * @file 18_math_test.cpp
 * @brief 测试数学库功能：Transform、数学工具函数、Ray、Plane
 */

#include <render/renderer.h>
#include <render/logger.h>
#include <render/transform.h>
#include <render/math_utils.h>
#include <iostream>
#include <sstream>

using namespace Render;

// 辅助函数：打印 Vector3
void PrintVector3(const std::string& name, const Vector3& v) {
    std::ostringstream oss;
    oss << name << ": (" << v.x() << ", " << v.y() << ", " << v.z() << ")";
    LOG_INFO(oss.str());
}

// 辅助函数：打印 Quaternion（欧拉角）
void PrintQuaternion(const std::string& name, const Quaternion& q) {
    Vector3 euler = MathUtils::ToEulerDegrees(q);
    std::ostringstream oss;
    oss << name << ": (" << euler.x() << ", " << euler.y() << ", " << euler.z() << ") degrees";
    LOG_INFO(oss.str());
}

// 辅助函数：打印 Matrix4
void PrintMatrix4(const std::string& name, const Matrix4& m) {
    LOG_INFO(name);
    for (int i = 0; i < 4; ++i) {
        std::ostringstream oss;
        oss << "  [" << m(i, 0) << ", " << m(i, 1) << ", " << m(i, 2) << ", " << m(i, 3) << "]";
        LOG_INFO(oss.str());
    }
}

// 测试基础数学工具函数
void TestMathUtils() {
    LOG_INFO("========================================");
    LOG_INFO("测试数学工具函数");
    LOG_INFO("========================================");
    
    // 角度转换
    float degrees = 90.0f;
    float radians = MathUtils::DegreesToRadians(degrees);
    std::ostringstream oss;
    oss << "90 度 = " << radians << " 弧度";
    LOG_INFO(oss.str());
    
    oss.str("");
    oss << radians << " 弧度 = " << MathUtils::RadiansToDegrees(radians) << " 度";
    LOG_INFO(oss.str());
    
    // 数值工具
    oss.str("");
    oss << "Clamp(5.0, 0.0, 10.0) = " << MathUtils::Clamp(5.0f, 0.0f, 10.0f);
    LOG_INFO(oss.str());
    
    oss.str("");
    oss << "Clamp(-1.0, 0.0, 10.0) = " << MathUtils::Clamp(-1.0f, 0.0f, 10.0f);
    LOG_INFO(oss.str());
    
    oss.str("");
    oss << "Lerp(0.0, 10.0, 0.5) = " << MathUtils::Lerp(0.0f, 10.0f, 0.5f);
    LOG_INFO(oss.str());
    
    oss.str("");
    oss << "Smoothstep(0.0, 1.0, 0.5) = " << MathUtils::Smoothstep(0.0f, 1.0f, 0.5f);
    LOG_INFO(oss.str());
    
    // 向量工具
    Vector3 v1(1.0f, 0.0f, 0.0f);
    Vector3 v2(0.0f, 1.0f, 0.0f);
    Vector3 lerped = MathUtils::Lerp(v1, v2, 0.5f);
    PrintVector3("Lerp((1,0,0), (0,1,0), 0.5)", lerped);
    
    oss.str("");
    oss << "Distance((0,0,0), (1,1,1)) = " << MathUtils::Distance(Vector3::Zero(), Vector3::Ones());
    LOG_INFO(oss.str());
    
    Vector3 projected = MathUtils::Project(Vector3(1.0f, 1.0f, 0.0f), Vector3::UnitX());
    PrintVector3("Project((1,1,0) onto X-axis)", projected);
    
    Vector3 reflected = MathUtils::Reflect(Vector3(1.0f, -1.0f, 0.0f), Vector3::UnitY());
    PrintVector3("Reflect((1,-1,0) on Y-plane)", reflected);
    
    LOG_INFO("");
}

// 测试四元数功能
void TestQuaternion() {
    LOG_INFO("========================================");
    LOG_INFO("测试四元数功能");
    LOG_INFO("========================================");
    
    // 创建旋转
    Quaternion q1 = MathUtils::AngleAxis(MathUtils::DegreesToRadians(90.0f), Vector3::UnitY());
    PrintQuaternion("围绕Y轴旋转90度", q1);
    
    // 欧拉角转换
    Quaternion q2 = MathUtils::FromEulerDegrees(45.0f, 30.0f, 15.0f);
    PrintQuaternion("从欧拉角创建 (45, 30, 15)", q2);
    
    // 四元数插值
    Quaternion q3 = MathUtils::Slerp(Quaternion::Identity(), q1, 0.5f);
    PrintQuaternion("Slerp(Identity, q1, 0.5)", q3);
    
    // LookRotation
    Quaternion lookRot = MathUtils::LookRotation(Vector3(1.0f, 0.0f, 1.0f).normalized());
    PrintQuaternion("LookRotation((1,0,1))", lookRot);
    
    LOG_INFO("");
}

// 测试矩阵变换
void TestMatrixTransforms() {
    LOG_INFO("========================================");
    LOG_INFO("测试矩阵变换");
    LOG_INFO("========================================");
    
    // TRS 矩阵
    Vector3 pos(5.0f, 3.0f, 2.0f);
    Quaternion rot = MathUtils::FromEulerDegrees(0.0f, 45.0f, 0.0f);
    Vector3 scale(2.0f, 1.0f, 1.0f);
    
    Matrix4 trs = MathUtils::TRS(pos, rot, scale);
    PrintMatrix4("TRS 矩阵", trs);
    
    // 分解矩阵
    Vector3 extractedPos, extractedScale;
    Quaternion extractedRot;
    MathUtils::DecomposeMatrix(trs, extractedPos, extractedRot, extractedScale);
    
    PrintVector3("提取的位置", extractedPos);
    PrintQuaternion("提取的旋转", extractedRot);
    PrintVector3("提取的缩放", extractedScale);
    
    // 投影矩阵
    Matrix4 proj = MathUtils::PerspectiveDegrees(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    PrintMatrix4("透视投影矩阵 (FOV=60)", proj);
    
    // 视图矩阵
    Matrix4 view = MathUtils::LookAt(
        Vector3(0.0f, 5.0f, 10.0f),
        Vector3(0.0f, 0.0f, 0.0f),
        Vector3::UnitY()
    );
    PrintMatrix4("LookAt 视图矩阵", view);
    
    LOG_INFO("");
}

// 测试 Transform 类
void TestTransform() {
    LOG_INFO("========================================");
    LOG_INFO("测试 Transform 类");
    LOG_INFO("========================================");
    
    // 创建 Transform
    Transform transform;
    transform.SetPosition(Vector3(10.0f, 5.0f, 0.0f));
    transform.SetRotationEulerDegrees(Vector3(0.0f, 90.0f, 0.0f));
    transform.SetScale(Vector3(2.0f, 2.0f, 2.0f));
    
    PrintVector3("位置", transform.GetPosition());
    PrintQuaternion("旋转", transform.GetRotation());
    PrintVector3("缩放", transform.GetScale());
    
    // 获取方向向量
    PrintVector3("前方向", transform.GetForward());
    PrintVector3("右方向", transform.GetRight());
    PrintVector3("上方向", transform.GetUp());
    
    // 变换矩阵
    Matrix4 localMat = transform.GetLocalMatrix();
    PrintMatrix4("本地变换矩阵", localMat);
    
    // 平移和旋转
    transform.Translate(Vector3(0.0f, 2.0f, 0.0f));
    transform.RotateAround(Vector3::UnitY(), MathUtils::DegreesToRadians(45.0f));
    PrintVector3("平移后的位置", transform.GetPosition());
    PrintQuaternion("旋转后的方向", transform.GetRotation());
    
    // 坐标变换
    Vector3 localPoint(1.0f, 0.0f, 0.0f);
    Vector3 worldPoint = transform.TransformPoint(localPoint);
    PrintVector3("本地点 (1,0,0) 的世界坐标", worldPoint);
    
    // LookAt
    transform.LookAt(Vector3(0.0f, 0.0f, 0.0f));
    PrintQuaternion("LookAt(0,0,0) 后的旋转", transform.GetRotation());
    
    LOG_INFO("");
}

// 测试父子关系
void TestHierarchy() {
    LOG_INFO("========================================");
    LOG_INFO("测试父子变换层级");
    LOG_INFO("========================================");
    
    // 创建父对象
    Transform parent;
    parent.SetPosition(Vector3(10.0f, 0.0f, 0.0f));
    parent.SetRotationEulerDegrees(Vector3(0.0f, 90.0f, 0.0f));
    parent.SetScale(2.0f);
    
    // 创建子对象
    Transform child;
    child.SetPosition(Vector3(5.0f, 0.0f, 0.0f));  // 相对于父对象的位置
    child.SetParent(&parent);
    
    PrintVector3("父对象位置", parent.GetPosition());
    PrintVector3("子对象本地位置", child.GetPosition());
    PrintVector3("子对象世界位置", child.GetWorldPosition());
    
    // 子对象的世界矩阵
    Matrix4 childWorldMat = child.GetWorldMatrix();
    PrintMatrix4("子对象世界矩阵", childWorldMat);
    
    // 验证世界位置
    Vector3 extractedPos = MathUtils::GetPosition(childWorldMat);
    PrintVector3("从世界矩阵提取的位置", extractedPos);
    
    LOG_INFO("");
}

// 测试 Plane 和 Ray
void TestPlaneAndRay() {
    LOG_INFO("========================================");
    LOG_INFO("测试 Plane 和 Ray");
    LOG_INFO("========================================");
    
    // 创建平面（地面，Y=0）
    Plane groundPlane(Vector3::UnitY(), 0.0f);
    PrintVector3("地面平面法向量", groundPlane.normal);
    
    std::ostringstream oss;
    oss << "地面平面距离: " << groundPlane.distance;
    LOG_INFO(oss.str());
    
    // 测试点到平面的距离
    Vector3 point1(5.0f, 3.0f, 2.0f);
    float distance = groundPlane.GetDistance(point1);
    oss.str("");
    oss << "点 (5,3,2) 到地面的距离: " << distance;
    LOG_INFO(oss.str());
    
    // 创建射线
    Ray ray(Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f));
    PrintVector3("射线起点", ray.origin);
    PrintVector3("射线方向", ray.direction);
    
    // 射线与平面相交
    float t;
    if (ray.IntersectPlane(groundPlane, t)) {
        Vector3 hitPoint = ray.GetPoint(t);
        oss.str("");
        oss << "射线与平面相交，t = " << t;
        LOG_INFO(oss.str());
        PrintVector3("交点位置", hitPoint);
    } else {
        LOG_INFO("射线与平面不相交");
    }
    
    // 创建 AABB
    AABB box(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    PrintVector3("AABB 最小点", box.min);
    PrintVector3("AABB 最大点", box.max);
    PrintVector3("AABB 中心", box.GetCenter());
    PrintVector3("AABB 大小", box.GetSize());
    
    // 射线与 AABB 相交
    float tMin, tMax;
    Ray ray2(Vector3(0.0f, 0.0f, 5.0f), Vector3(0.0f, 0.0f, -1.0f));
    if (ray2.IntersectAABB(box, tMin, tMax)) {
        LOG_INFO("射线与 AABB 相交");
        oss.str("");
        oss << "进入点 t = " << tMin;
        LOG_INFO(oss.str());
        oss.str("");
        oss << "退出点 t = " << tMax;
        LOG_INFO(oss.str());
        PrintVector3("进入位置", ray2.GetPoint(tMin));
        PrintVector3("退出位置", ray2.GetPoint(tMax));
    } else {
        LOG_INFO("射线与 AABB 不相交");
    }
    
    LOG_INFO("");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 设置日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("========================================");
    LOG_INFO("数学库测试程序");
    LOG_INFO("========================================");
    LOG_INFO("");
    
    try {
        // 运行所有测试
        TestMathUtils();
        TestQuaternion();
        TestMatrixTransforms();
        TestTransform();
        TestHierarchy();
        TestPlaneAndRay();
        
        LOG_INFO("========================================");
        LOG_INFO("所有测试完成！");
        LOG_INFO("========================================");
        
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "测试过程中发生异常: " << e.what();
        LOG_ERROR(oss.str());
        return 1;
    }
    
    return 0;
}
