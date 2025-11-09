#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace Render {

/**
 * @brief 资源类型枚举
 */
enum class ResourceType {
    Texture,
    Mesh,
    Model,
    Material,
    Shader,
    SpriteAtlas,
    Font,
    Unknown
};

/**
 * @brief 资源依赖信息
 * 
 * 记录一个资源依赖了哪些其他资源
 */
struct ResourceDependency {
    std::string resourceName;           // 资源名称
    ResourceType resourceType;          // 资源类型
    std::vector<std::string> dependencies;  // 依赖的资源名称列表
    size_t referenceCount = 0;          // 引用计数
    
    ResourceDependency() = default;
    ResourceDependency(const std::string& name, ResourceType type)
        : resourceName(name), resourceType(type) {}
};

/**
 * @brief 循环引用信息
 */
struct CircularReference {
    std::vector<std::string> cycle;     // 循环路径（A → B → C → A）
    size_t cycleLength = 0;             // 循环长度
    
    std::string ToString() const {
        std::string result = "Circular reference detected: ";
        for (size_t i = 0; i < cycle.size(); ++i) {
            result += cycle[i];
            if (i < cycle.size() - 1) {
                result += " -> ";
            }
        }
        return result;
    }
};

/**
 * @brief 资源依赖分析结果
 */
struct DependencyAnalysisResult {
    std::vector<CircularReference> circularReferences;  // 发现的循环引用
    std::unordered_map<std::string, size_t> dependencyDepth;  // 每个资源的依赖深度
    size_t totalResources = 0;          // 总资源数
    size_t isolatedResources = 0;      // 孤立资源数（无依赖）
    size_t maxDepth = 0;                // 最大依赖深度
    
    bool HasCircularReferences() const {
        return !circularReferences.empty();
    }
    
    std::string GetSummary() const {
        std::string summary;
        summary += "=== Resource Dependency Analysis ===\n";
        summary += "Total Resources: " + std::to_string(totalResources) + "\n";
        summary += "Isolated Resources: " + std::to_string(isolatedResources) + "\n";
        summary += "Max Dependency Depth: " + std::to_string(maxDepth) + "\n";
        summary += "Circular References Found: " + std::to_string(circularReferences.size()) + "\n";
        
        if (HasCircularReferences()) {
            summary += "\n⚠️ WARNING: Circular references detected!\n";
            for (const auto& cycle : circularReferences) {
                summary += "  - " + cycle.ToString() + "\n";
            }
        } else {
            summary += "\n✅ No circular references detected.\n";
        }
        
        return summary;
    }
};

/**
 * @class ResourceDependencyTracker
 * @brief 资源依赖关系跟踪器
 * 
 * 功能：
 * - 跟踪资源之间的依赖关系
 * - 检测循环引用
 * - 分析依赖深度
 * - 生成依赖关系图
 * 
 * 线程安全：所有公共方法都是线程安全的
 */
class ResourceDependencyTracker {
public:
    /**
     * @brief 构造函数
     */
    ResourceDependencyTracker() = default;
    
    /**
     * @brief 析构函数
     */
    ~ResourceDependencyTracker() = default;
    
    // 禁止拷贝和移动
    ResourceDependencyTracker(const ResourceDependencyTracker&) = delete;
    ResourceDependencyTracker& operator=(const ResourceDependencyTracker&) = delete;
    
    // ========================================================================
    // 依赖关系管理
    // ========================================================================
    
    /**
     * @brief 注册资源
     * @param name 资源名称
     * @param type 资源类型
     */
    void RegisterResource(const std::string& name, ResourceType type);
    
    /**
     * @brief 注销资源
     * @param name 资源名称
     */
    void UnregisterResource(const std::string& name);
    
    /**
     * @brief 添加依赖关系
     * @param resourceName 资源名称
     * @param dependencyName 依赖的资源名称
     */
    void AddDependency(const std::string& resourceName, const std::string& dependencyName);
    
    /**
     * @brief 移除依赖关系
     * @param resourceName 资源名称
     * @param dependencyName 依赖的资源名称
     */
    void RemoveDependency(const std::string& resourceName, const std::string& dependencyName);
    
