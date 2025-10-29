/**
 * @file 19_math_benchmark.cpp
 * @brief 数学库性能基准测试
 */

#include <render/transform.h>
#include <render/math_utils.h>
#include <render/logger.h>
#include <chrono>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace Render;
using namespace std::chrono;

// 基准测试辅助类
class Benchmark {
public:
    explicit Benchmark(const std::string& name) 
        : m_name(name), m_start(high_resolution_clock::now()) {}
    
    ~Benchmark() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - m_start).count();
        
        std::ostringstream oss;
        oss << m_name << ": " << std::fixed << std::setprecision(3) 
            << (duration / 1000.0) << " ms";
        LOG_INFO(oss.str());
    }
    
private:
    std::string m_name;
    high_resolution_clock::time_point m_start;
};

// 测试 FromEuler 性能
void BenchmarkFromEuler() {
    LOG_INFO("========================================");
    LOG_INFO("FromEuler 性能测试");
    LOG_INFO("========================================");
    
    const int iterations = 100000;
    std::vector<Quaternion> results(iterations);
    
    {
        Benchmark bench("FromEuler (100,000次)");
        for (int i = 0; i < iterations; ++i) {
            float angle = static_cast<float>(i) * 0.01f;
            results[i] = MathUtils::FromEuler(angle, angle * 0.5f, angle * 0.3f);
        }
    }
    
    // 验证结果（防止编译器优化掉计算）
    float sum = 0.0f;
    for (const auto& q : results) {
        sum += q.w();
    }
    
    std::ostringstream oss;
    oss << "验证和: " << sum;
    LOG_INFO(oss.str());
    LOG_INFO("");
}

// 测试 LookRotation 性能
void BenchmarkLookRotation() {
    LOG_INFO("========================================");
    LOG_INFO("LookRotation 性能测试");
    LOG_INFO("========================================");
    
    const int iterations = 50000;
    std::vector<Quaternion> results(iterations);
    std::vector<Vector3> directions(iterations);
    
    // 生成测试数据
    for (int i = 0; i < iterations; ++i) {
        float angle = static_cast<float>(i) * 0.001f;
        directions[i] = Vector3(std::sin(angle), 0.0f, std::cos(angle));
    }
    
    {
        Benchmark bench("LookRotation (50,000次)");
        for (int i = 0; i < iterations; ++i) {
            results[i] = MathUtils::LookRotation(directions[i]);
        }
    }
    
    // 验证结果
    float sum = 0.0f;
    for (const auto& q : results) {
        sum += q.w();
    }
    
    std::ostringstream oss;
    oss << "验证和: " << sum;
    LOG_INFO(oss.str());
    LOG_INFO("");
}

// 测试 TRS 矩阵构建性能
void BenchmarkTRS() {
    LOG_INFO("========================================");
    LOG_INFO("TRS 矩阵构建性能测试");
    LOG_INFO("========================================");
    
    const int iterations = 100000;
    std::vector<Matrix4> results(iterations);
    
    {
        Benchmark bench("TRS (100,000次)");
        for (int i = 0; i < iterations; ++i) {
            float t = static_cast<float>(i) * 0.01f;
            Vector3 pos(t, t * 0.5f, t * 0.3f);
            Quaternion rot = MathUtils::FromEuler(t, t * 0.5f, 0.0f);
            Vector3 scale(1.0f + t * 0.01f, 1.0f, 1.0f);
            results[i] = MathUtils::TRS(pos, rot, scale);
        }
    }
    
    // 验证结果
    float sum = 0.0f;
    for (const auto& mat : results) {
        sum += mat(0, 0);
    }
    
    std::ostringstream oss;
    oss << "验证和: " << sum;
    LOG_INFO(oss.str());
    LOG_INFO("");
}

// 测试 Transform 世界变换缓存性能
void BenchmarkTransformCache() {
    LOG_INFO("========================================");
    LOG_INFO("Transform 世界变换缓存性能测试");
    LOG_INFO("========================================");
    
    // 创建深层级变换层次
    const int depth = 10;
    std::vector<Transform> transforms(depth);
    
    // 建立父子关系
    for (int i = 1; i < depth; ++i) {
        transforms[i].SetParent(&transforms[i - 1]);
        transforms[i].SetPosition(Vector3(1.0f, 0.0f, 0.0f));
        transforms[i].SetRotationEulerDegrees(Vector3(0.0f, 10.0f, 0.0f));
    }
    
    const int iterations = 10000;
    std::vector<Vector3> positions(iterations);
    std::vector<Quaternion> rotations(iterations);
    
    {
        Benchmark bench("GetWorldPosition/Rotation (深度=10, 10,000次)");
        for (int i = 0; i < iterations; ++i) {
            Transform& t = transforms[depth - 1];
            positions[i] = t.GetWorldPosition();
            rotations[i] = t.GetWorldRotation();
        }
    }
    
    // 验证结果
    std::ostringstream oss;
    oss << "最终位置: (" << positions.back().x() << ", " 
        << positions.back().y() << ", " << positions.back().z() << ")";
    LOG_INFO(oss.str());
    LOG_INFO("");
}

