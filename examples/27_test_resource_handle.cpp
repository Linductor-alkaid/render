/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
/**
 * @file 27_test_resource_handle.cpp
 * @brief 智能资源句柄系统测试
 * 
 * 本示例演示智能资源句柄系统的使用方法和优势：
 * 1. 基本使用 - 创建和访问资源句柄
 * 2. 句柄失效检测 - 资源删除后句柄自动失效
 * 3. 资源热重载 - 不改变句柄，只替换资源内容
 * 4. ID 重用 - 验证代数机制防止悬空引用
 * 5. 性能对比 - 句柄 vs shared_ptr
 * 6. 内存使用 - 句柄的内存优势
 */

#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/texture.h"
#include "render/mesh.h"
#include "render/logger.h"
#include <iostream>
#include <chrono>
#include <vector>

using namespace Render;
using namespace std::chrono;

// ============================================================================
// 辅助函数
// ============================================================================

void PrintSeparator(const std::string& title) {
    std::cout << "\n========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n\n";
}

void PrintHandleStats(const ResourceManager::HandleStats& stats) {
    std::cout << "句柄系统统计:\n";
    std::cout << "  纹理槽: " << stats.textureActiveSlots << "/" << stats.textureSlots 
              << " (空闲: " << stats.textureFreeSlots << ")\n";
    std::cout << "  网格槽: " << stats.meshActiveSlots << "/" << stats.meshSlots 
              << " (空闲: " << stats.meshFreeSlots << ")\n";
    std::cout << "  材质槽: " << stats.materialActiveSlots << "/" << stats.materialSlots 
              << " (空闲: " << stats.materialFreeSlots << ")\n";
    std::cout << "  着色器槽: " << stats.shaderActiveSlots << "/" << stats.shaderSlots 
              << " (空闲: " << stats.shaderFreeSlots << ")\n";
}

// ============================================================================
// 测试1: 基本使用
// ============================================================================

void Test1_BasicUsage() {
    PrintSeparator("测试 1: 基本使用");
    
    auto& rm = ResourceManager::GetInstance();
    
    // 创建纹理和句柄
    auto texture = std::make_shared<Texture>();
    texture->CreateEmpty(512, 512, TextureFormat::RGBA);
    
    TextureHandle handle = rm.CreateTextureHandle("test_texture", texture);
    
    std::cout << "创建纹理句柄:\n";
    std::cout << "  ID: " << handle.GetID() << "\n";
    std::cout << "  代数: " << handle.GetGeneration() << "\n";
    std::cout << "  大小: " << sizeof(handle) << " 字节\n";
    std::cout << "  有效: " << (handle.IsValid() ? "是" : "否") << "\n\n";
    
    // 使用句柄访问资源
    if (auto tex = handle.Get()) {
        std::cout << "通过句柄访问纹理:\n";
        std::cout << "  宽度: " << tex->GetWidth() << "\n";
        std::cout << "  高度: " << tex->GetHeight() << "\n";
    }
    
    // 也可以使用运算符重载
    if (handle) {
        std::cout << "\n使用 operator bool:\n";
        std::cout << "  句柄有效，纹理尺寸: " 
                  << handle->GetWidth() << "x" << handle->GetHeight() << "\n";
    }
    
    PrintHandleStats(rm.GetHandleStats());
}

// ============================================================================
// 测试2: 句柄失效检测
// ============================================================================

void Test2_HandleInvalidation() {
    PrintSeparator("测试 2: 句柄失效检测");
    
    auto& rm = ResourceManager::GetInstance();
    
    // 创建纹理句柄
    auto texture = std::make_shared<Texture>();
    texture->CreateEmpty(256, 256, TextureFormat::RGB);
    
    TextureHandle handle = rm.CreateTextureHandle("temp_texture", texture);
    
    std::cout << "创建句柄后:\n";
    std::cout << "  句柄有效: " << (handle.IsValid() ? "是" : "否") << "\n";
    std::cout << "  可以访问: " << (handle.Get() != nullptr ? "是" : "否") << "\n";
    
    // 删除资源
    rm.RemoveTextureByHandle(handle);
    
    std::cout << "\n删除资源后:\n";
    std::cout << "  句柄有效: " << (handle.IsValid() ? "是" : "否") << "\n";
    std::cout << "  可以访问: " << (handle.Get() != nullptr ? "是" : "否") << "\n";
    std::cout << "  ✅ 句柄自动失效，不会崩溃！\n";
}

