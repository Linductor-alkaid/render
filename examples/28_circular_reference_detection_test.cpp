/**
 * @file 28_circular_reference_detection_test.cpp
 * @brief 循环引用检测测试
 * 
 * 演示如何使用 ResourceDependencyTracker 检测资源之间的循环引用
 */

#include <render/resource_manager.h>
#include <render/material.h>
#include <render/texture.h>
#include <render/shader.h>
#include <render/mesh.h>
#include <render/logger.h>
#include <iostream>

using namespace Render;

/**
 * @brief 测试1: 正常的单向依赖（无循环）
 */
void TestNormalDependency() {
    LOG_INFO("========================================");
    LOG_INFO("测试1: 正常的单向依赖");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 创建资源
    auto shader = std::make_shared<Shader>();
    auto texture1 = std::make_shared<Texture>();
    auto texture2 = std::make_shared<Texture>();
    auto material = std::make_shared<Material>();
    
    // 注册资源
    manager.RegisterShader("basic_shader", shader);
    manager.RegisterTexture("diffuse_tex", texture1);
    manager.RegisterTexture("normal_tex", texture2);
    manager.RegisterMaterial("wood_material", material);
    
    // 设置依赖关系
    // material 依赖 shader + 2个texture
    manager.UpdateResourceDependencies("wood_material", {
        "basic_shader",
        "diffuse_tex",
        "normal_tex"
    });
    
    // 执行循环检测
    auto cycles = manager.DetectCircularReferences();
    
    if (cycles.empty()) {
        LOG_INFO("✅ 测试通过：未检测到循环引用");
    } else {
        LOG_ERROR("❌ 测试失败：不应该有循环引用");
    }
    
    // 打印依赖统计
    manager.PrintDependencyStatistics();
}

/**
 * @brief 测试2: 简单的循环引用（A → B → A）
 */
void TestSimpleCircularReference() {
    LOG_INFO("========================================");
    LOG_INFO("测试2: 简单的循环引用");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 创建资源（模拟场景：材质A引用材质B，材质B又引用材质A）
    auto materialA = std::make_shared<Material>();
    auto materialB = std::make_shared<Material>();
    
    manager.RegisterMaterial("materialA", materialA);
    manager.RegisterMaterial("materialB", materialB);
    
    // 设置循环依赖
    manager.UpdateResourceDependencies("materialA", {"materialB"});
    manager.UpdateResourceDependencies("materialB", {"materialA"});
    
    // 执行循环检测
    auto cycles = manager.DetectCircularReferences();
    
    if (!cycles.empty()) {
        LOG_INFO("✅ 测试通过：成功检测到循环引用");
        for (const auto& cycle : cycles) {
            LOG_WARNING("  检测到循环: " + cycle.ToString());
        }
    } else {
        LOG_ERROR("❌ 测试失败：应该检测到循环引用");
    }
}

/**
 * @brief 测试3: 复杂的循环引用（A → B → C → A）
 */
void TestComplexCircularReference() {
    LOG_INFO("========================================");
    LOG_INFO("测试3: 复杂的循环引用");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 创建资源链：Material → Texture → Shader → Material
    auto material = std::make_shared<Material>();
    auto texture = std::make_shared<Texture>();
    auto shader = std::make_shared<Shader>();
    
    manager.RegisterMaterial("mat1", material);
    manager.RegisterTexture("tex1", texture);
    manager.RegisterShader("shader1", shader);
    
    // 设置三角循环依赖
    manager.UpdateResourceDependencies("mat1", {"tex1"});
    manager.UpdateResourceDependencies("tex1", {"shader1"});
    manager.UpdateResourceDependencies("shader1", {"mat1"});  // 循环回来
    
    // 执行循环检测
    auto cycles = manager.DetectCircularReferences();
    
    if (!cycles.empty()) {
        LOG_INFO("✅ 测试通过：成功检测到复杂循环引用");
        for (const auto& cycle : cycles) {
            LOG_WARNING("  检测到循环: " + cycle.ToString());
        }
    } else {
        LOG_ERROR("❌ 测试失败：应该检测到循环引用");
    }
}

/**
 * @brief 测试4: 深层依赖树（无循环）
 */
void TestDeepDependencyTree() {
    LOG_INFO("========================================");
    LOG_INFO("测试4: 深层依赖树分析");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 创建深层依赖树
    // Level 0: material
    // Level 1: shader, tex1, tex2
    // Level 2: base_shader, base_tex
    
    manager.RegisterMaterial("material", std::make_shared<Material>());
    manager.RegisterShader("shader", std::make_shared<Shader>());
    manager.RegisterTexture("tex1", std::make_shared<Texture>());
    manager.RegisterTexture("tex2", std::make_shared<Texture>());
    manager.RegisterShader("base_shader", std::make_shared<Shader>());
    manager.RegisterTexture("base_tex", std::make_shared<Texture>());
    
    // 设置依赖关系
    manager.UpdateResourceDependencies("material", {"shader", "tex1", "tex2"});
    manager.UpdateResourceDependencies("shader", {"base_shader"});
    manager.UpdateResourceDependencies("tex1", {"base_tex"});
    manager.UpdateResourceDependencies("tex2", {"base_tex"});
    
    // 执行依赖分析
    auto result = manager.AnalyzeDependencies();
    
    LOG_INFO("依赖深度分析:");
    LOG_INFO("  最大深度: " + std::to_string(result.maxDepth));
    LOG_INFO("  孤立资源: " + std::to_string(result.isolatedResources));
    
    if (result.maxDepth >= 2) {
        LOG_INFO("✅ 测试通过：正确计算了深层依赖");
    }
}

