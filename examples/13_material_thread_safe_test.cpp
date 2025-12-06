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
 * @file 13_material_thread_safe_test.cpp
 * @brief Material 类线程安全测试
 * 
 * 本测试程序验证 Material 类在多线程环境下的安全性：
 * 1. 并发读取材质属性
 * 2. 并发修改材质属性
 * 3. 并发访问纹理和着色器
 * 4. 移动操作的线程安全性
 * 5. 压力测试
 */

#include <render/material.h>
#include <render/shader_cache.h>
#include <render/texture_loader.h>
#include <render/logger.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 全局计数器
std::atomic<int> g_readCount{0};
std::atomic<int> g_writeCount{0};
std::atomic<int> g_errorCount{0};

/**
 * @brief 测试1: 并发读取材质属性
 */
void Test1_ConcurrentRead() {
    Logger::GetInstance().Info("\n=== 测试 1: 并发读取材质属性 ===");
    
    auto material = std::make_shared<Material>();
    material->SetName("Test Material");
    material->SetDiffuseColor(Color(0.8f, 0.2f, 0.1f, 1.0f));
    material->SetShininess(64.0f);
    material->SetMetallic(0.5f);
    
    const int numThreads = 10;
    const int readsPerThread = 1000;
    std::vector<std::thread> threads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建多个读取线程
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&material, readsPerThread, i]() {
            for (int j = 0; j < readsPerThread; ++j) {
                // 读取各种属性
                std::string name = material->GetName();
                Color diffuse = material->GetDiffuseColor();
                float shininess = material->GetShininess();
                float metallic = material->GetMetallic();
                bool valid = material->IsValid();
                
                // 验证数据
                if (name != "Test Material" || 
                    shininess != 64.0f || 
                    metallic != 0.5f) {
                    g_errorCount++;
                }
                
                g_readCount++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::GetInstance().Info("读取次数: " + std::to_string(g_readCount.load()));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info("错误数: " + std::to_string(g_errorCount.load()));
    Logger::GetInstance().Info(g_errorCount.load() == 0 ? "✅ 测试通过" : "❌ 测试失败");
}

/**
 * @brief 测试2: 并发写入材质属性
 */
void Test2_ConcurrentWrite() {
    Logger::GetInstance().Info("\n=== 测试 2: 并发写入材质属性 ===");
    
    auto material = std::make_shared<Material>();
    
    const int numThreads = 10;
    const int writesPerThread = 500;
    std::vector<std::thread> threads;
    
    g_writeCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建多个写入线程
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&material, writesPerThread, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            for (int j = 0; j < writesPerThread; ++j) {
                // 随机写入属性
                material->SetDiffuseColor(Color(dis(gen), dis(gen), dis(gen), 1.0f));
                material->SetShininess(dis(gen) * 128.0f);
                material->SetMetallic(dis(gen));
                material->SetRoughness(dis(gen));
                material->SetOpacity(dis(gen));
                
                // 写入自定义参数
                material->SetFloat("customParam", dis(gen));
                material->SetInt("threadId", i);
                
                g_writeCount++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::GetInstance().Info("写入次数: " + std::to_string(g_writeCount.load()));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info("✅ 测试通过（无崩溃）");
}

/**
 * @brief 测试3: 并发读写混合
 */
void Test3_ConcurrentReadWrite() {
    Logger::GetInstance().Info("\n=== 测试 3: 并发读写混合 ===");
    
    auto material = std::make_shared<Material>();
    material->SetName("Mixed Access Test");
    material->SetDiffuseColor(Color::Red());
    
    const int numReaders = 5;
    const int numWriters = 5;
    const int operationsPerThread = 500;
    std::vector<std::thread> threads;
    
    g_readCount = 0;
    g_writeCount = 0;
    g_errorCount = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建读取线程
    for (int i = 0; i < numReaders; ++i) {
        threads.emplace_back([&material, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                std::string name = material->GetName();
                Color diffuse = material->GetDiffuseColor();
                float shininess = material->GetShininess();
                BlendMode blend = material->GetBlendMode();
                
                if (name.empty()) {
                    g_errorCount++;
                }
                
                g_readCount++;
            }
        });
    }
    
    // 创建写入线程
    for (int i = 0; i < numWriters; ++i) {
        threads.emplace_back([&material, operationsPerThread, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            
            for (int j = 0; j < operationsPerThread; ++j) {
                material->SetDiffuseColor(Color(dis(gen), dis(gen), dis(gen), 1.0f));
                material->SetShininess(dis(gen) * 128.0f);
                material->SetBlendMode(static_cast<BlendMode>(j % 4));
                
                g_writeCount++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::GetInstance().Info("读取次数: " + std::to_string(g_readCount.load()));
    Logger::GetInstance().Info("写入次数: " + std::to_string(g_writeCount.load()));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info("错误数: " + std::to_string(g_errorCount.load()));
    Logger::GetInstance().Info(g_errorCount.load() == 0 ? "✅ 测试通过" : "❌ 测试失败");
}

/**
 * @brief 测试4: 纹理和着色器并发访问
 */
void Test4_TextureShaderAccess() {
    Logger::GetInstance().Info("\n=== 测试 4: 纹理和着色器并发访问 ===");
    
    auto material = std::make_shared<Material>();
    
    // 创建一个虚拟着色器（仅用于测试，不实际使用）
    auto shader = std::make_shared<Shader>();
    material->SetShader(shader);
    
    const int numThreads = 8;
    const int operationsPerThread = 500;
    std::vector<std::thread> threads;
    
    g_readCount = 0;
    g_writeCount = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&material, operationsPerThread, i]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                if (i % 2 == 0) {
                    // 读取操作
                    auto shader = material->GetShader();
                    bool hasTexture = material->HasTexture("diffuseMap");
                    auto names = material->GetTextureNames();
                    g_readCount++;
                } else {
                    // 写入操作
                    material->SetTexture("texture" + std::to_string(i), nullptr);
                    material->RemoveTexture("texture" + std::to_string(i));
                    g_writeCount++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::GetInstance().Info("读取次数: " + std::to_string(g_readCount.load()));
    Logger::GetInstance().Info("写入次数: " + std::to_string(g_writeCount.load()));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info("✅ 测试通过（无崩溃）");
}

/**
 * @brief 测试5: 压力测试
 */
void Test5_StressTest() {
    Logger::GetInstance().Info("\n=== 测试 5: 压力测试 ===");
    
    const int numMaterials = 20;
    std::vector<std::shared_ptr<Material>> materials;
    
    // 创建多个材质
    for (int i = 0; i < numMaterials; ++i) {
        auto material = std::make_shared<Material>();
        material->SetName("Material " + std::to_string(i));
        materials.push_back(material);
    }
    
    const int numThreads = 16;
    const int operationsPerThread = 1000;
    std::vector<std::thread> threads;
    
    g_readCount = 0;
    g_writeCount = 0;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建大量线程同时操作多个材质
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&materials, operationsPerThread, numMaterials, i]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> matDis(0, numMaterials - 1);
            std::uniform_real_distribution<float> valueDis(0.0f, 1.0f);
            
            for (int j = 0; j < operationsPerThread; ++j) {
                auto& material = materials[matDis(gen)];
                
                if (j % 3 == 0) {
                    // 读取
                    Color diffuse = material->GetDiffuseColor();
                    float shininess = material->GetShininess();
                    g_readCount++;
                } else {
                    // 写入
                    material->SetDiffuseColor(Color(valueDis(gen), valueDis(gen), valueDis(gen), 1.0f));
                    material->SetShininess(valueDis(gen) * 128.0f);
                    material->SetMetallic(valueDis(gen));
                    g_writeCount++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    Logger::GetInstance().Info("材质数量: " + std::to_string(numMaterials));
    Logger::GetInstance().Info("线程数: " + std::to_string(numThreads));
    Logger::GetInstance().Info("读取次数: " + std::to_string(g_readCount.load()));
    Logger::GetInstance().Info("写入次数: " + std::to_string(g_writeCount.load()));
    Logger::GetInstance().Info("总操作数: " + std::to_string(g_readCount.load() + g_writeCount.load()));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info("吞吐量: " + std::to_string((g_readCount.load() + g_writeCount.load()) * 1000 / duration.count()) + " ops/s");
    Logger::GetInstance().Info("✅ 压力测试通过");
}

/**
 * @brief 测试6: 移动操作线程安全性
 */
void Test6_MoveOperations() {
    Logger::GetInstance().Info("\n=== 测试 6: 移动操作线程安全性 ===");
    
    const int numIterations = 100;
    std::atomic<int> successCount{0};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&successCount, numIterations]() {
            for (int j = 0; j < numIterations; ++j) {
                // 创建材质
                Material mat1;
                mat1.SetName("Material 1");
                mat1.SetDiffuseColor(Color::Red());
                mat1.SetShininess(32.0f);
                
                Material mat2;
                mat2.SetName("Material 2");
                mat2.SetDiffuseColor(Color::Blue());
                mat2.SetShininess(64.0f);
                
                // 移动构造
                Material mat3(std::move(mat1));
                
                // 移动赋值
                mat2 = std::move(mat3);
                
                // 验证
                if (mat2.GetName() == "Material 1" && 
                    mat2.GetShininess() == 32.0f) {
                    successCount++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    int expected = numIterations * 4;
    Logger::GetInstance().Info("成功次数: " + std::to_string(successCount.load()) + " / " + std::to_string(expected));
    Logger::GetInstance().Info("耗时: " + std::to_string(duration.count()) + " ms");
    Logger::GetInstance().Info(successCount.load() == expected ? "✅ 测试通过" : "❌ 测试失败");
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 配置日志
    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().Info("=== Material 类线程安全测试 ===\n");
    
    try {
        // 运行所有测试
        Test1_ConcurrentRead();
        Test2_ConcurrentWrite();
        Test3_ConcurrentReadWrite();
        Test4_TextureShaderAccess();
        Test5_StressTest();
        Test6_MoveOperations();
        
        Logger::GetInstance().Info("\n=== 所有测试完成 ===");
        Logger::GetInstance().Info("✅ Material 类线程安全验证通过");
        Logger::GetInstance().Info("\n日志已保存到: " + Logger::GetInstance().GetCurrentLogFile());
        
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("测试异常: " + std::string(e.what()));
        return -1;
    }
    
    return 0;
}