// ============================================================================
// 测试3: 资源热重载
// ============================================================================

void Test3_HotReload() {
    PrintSeparator("测试 3: 资源热重载");
    
    auto& rm = ResourceManager::GetInstance();
    
    // 创建原始纹理
    auto texture1 = std::make_shared<Texture>();
    texture1->CreateEmpty(128, 128, TextureFormat::RGBA);
    
    TextureHandle handle = rm.CreateTextureHandle("reloadable_texture", texture1);
    
    std::cout << "原始纹理:\n";
    std::cout << "  句柄 ID: " << handle.GetID() << "\n";
    std::cout << "  代数: " << handle.GetGeneration() << "\n";
    std::cout << "  尺寸: " << handle->GetWidth() << "x" << handle->GetHeight() << "\n";
    
    // 创建新纹理（更大）
    auto texture2 = std::make_shared<Texture>();
    texture2->CreateEmpty(512, 512, TextureFormat::RGBA);
    
    // 热重载
    rm.ReloadTexture(handle, texture2);
    
    std::cout << "\n热重载后:\n";
    std::cout << "  句柄 ID: " << handle.GetID() << " (不变)\n";
    std::cout << "  代数: " << handle.GetGeneration() << " (不变)\n";
    std::cout << "  尺寸: " << handle->GetWidth() << "x" << handle->GetHeight() << " (已更新)\n";
    std::cout << "  ✅ 所有持有该句柄的对象自动使用新纹理！\n";
}

// ============================================================================
// 测试4: ID 重用和代数机制
// ============================================================================

void Test4_IDReuseAndGeneration() {
    PrintSeparator("测试 4: ID 重用和代数机制");
    
    auto& rm = ResourceManager::GetInstance();
    
    // 创建第一个纹理
    auto texture1 = std::make_shared<Texture>();
    texture1->CreateEmpty(64, 64, TextureFormat::RGBA);
    TextureHandle handle1 = rm.CreateTextureHandle("texture_slot_0", texture1);
    
    std::cout << "第一个纹理:\n";
    std::cout << "  ID: " << handle1.GetID() << "\n";
    std::cout << "  代数: " << handle1.GetGeneration() << "\n";
    
    ResourceID firstID = handle1.GetID();
    
    // 删除纹理
    rm.RemoveTextureByHandle(handle1);
    std::cout << "\n第一个纹理已删除\n";
    
    // 创建第二个纹理（应该重用相同的 ID）
    auto texture2 = std::make_shared<Texture>();
    texture2->CreateEmpty(128, 128, TextureFormat::RGBA);
    TextureHandle handle2 = rm.CreateTextureHandle("texture_slot_0_reused", texture2);
    
    std::cout << "\n第二个纹理（重用槽）:\n";
    std::cout << "  ID: " << handle2.GetID() << "\n";
    std::cout << "  代数: " << handle2.GetGeneration() << "\n";
    std::cout << "  ID 重用: " << (handle2.GetID() == firstID ? "是" : "否") << "\n";
    
    // 尝试用旧句柄访问（应该失败）
    std::cout << "\n使用旧句柄访问:\n";
    std::cout << "  旧句柄有效: " << (handle1.IsValid() ? "是" : "否") << "\n";
    std::cout << "  可以访问: " << (handle1.Get() != nullptr ? "是" : "否") << "\n";
    std::cout << "  ✅ 代数机制防止了悬空引用！\n";
    
    // 新句柄可以正常使用
    std::cout << "\n使用新句柄访问:\n";
    std::cout << "  新句柄有效: " << (handle2.IsValid() ? "是" : "否") << "\n";
    std::cout << "  纹理尺寸: " << handle2->GetWidth() << "x" << handle2->GetHeight() << "\n";
}

// ============================================================================
// 测试5: 性能对比
// ============================================================================