// 测试批量变换性能
void BenchmarkBatchTransform() {
    LOG_INFO("========================================");
    LOG_INFO("批量变换性能测试");
    LOG_INFO("========================================");
    
    Transform transform;
    transform.SetPosition(Vector3(10.0f, 5.0f, 0.0f));
    transform.SetRotationEulerDegrees(Vector3(0.0f, 45.0f, 0.0f));
    transform.SetScale(2.0f);
    
    // 生成测试点
    const int pointCount = 10000;
    std::vector<Vector3> localPoints(pointCount);
    std::vector<Vector3> worldPoints;
    
    for (int i = 0; i < pointCount; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(pointCount) * 6.28f;
        float radius = static_cast<float>(i % 100) * 0.1f;
        localPoints[i] = Vector3(
            std::cos(angle) * radius,
            static_cast<float>(i % 50) * 0.1f,
            std::sin(angle) * radius
        );
    }
    
    // 批量变换测试
    {
        Benchmark bench("批量变换 TransformPoints (10,000点)");
        transform.TransformPoints(localPoints, worldPoints);
    }
    
    // 逐个变换对比
    std::vector<Vector3> worldPointsSingle(pointCount);
    {
        Benchmark bench("逐个变换 TransformPoint (10,000次)");
        for (int i = 0; i < pointCount; ++i) {
            worldPointsSingle[i] = transform.TransformPoint(localPoints[i]);
        }
    }
    
    // 验证结果一致性
    bool consistent = true;
    for (int i = 0; i < pointCount; ++i) {
        float diff = (worldPoints[i] - worldPointsSingle[i]).norm();
        if (diff > 0.001f) {
            consistent = false;
            break;
        }
    }
    
    LOG_INFO(consistent ? "结果一致性: ✓ 通过" : "结果一致性: ✗ 失败");
    LOG_INFO("");
}

// 测试向量归一化优化
void BenchmarkSafeNormalize() {
    LOG_INFO("========================================");
    LOG_INFO("SafeNormalize 性能测试");
    LOG_INFO("========================================");
    
    const int iterations = 100000;
    
    // 生成已归一化的向量
    std::vector<Vector3> normalizedVectors(iterations);
    for (int i = 0; i < iterations; ++i) {
        float angle = static_cast<float>(i) * 0.01f;
        normalizedVectors[i] = Vector3(std::cos(angle), std::sin(angle), 0.0f);
    }
    
    std::vector<Vector3> results1(iterations);
    std::vector<Vector3> results2(iterations);
    
    // 使用 SafeNormalize（已归一化的向量会快速返回）
    {
        Benchmark bench("SafeNormalize (已归一化, 100,000次)");
        for (int i = 0; i < iterations; ++i) {
            results1[i] = MathUtils::SafeNormalize(normalizedVectors[i]);
        }
    }
    
    // 使用普通 normalized()
    {
        Benchmark bench("normalized() (已归一化, 100,000次)");
        for (int i = 0; i < iterations; ++i) {
            results2[i] = normalizedVectors[i].normalized();
        }
    }
    
    LOG_INFO("注意: SafeNormalize 对已归一化向量有显著优化");
    LOG_INFO("");
}

// 综合性能测试
void BenchmarkComprehensive() {
    LOG_INFO("========================================");
    LOG_INFO("综合场景性能测试");
    LOG_INFO("========================================");
    
    const int iterations = 1000;
    
    {
        Benchmark bench("综合场景 (1,000次迭代)");
        
        std::vector<Transform> transforms(5);
        for (int i = 1; i < 5; ++i) {
            transforms[i].SetParent(&transforms[i - 1]);
        }
        
        for (int iter = 0; iter < iterations; ++iter) {
            float t = static_cast<float>(iter) * 0.01f;
            
            // 更新变换
            for (int i = 0; i < 5; ++i) {
                transforms[i].SetPosition(Vector3(
                    std::sin(t + i), 
                    std::cos(t + i), 
                    0.0f
                ));
                transforms[i].SetRotationEulerDegrees(Vector3(
                    t * 10.0f, 
                    t * 20.0f, 
                    0.0f
                ));
            }
            
            // 查询世界变换
            for (int i = 0; i < 5; ++i) {
                Vector3 pos = transforms[i].GetWorldPosition();
                Quaternion rot = transforms[i].GetWorldRotation();
                Matrix4 mat = transforms[i].GetWorldMatrix();
                (void)pos; (void)rot; (void)mat; // 防止优化
            }
            
            // LookAt 操作
            transforms[0].LookAt(Vector3(10.0f, 0.0f, 0.0f));
        }
    }
    
    LOG_INFO("");
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Info);
    
    LOG_INFO("========================================");
    LOG_INFO("数学库性能基准测试");
    LOG_INFO("========================================");
    LOG_INFO("编译配置:");
    LOG_INFO("  C++ 标准: C++17");
#ifdef _OPENMP
    LOG_INFO("  OpenMP: 启用");
#else
    LOG_INFO("  OpenMP: 禁用");
#endif
#ifdef __AVX2__
    LOG_INFO("  AVX2: 启用");
#else
    LOG_INFO("  AVX2: 禁用");
#endif
    LOG_INFO("");
    
    try {
        // 运行所有基准测试
        BenchmarkFromEuler();
        BenchmarkLookRotation();
        BenchmarkTRS();
        BenchmarkTransformCache();
        BenchmarkBatchTransform();
        BenchmarkSafeNormalize();
        BenchmarkComprehensive();
        
        LOG_INFO("========================================");
        LOG_INFO("所有基准测试完成！");
        LOG_INFO("========================================");
        
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "基准测试过程中发生异常: " << e.what();
        LOG_ERROR(oss.str());
        return 1;
    }
    
    return 0;
}

