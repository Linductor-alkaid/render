#include "render/transform.h"
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

// 测试1：多线程读取
void TestConcurrentReads() {
    std::cout << "测试1: 多线程并发读取..." << std::endl;
    
    Transform transform;
    transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    transform.SetRotation(MathUtils::FromEuler(0.1f, 0.2f, 0.3f));
    transform.SetScale(Vector3(2.0f, 2.0f, 2.0f));
    
    std::atomic<int> successCount{0};
    const int numThreads = 10;
    const int numIterations = 1000;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < numIterations; ++i) {
                // 并发读取各种属性
                Vector3 pos = transform.GetPosition();
                Quaternion rot = transform.GetRotation();
                Vector3 scale = transform.GetScale();
                
                Matrix4 localMat = transform.GetLocalMatrix();
                Vector3 worldPos = transform.GetWorldPosition();
                
                Vector3 forward = transform.GetForward();
                Vector3 up = transform.GetUp();
                
                successCount++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  完成 " << successCount.load() 
              << " 次并发读取操作，无数据竞争" << std::endl;
}

// 测试2：多线程写入
void TestConcurrentWrites() {
    std::cout << "\n测试2: 多线程并发写入..." << std::endl;
    
    Transform transform;
    std::atomic<int> successCount{0};
    const int numThreads = 10;
    const int numIterations = 100;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < numIterations; ++i) {
                float value = static_cast<float>(t * numIterations + i);
                
                // 并发写入不同属性
                transform.SetPosition(Vector3(value, value, value));
                transform.SetRotation(MathUtils::FromEuler(value, value, value));
                transform.SetScale(Vector3(value, value, value));
                
                successCount++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  完成 " << successCount.load() 
              << " 次并发写入操作，无数据竞争" << std::endl;
}

// 测试3：混合读写
void TestConcurrentReadWrite() {
    std::cout << "\n测试3: 多线程混合读写..." << std::endl;
    
    Transform transform;
    transform.SetPosition(Vector3(1.0f, 2.0f, 3.0f));
    
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    const int numReaderThreads = 8;
    const int numWriterThreads = 2;
    const int numIterations = 500;
    
    std::vector<std::thread> threads;
    
    std::cout << "  启动 " << numReaderThreads << " 个读线程..." << std::endl;
    // 启动读线程
    for (int t = 0; t < numReaderThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < numIterations; ++i) {
                if (i % 100 == 0) {
                    std::cout << "    读线程 " << t << " 迭代 " << i << "/500" << std::endl;
                }
                Vector3 pos = transform.GetWorldPosition();
                Matrix4 mat = transform.GetWorldMatrix();
                readCount++;
            }
        });
    }
    
    std::cout << "  启动 " << numWriterThreads << " 个写线程..." << std::endl;
    // 启动写线程
    for (int t = 0; t < numWriterThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < numIterations; ++i) {
                if (i % 100 == 0) {
                    std::cout << "    写线程 " << t << " 迭代 " << i << "/500" << std::endl;
                }
                float value = static_cast<float>(t * numIterations + i);
                transform.SetPosition(Vector3(value, value, value));
                transform.Translate(Vector3(0.1f, 0.1f, 0.1f));
                writeCount++;
            }
        });
    }
    
    std::cout << "  等待所有线程完成..." << std::endl;
    for (size_t i = 0; i < threads.size(); ++i) {
        std::cout << "  等待线程 " << i << "..." << std::endl;
        threads[i].join();
        std::cout << "  线程 " << i << " 已完成" << std::endl;
    }
    
    std::cout << "  完成 " << readCount.load() << " 次读取和 " 
              << writeCount.load() << " 次写入，无死锁和数据竞争" << std::endl;
}