    /**
     * @brief 设置资源的所有依赖
     * @param resourceName 资源名称
     * @param dependencies 依赖列表
     */
    void SetDependencies(const std::string& resourceName, const std::vector<std::string>& dependencies);
    
    /**
     * @brief 获取资源的依赖列表
     * @param resourceName 资源名称
     * @return 依赖列表
     */
    std::vector<std::string> GetDependencies(const std::string& resourceName) const;
    
    /**
     * @brief 获取依赖某资源的资源列表（反向依赖）
     * @param resourceName 资源名称
     * @return 依赖此资源的资源列表
     */
    std::vector<std::string> GetDependents(const std::string& resourceName) const;
    
    /**
     * @brief 清空所有依赖信息
     */
    void Clear();
    
    // ========================================================================
    // 循环检测
    // ========================================================================
    
    /**
     * @brief 检测特定资源是否存在循环引用
     * @param resourceName 资源名称
     * @return 如果存在循环引用返回 true
     */
    bool HasCircularReference(const std::string& resourceName) const;
    
    /**
     * @brief 检测特定资源的循环路径
     * @param resourceName 资源名称
     * @param outCycle 输出循环路径
     * @return 如果存在循环引用返回 true
     */
    bool DetectCycle(const std::string& resourceName, std::vector<std::string>& outCycle) const;
    
    /**
     * @brief 检测所有资源的循环引用
     * @return 所有发现的循环引用列表
     */
    std::vector<CircularReference> DetectAllCycles() const;
    
    // ========================================================================
    // 依赖分析
    // ========================================================================
    
    /**
     * @brief 计算资源的依赖深度
     * @param resourceName 资源名称
     * @return 依赖深度（0表示无依赖）
     */
    size_t CalculateDependencyDepth(const std::string& resourceName) const;
    
    /**
     * @brief 获取资源的所有递归依赖（包括间接依赖）
     * @param resourceName 资源名称
     * @return 所有依赖的资源名称集合
     */
    std::unordered_set<std::string> GetAllDependencies(const std::string& resourceName) const;
    
    /**
     * @brief 执行完整的依赖分析
     * @return 分析结果
     */
    DependencyAnalysisResult AnalyzeDependencies() const;
    
    // ========================================================================
    // 可视化和调试
    // ========================================================================
    
    /**
     * @brief 生成依赖关系的DOT格式图（用于Graphviz）
     * @return DOT格式字符串
     */
    std::string GenerateDOTGraph() const;
    
    /**
     * @brief 打印依赖关系树
     * @param resourceName 资源名称
     * @param maxDepth 最大深度（0表示无限制）
     */
    std::string PrintDependencyTree(const std::string& resourceName, size_t maxDepth = 0) const;
    
    /**
     * @brief 获取依赖统计信息
     */
    std::string GetStatistics() const;
    
    // ========================================================================
    // 引用计数管理
    // ========================================================================
    
    /**
     * @brief 更新资源的引用计数
     * @param resourceName 资源名称
     * @param refCount 引用计数
     */
    void UpdateReferenceCount(const std::string& resourceName, size_t refCount);
    
    /**
     * @brief 获取资源的引用计数
     * @param resourceName 资源名称
     * @return 引用计数
     */
    size_t GetReferenceCount(const std::string& resourceName) const;
    
private:
    // 内部辅助方法（不加锁，由调用者保证线程安全）
    bool DetectCycleInternal(const std::string& resourceName,
                            std::unordered_set<std::string>& visited,
                            std::unordered_set<std::string>& recursionStack,
                            std::vector<std::string>& path) const;
    
    size_t CalculateDepthInternal(const std::string& resourceName,
                                  std::unordered_set<std::string>& visited) const;
    
    void GetAllDependenciesInternal(const std::string& resourceName,
                                   std::unordered_set<std::string>& result,
                                   std::unordered_set<std::string>& visited) const;
    
    void PrintTreeInternal(const std::string& resourceName,
                          size_t currentDepth,
                          size_t maxDepth,
                          const std::string& prefix,
                          std::unordered_set<std::string>& visited,
                          std::string& output) const;
    
    std::string ResourceTypeToString(ResourceType type) const;
    
    // 资源依赖图
    std::unordered_map<std::string, ResourceDependency> m_dependencies;
    
    // 线程安全
    mutable std::mutex m_mutex;
};

} // namespace Render