/**
 * @brief 测试5: 导出依赖关系图
 */
void TestExportDependencyGraph() {
    LOG_INFO("========================================");
    LOG_INFO("测试5: 导出依赖关系图");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 创建一个复杂的依赖关系网络
    manager.RegisterMaterial("wood_material", std::make_shared<Material>());
    manager.RegisterMaterial("metal_material", std::make_shared<Material>());
    manager.RegisterTexture("wood_diffuse", std::make_shared<Texture>());
    manager.RegisterTexture("wood_normal", std::make_shared<Texture>());
    manager.RegisterTexture("metal_diffuse", std::make_shared<Texture>());
    manager.RegisterShader("pbr_shader", std::make_shared<Shader>());
    manager.RegisterMesh("cube_mesh", std::make_shared<Mesh>());
    
    // 设置依赖
    manager.UpdateResourceDependencies("wood_material", {
        "pbr_shader", "wood_diffuse", "wood_normal"
    });
    manager.UpdateResourceDependencies("metal_material", {
        "pbr_shader", "metal_diffuse"
    });
    
    // 导出图
    bool success = manager.ExportDependencyGraph("dependency_graph.dot");
    
    if (success) {
        LOG_INFO("✅ 依赖关系图已导出到 dependency_graph.dot");
        LOG_INFO("   使用以下命令查看:");
        LOG_INFO("   dot -Tpng dependency_graph.dot -o dependency_graph.png");
    } else {
        LOG_ERROR("❌ 导出失败");
    }
}

/**
 * @brief 测试6: 实际使用场景 - Material依赖追踪
 */
void TestRealWorldScenario() {
    LOG_INFO("========================================");
    LOG_INFO("测试6: 实际使用场景");
    LOG_INFO("========================================");
    
    auto& manager = ResourceManager::GetInstance();
    manager.Clear();
    
    // 场景：创建一个完整的PBR材质系统
    
    // 1. 创建着色器
    auto pbrShader = std::make_shared<Shader>();
    manager.RegisterShader("pbr_shader", pbrShader);
    
    // 2. 创建纹理
    auto albedo = std::make_shared<Texture>();
    auto normal = std::make_shared<Texture>();
    auto metallic = std::make_shared<Texture>();
    auto roughness = std::make_shared<Texture>();
    auto ao = std::make_shared<Texture>();
    
    manager.RegisterTexture("metal_albedo", albedo);
    manager.RegisterTexture("metal_normal", normal);
    manager.RegisterTexture("metal_metallic", metallic);
    manager.RegisterTexture("metal_roughness", roughness);
    manager.RegisterTexture("metal_ao", ao);
    
    // 3. 创建材质并设置依赖
    auto material = std::make_shared<Material>();
    material->SetShader(pbrShader);
    // 假设material内部设置了这些纹理
    
    manager.RegisterMaterial("metal_pbr_material", material);
    
    // 4. 手动报告依赖关系
    manager.UpdateResourceDependencies("metal_pbr_material", {
        "pbr_shader",
        "metal_albedo",
        "metal_normal",
        "metal_metallic",
        "metal_roughness",
        "metal_ao"
    });
    
    // 5. 分析依赖
    auto result = manager.AnalyzeDependencies();
    
    LOG_INFO("实际场景分析:");
    LOG_INFO("  总资源: " + std::to_string(result.totalResources));
    LOG_INFO("  循环引用: " + std::to_string(result.circularReferences.size()));
    LOG_INFO("  最大深度: " + std::to_string(result.maxDepth));
    
    // 6. 打印依赖树
    auto& tracker = manager.GetDependencyTracker();
    std::string tree = tracker.PrintDependencyTree("metal_pbr_material", 5);
    LOG_INFO("\n依赖树:\n" + tree);
}

int main() {
    // 设置日志
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogLevel(LogLevel::Debug);
    
    LOG_INFO("========================================");
    LOG_INFO("循环引用检测系统测试");
    LOG_INFO("========================================\n");
    
    try {
        // 运行所有测试
        TestNormalDependency();
        std::cout << "\n";
        
        TestSimpleCircularReference();
        std::cout << "\n";
        
        TestComplexCircularReference();
        std::cout << "\n";
        
        TestDeepDependencyTree();
        std::cout << "\n";
        
        TestExportDependencyGraph();
        std::cout << "\n";
        
        TestRealWorldScenario();
        std::cout << "\n";
        
        LOG_INFO("========================================");
        LOG_INFO("所有测试完成！");
        LOG_INFO("========================================");
        
    } catch (const std::exception& e) {
        LOG_ERROR("测试失败: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}