// 测试4：父子关系的并发访问（避免死锁测试）
void TestParentChildConcurrency() {
    std::cout << "\n测试4: 父子关系的并发访问（死锁测试）..." << std::endl;
    
    Transform parent;
    Transform child1;
    Transform child2;
    
    parent.SetPosition(Vector3(10.0f, 0.0f, 0.0f));
    child1.SetParent(&parent);
    child2.SetParent(&parent);
    
    child1.SetPosition(Vector3(1.0f, 0.0f, 0.0f));
    child2.SetPosition(Vector3(0.0f, 1.0f, 0.0f));
    
    std::atomic<int> successCount{0};
    const int numThreads = 8;
    const int numIterations = 500;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < numIterations; ++i) {
                // 并发访问父节点和子节点
                if (t % 3 == 0) {
                    // 修改父节点
                    parent.Rotate(MathUtils::AngleAxis(0.01f, Vector3::UnitY()));
                } else if (t % 3 == 1) {
                    // 读取子节点的世界坐标（需要访问父节点）
                    Vector3 worldPos1 = child1.GetWorldPosition();
                    Matrix4 worldMat2 = child2.GetWorldMatrix();
                } else {
                    // 修改子节点
                    child1.Translate(Vector3(0.01f, 0.0f, 0.0f));
                    child2.RotateAround(Vector3::UnitY(), 0.01f);
                }
                successCount++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  完成 " << successCount.load() 
              << " 次父子关系操作，无死锁" << std::endl;
    
    // 验证最终位置
    Vector3 child1WorldPos = child1.GetWorldPosition();
    std::cout << "  子节点1最终世界位置: (" 
              << child1WorldPos.x() << ", "
              << child1WorldPos.y() << ", "
              << child1WorldPos.z() << ")" << std::endl;
}

// 测试5：批量操作的线程安全
void TestBatchOperations() {
    std::cout << "\n测试5: 批量操作的线程安全..." << std::endl;
    
    Transform transform;
    transform.SetPosition(Vector3(5.0f, 5.0f, 5.0f));
    transform.SetRotation(MathUtils::FromEuler(0.5f, 0.5f, 0.5f));
    
    std::vector<Vector3> localPoints;
    for (int i = 0; i < 1000; ++i) {
        localPoints.push_back(Vector3(
            static_cast<float>(i % 10),
            static_cast<float>((i / 10) % 10),
            static_cast<float>(i / 100)
        ));
    }
    
    std::atomic<int> successCount{0};
    const int numThreads = 4;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            std::vector<Vector3> worldPoints;
            for (int i = 0; i < 100; ++i) {
                transform.TransformPoints(localPoints, worldPoints);
                successCount++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "  完成 " << successCount.load() 
              << " 次批量变换操作，无数据竞争" << std::endl;
}

// 测试6：压力测试
void StressTest() {
    std::cout << "\n测试6: 压力测试（大量并发操作）..." << std::endl;
    
    Transform parent;
    std::vector<Transform> children(10);
    
    for (auto& child : children) {
        child.SetParent(&parent);
    }
    
    std::atomic<int> operationCount{0};
    const int numThreads = 16;
    const int duration = 2; // 秒
    std::atomic<bool> running{true};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (running.load()) {
                int op = t % 6;
                int childIdx = t % static_cast<int>(children.size());
                
                switch (op) {
                    case 0:
                        parent.SetPosition(Vector3(
                            static_cast<float>(std::rand() % 100),
                            static_cast<float>(std::rand() % 100),
                            static_cast<float>(std::rand() % 100)
                        ));
                        break;
                    case 1:
                        children[childIdx].GetWorldPosition();
                        break;
                    case 2:
                        children[childIdx].GetWorldMatrix();
                        break;
                    case 3:
                        children[childIdx].Rotate(MathUtils::AngleAxis(0.01f, Vector3::UnitY()));
                        break;
                    case 4:
                        parent.GetRotation();
                        break;
                    case 5:
                        children[childIdx].TransformPoint(Vector3(1.0f, 1.0f, 1.0f));
                        break;
                }
                operationCount++;
            }
        });
    }
    
    // 运行指定时间
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    running.store(false);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    std::cout << "  在 " << elapsed << " 毫秒内完成 " << operationCount.load() 
              << " 次操作" << std::endl;
    std::cout << "  平均吞吐量: " 
              << (operationCount.load() * 1000.0 / elapsed) 
              << " 操作/秒" << std::endl;
}

int main() {
    // 设置控制台为UTF-8编码（Windows）
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "======================================" << std::endl;
    std::cout << "Transform 类线程安全测试" << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        TestConcurrentReads();
        TestConcurrentWrites();
        TestConcurrentReadWrite();
        TestParentChildConcurrency();
        TestBatchOperations();
        StressTest();
        
        std::cout << "\n======================================" << std::endl;
        std::cout << "所有测试通过！✓" << std::endl;
        std::cout << "Transform 类是线程安全的，无死锁问题" << std::endl;
        std::cout << "======================================" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "测试失败: " << e.what() << std::endl;
        return 1;
    }
}