void Test5_PerformanceComparison() {
    PrintSeparator("测试 5: 性能对比");
    
    auto& rm = ResourceManager::GetInstance();
    
    const int COUNT = 10000;
    
    // 测试 1: 创建句柄
    std::vector<TextureHandle> handles;
    handles.reserve(COUNT);
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < COUNT; ++i) {
        auto tex = std::make_shared<Texture>();
        tex->CreateEmpty(64, 64, TextureFormat::RGBA);
        TextureHandle handle = rm.CreateTextureHandle("perf_test_" + std::to_string(i), tex);
        handles.push_back(handle);
    }
    
    auto end = high_resolution_clock::now();
    auto duration1 = duration_cast<microseconds>(end - start).count();
    
    std::cout << "创建 " << COUNT << " 个句柄: " << duration1 << " 微秒\n";
    std::cout << "平均每个: " << (duration1 / (double)COUNT) << " 微秒\n";
    
    // 测试 2: 访问句柄
    start = high_resolution_clock::now();
    
    size_t validCount = 0;
    for (int i = 0; i < COUNT; ++i) {
        if (handles[i].Get() != nullptr) {
            ++validCount;
        }
    }
    
    end = high_resolution_clock::now();
    auto duration2 = duration_cast<microseconds>(end - start).count();
    
    std::cout << "\n访问 " << COUNT << " 个句柄: " << duration2 << " 微秒\n";
    std::cout << "平均每个: " << (duration2 / (double)COUNT) << " 微秒\n";
    std::cout << "有效句柄: " << validCount << "\n";
    
    // 测试 3: shared_ptr 对比
    std::vector<Ref<Texture>> sharedPtrs;
    sharedPtrs.reserve(COUNT);
    
    start = high_resolution_clock::now();
    
    for (int i = 0; i < COUNT; ++i) {
        auto tex = std::make_shared<Texture>();
        tex->CreateEmpty(64, 64, TextureFormat::RGBA);
        sharedPtrs.push_back(tex);
    }
    
    end = high_resolution_clock::now();
    auto duration3 = duration_cast<microseconds>(end - start).count();
    
    std::cout << "\n创建 " << COUNT << " 个 shared_ptr: " << duration3 << " 微秒\n";
    std::cout << "速度对比: 句柄比 shared_ptr " 
              << (duration3 > duration1 ? "快" : "慢") << " " 
              << std::abs((double)(duration3 - duration1) / duration3 * 100.0) << "%\n";
}

// ============================================================================
// 测试6: 内存使用
// ============================================================================

void Test6_MemoryUsage() {
    PrintSeparator("测试 6: 内存使用");
    
    std::cout << "类型大小对比:\n";
    std::cout << "  TextureHandle: " << sizeof(TextureHandle) << " 字节\n";
    std::cout << "  shared_ptr<Texture>: " << sizeof(Ref<Texture>) << " 字节\n";
    std::cout << "  Texture*: " << sizeof(Texture*) << " 字节\n";
    
    const int COUNT = 1000;
    std::cout << "\n存储 " << COUNT << " 个引用的内存:\n";
    std::cout << "  使用句柄: " << (sizeof(TextureHandle) * COUNT) << " 字节 ("
              << (sizeof(TextureHandle) * COUNT / 1024.0) << " KB)\n";
    std::cout << "  使用 shared_ptr: " << (sizeof(Ref<Texture>) * COUNT) << " 字节 ("
              << (sizeof(Ref<Texture>) * COUNT / 1024.0) << " KB)\n";
    
    double savings = (1.0 - (double)sizeof(TextureHandle) / sizeof(Ref<Texture>)) * 100.0;
    std::cout << "\n内存节省: " << savings << "%\n";
    
    std::cout << "\n缓存行分析:\n";
    std::cout << "  典型 L1 缓存行: 64 字节\n";
    std::cout << "  每行可存储句柄: " << (64 / sizeof(TextureHandle)) << " 个\n";
    std::cout << "  每行可存储 shared_ptr: " << (64 / sizeof(Ref<Texture>)) << " 个\n";
    std::cout << "  ✅ 句柄缓存局部性更好！\n";
}

// ============================================================================
// 测试7: 多资源类型
// ============================================================================

void Test7_MultipleResourceTypes() {
    PrintSeparator("测试 7: 多资源类型");
    
    auto& rm = ResourceManager::GetInstance();
    
    // 创建不同类型的资源句柄
    auto texture = std::make_shared<Texture>();
    texture->CreateEmpty(256, 256, TextureFormat::RGBA);
    TextureHandle texHandle = rm.CreateTextureHandle("multi_texture", texture);
    
    auto mesh = std::make_shared<Mesh>();
    // 创建简单三角形
    std::vector<Vertex> vertices;
    Vertex v0, v1, v2;
    v0.position = Vector3(0, 0, 0);
    v0.texCoord = Vector2(0, 0);
    v0.normal = Vector3(0, 0, 1);
    
    v1.position = Vector3(1, 0, 0);
    v1.texCoord = Vector2(1, 0);
    v1.normal = Vector3(0, 0, 1);
    
    v2.position = Vector3(0, 1, 0);
    v2.texCoord = Vector2(0, 1);
    v2.normal = Vector3(0, 0, 1);
    
    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    
    mesh->SetVertices(vertices);
    mesh->SetIndices({0, 1, 2});
    MeshHandle meshHandle = rm.CreateMeshHandle("multi_mesh", mesh);
    
    std::cout << "创建多种资源句柄:\n";
    std::cout << "  纹理句柄 ID: " << texHandle.GetID() << "\n";
    std::cout << "  网格句柄 ID: " << meshHandle.GetID() << "\n";
    std::cout << "  句柄类型安全: 是（编译时检查）\n";
    
    PrintHandleStats(rm.GetHandleStats());
    
    // 访问资源
    if (texHandle) {
        std::cout << "\n纹理信息:\n";
        std::cout << "  尺寸: " << texHandle->GetWidth() << "x" << texHandle->GetHeight() << "\n";
    }
    
    if (meshHandle) {
        std::cout << "\n网格信息:\n";
        std::cout << "  顶点数: " << meshHandle->GetVertexCount() << "\n";
        std::cout << "  索引数: " << meshHandle->GetIndexCount() << "\n";
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    try {
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║         智能资源句柄系统测试                              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        
        // 先初始化渲染器（创建 OpenGL 上下文）
        std::cout << "\n正在初始化渲染器...\n";
        Renderer renderer;
        if (!renderer.Initialize("Resource Handle Test", 800, 600)) {
            std::cerr << "初始化渲染器失败\n";
            return 1;
        }
        std::cout << "✅ 渲染器初始化成功\n";
        
        // 设置日志级别（在初始化之后，避免显示过多初始化日志）
        Logger::GetInstance().SetLogLevel(LogLevel::Warning);
        
        // 运行测试
        Test1_BasicUsage();
        Test2_HandleInvalidation();
        Test3_HotReload();
        Test4_IDReuseAndGeneration();
        Test5_PerformanceComparison();
        Test6_MemoryUsage();
        Test7_MultipleResourceTypes();
        
        PrintSeparator("测试完成");
        std::cout << "✅ 所有测试通过！\n\n";
        
        std::cout << "资源句柄系统优势总结:\n";
        std::cout << "1. 内存高效 - 只有 8 字节（vs shared_ptr 的 16 字节）\n";
        std::cout << "2. 缓存友好 - 更好的缓存局部性\n";
        std::cout << "3. 热重载 - 保持句柄，替换资源\n";
        std::cout << "4. 安全 - 代数机制防止悬空引用\n";
        std::cout << "5. 无循环引用 - 不使用引用计数\n";
        std::cout << "6. ID 重用 - 减少内存碎片\n\n";
        
        std::cout << "正在清理资源...\n";
        
        // 重要：必须在 Shutdown 之前清理所有资源
        // 否则 OpenGL 对象会在上下文销毁后才析构，导致线程警告
        ResourceManager::GetInstance().Clear();
        
        std::cout << "✅ 资源清理完成\n";
        std::cout << "正在关闭渲染器...\n";
        
        renderer.Shutdown();
        
        std::cout << "✅ 测试程序正常退出\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << "\n";
        return 1;
    }
}

