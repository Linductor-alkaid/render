#include "render/resource_dependency.h"
#include "render/logger.h"
#include <sstream>
#include <algorithm>

namespace Render {

// ============================================================================
// 依赖关系管理
// ============================================================================

void ResourceDependencyTracker::RegisterResource(const std::string& name, ResourceType type) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_dependencies.find(name) != m_dependencies.end()) {
        LOG_WARNING("ResourceDependencyTracker: 资源已注册: " + name);
        return;
    }
    
    m_dependencies[name] = ResourceDependency(name, type);
    LOG_DEBUG("ResourceDependencyTracker: 注册资源: " + name);
}

void ResourceDependencyTracker::UnregisterResource(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(name);
    if (it == m_dependencies.end()) {
        return;
    }
    
    // 移除其他资源对此资源的依赖
    for (auto& [otherName, dep] : m_dependencies) {
        auto& deps = dep.dependencies;
        deps.erase(std::remove(deps.begin(), deps.end(), name), deps.end());
    }
    
    m_dependencies.erase(it);
    LOG_DEBUG("ResourceDependencyTracker: 注销资源: " + name);
}

void ResourceDependencyTracker::AddDependency(const std::string& resourceName, 
                                              const std::string& dependencyName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end()) {
        LOG_WARNING("ResourceDependencyTracker: 资源不存在: " + resourceName);
        return;
    }
    
    auto& deps = it->second.dependencies;
    if (std::find(deps.begin(), deps.end(), dependencyName) == deps.end()) {
        deps.push_back(dependencyName);
        LOG_DEBUG("ResourceDependencyTracker: " + resourceName + " 添加依赖 -> " + dependencyName);
    }
}

void ResourceDependencyTracker::RemoveDependency(const std::string& resourceName,
                                                 const std::string& dependencyName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end()) {
        return;
    }
    
    auto& deps = it->second.dependencies;
    deps.erase(std::remove(deps.begin(), deps.end(), dependencyName), deps.end());
}

void ResourceDependencyTracker::SetDependencies(const std::string& resourceName,
                                               const std::vector<std::string>& dependencies) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end()) {
        LOG_WARNING("ResourceDependencyTracker: 资源不存在: " + resourceName);
        return;
    }
    
    it->second.dependencies = dependencies;
}

std::vector<std::string> ResourceDependencyTracker::GetDependencies(const std::string& resourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it != m_dependencies.end()) {
        return it->second.dependencies;
    }
    
    return {};
}

std::vector<std::string> ResourceDependencyTracker::GetDependents(const std::string& resourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> dependents;
    
    for (const auto& [name, dep] : m_dependencies) {
        const auto& deps = dep.dependencies;
        if (std::find(deps.begin(), deps.end(), resourceName) != deps.end()) {
            dependents.push_back(name);
        }
    }
    
    return dependents;
}

void ResourceDependencyTracker::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dependencies.clear();
    LOG_DEBUG("ResourceDependencyTracker: 已清空所有依赖信息");
}

// ============================================================================
// 循环检测
// ============================================================================

bool ResourceDependencyTracker::HasCircularReference(const std::string& resourceName) const {
    std::vector<std::string> cycle;
    return DetectCycle(resourceName, cycle);
}

bool ResourceDependencyTracker::DetectCycle(const std::string& resourceName,
                                           std::vector<std::string>& outCycle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;
    std::vector<std::string> path;
    
    if (DetectCycleInternal(resourceName, visited, recursionStack, path)) {
        outCycle = path;
        return true;
    }
    
    return false;
}

bool ResourceDependencyTracker::DetectCycleInternal(const std::string& resourceName,
                                                    std::unordered_set<std::string>& visited,
                                                    std::unordered_set<std::string>& recursionStack,
                                                    std::vector<std::string>& path) const {
    // 如果在递归栈中，说明发现循环
    if (recursionStack.find(resourceName) != recursionStack.end()) {
        // 找到循环的起始点并构建循环路径
        auto it = std::find(path.begin(), path.end(), resourceName);
        if (it != path.end()) {
            path.erase(path.begin(), it);
        }
        path.push_back(resourceName);  // 闭合循环
        return true;
    }
    
    // 如果已访问过且未在递归栈中，说明这条路径已检查过，无循环
    if (visited.find(resourceName) != visited.end()) {
        return false;
    }
    
    visited.insert(resourceName);
    recursionStack.insert(resourceName);
    path.push_back(resourceName);
    
    // 检查所有依赖
    auto it = m_dependencies.find(resourceName);
    if (it != m_dependencies.end()) {
        for (const auto& dep : it->second.dependencies) {
            if (DetectCycleInternal(dep, visited, recursionStack, path)) {
                return true;
            }
        }
    }
    
    recursionStack.erase(resourceName);
    path.pop_back();
    
    return false;
}

std::vector<CircularReference> ResourceDependencyTracker::DetectAllCycles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<CircularReference> cycles;
    std::unordered_set<std::string> globalVisited;
    
    for (const auto& [name, dep] : m_dependencies) {
        if (globalVisited.find(name) != globalVisited.end()) {
            continue;
        }
        
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recursionStack;
        std::vector<std::string> path;
        
        if (DetectCycleInternal(name, visited, recursionStack, path)) {
            CircularReference cycle;
            cycle.cycle = path;
            cycle.cycleLength = path.size();
            cycles.push_back(cycle);
            
            // 将循环中的所有资源标记为已访问，避免重复报告
            for (const auto& res : path) {
                globalVisited.insert(res);
            }
        }
    }
    
    return cycles;
}

// ============================================================================
// 依赖分析
// ============================================================================

size_t ResourceDependencyTracker::CalculateDependencyDepth(const std::string& resourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::unordered_set<std::string> visited;
    return CalculateDepthInternal(resourceName, visited);
}

size_t ResourceDependencyTracker::CalculateDepthInternal(const std::string& resourceName,
                                                        std::unordered_set<std::string>& visited) const {
    // 防止循环导致无限递归
    if (visited.find(resourceName) != visited.end()) {
        return 0;
    }
    
    visited.insert(resourceName);
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end() || it->second.dependencies.empty()) {
        return 0;
    }
    
    size_t maxDepth = 0;
    for (const auto& dep : it->second.dependencies) {
        size_t depth = CalculateDepthInternal(dep, visited);
        maxDepth = std::max(maxDepth, depth);
    }
    
    return maxDepth + 1;
}

std::unordered_set<std::string> ResourceDependencyTracker::GetAllDependencies(const std::string& resourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::unordered_set<std::string> result;
    std::unordered_set<std::string> visited;
    GetAllDependenciesInternal(resourceName, result, visited);
    
    return result;
}

void ResourceDependencyTracker::GetAllDependenciesInternal(const std::string& resourceName,
                                                          std::unordered_set<std::string>& result,
                                                          std::unordered_set<std::string>& visited) const {
    if (visited.find(resourceName) != visited.end()) {
        return;
    }
    
    visited.insert(resourceName);
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end()) {
        return;
    }
    
    for (const auto& dep : it->second.dependencies) {
        result.insert(dep);
        GetAllDependenciesInternal(dep, result, visited);
    }
}

DependencyAnalysisResult ResourceDependencyTracker::AnalyzeDependencies() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DependencyAnalysisResult result;
    result.totalResources = m_dependencies.size();
    
    // 检测循环引用（内部实现，无需额外加锁）
    std::unordered_set<std::string> globalVisited;
    
    for (const auto& [name, dep] : m_dependencies) {
        if (globalVisited.find(name) != globalVisited.end()) {
            continue;
        }
        
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recursionStack;
        std::vector<std::string> path;
        
        if (DetectCycleInternal(name, visited, recursionStack, path)) {
            CircularReference cycle;
            cycle.cycle = path;
            cycle.cycleLength = path.size();
            result.circularReferences.push_back(cycle);
            
            // 将循环中的所有资源标记为已访问，避免重复报告
            for (const auto& res : path) {
                globalVisited.insert(res);
            }
        }
    }
    
    // 计算每个资源的依赖深度（内部实现，无需额外加锁）
    for (const auto& [name, dep] : m_dependencies) {
        std::unordered_set<std::string> visited;
        size_t depth = CalculateDepthInternal(name, visited);
        result.dependencyDepth[name] = depth;
        result.maxDepth = std::max(result.maxDepth, depth);
        
        if (dep.dependencies.empty()) {
            result.isolatedResources++;
        }
    }
    
    return result;
}

// ============================================================================
// 可视化和调试
// ============================================================================

std::string ResourceDependencyTracker::GenerateDOTGraph() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream oss;
    oss << "digraph ResourceDependencies {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box, style=rounded];\n\n";
    
    // 定义节点
    for (const auto& [name, dep] : m_dependencies) {
        std::string color;
        switch (dep.resourceType) {
            case ResourceType::Texture:  color = "lightblue"; break;
            case ResourceType::Mesh:     color = "lightgreen"; break;
            case ResourceType::Material: color = "lightyellow"; break;
            case ResourceType::Shader:   color = "lightpink"; break;
            case ResourceType::SpriteAtlas: color = "lightgray"; break;
            default:                     color = "white"; break;
        }
        
        oss << "  \"" << name << "\" [fillcolor=" << color << ", style=filled];\n";
    }
    
    oss << "\n";
    
    // 定义边
    for (const auto& [name, dep] : m_dependencies) {
        for (const auto& dependency : dep.dependencies) {
            oss << "  \"" << name << "\" -> \"" << dependency << "\";\n";
        }
    }
    
    oss << "}\n";
    
    return oss.str();
}

std::string ResourceDependencyTracker::PrintDependencyTree(const std::string& resourceName,
                                                          size_t maxDepth) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string output;
    std::unordered_set<std::string> visited;
    
    output += "Dependency Tree for: " + resourceName + "\n";
    PrintTreeInternal(resourceName, 0, maxDepth, "", visited, output);
    
    return output;
}

void ResourceDependencyTracker::PrintTreeInternal(const std::string& resourceName,
                                                 size_t currentDepth,
                                                 size_t maxDepth,
                                                 const std::string& prefix,
                                                 std::unordered_set<std::string>& visited,
                                                 std::string& output) const {
    if (maxDepth > 0 && currentDepth >= maxDepth) {
        return;
    }
    
    bool alreadyVisited = visited.find(resourceName) != visited.end();
    
    auto it = m_dependencies.find(resourceName);
    if (it == m_dependencies.end()) {
        output += prefix + "└─ " + resourceName + " [NOT FOUND]\n";
        return;
    }
    
    const auto& dep = it->second;
    std::string typeStr = ResourceTypeToString(dep.resourceType);
    
    output += prefix + "├─ " + resourceName + " (" + typeStr + ")";
    
    if (alreadyVisited) {
        output += " [CIRCULAR REFERENCE!]\n";
        return;
    }
    
    if (dep.dependencies.empty()) {
        output += "\n";
        return;
    }
    
    output += "\n";
    visited.insert(resourceName);
    
    for (size_t i = 0; i < dep.dependencies.size(); ++i) {
        bool isLast = (i == dep.dependencies.size() - 1);
        std::string newPrefix = prefix + (isLast ? "    " : "│   ");
        PrintTreeInternal(dep.dependencies[i], currentDepth + 1, maxDepth, newPrefix, visited, output);
    }
}

std::string ResourceDependencyTracker::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t textureCount = 0, meshCount = 0, materialCount = 0, shaderCount = 0, atlasCount = 0;
    size_t totalDeps = 0;
    size_t isolatedCount = 0;
    
    for (const auto& [name, dep] : m_dependencies) {
        switch (dep.resourceType) {
            case ResourceType::Texture:  textureCount++; break;
            case ResourceType::Mesh:     meshCount++; break;
            case ResourceType::Material: materialCount++; break;
            case ResourceType::Shader:   shaderCount++; break;
            case ResourceType::SpriteAtlas: atlasCount++; break;
            default: break;
        }
        
        totalDeps += dep.dependencies.size();
        if (dep.dependencies.empty()) {
            isolatedCount++;
        }
    }
    
    std::ostringstream oss;
    oss << "=== Resource Dependency Statistics ===\n";
    oss << "Total Resources: " << m_dependencies.size() << "\n";
    oss << "  - Textures:  " << textureCount << "\n";
    oss << "  - Meshes:    " << meshCount << "\n";
    oss << "  - Materials: " << materialCount << "\n";
    oss << "  - Shaders:   " << shaderCount << "\n";
    oss << "  - Atlases:   " << atlasCount << "\n";
    oss << "Total Dependencies: " << totalDeps << "\n";
    oss << "Isolated Resources: " << isolatedCount << "\n";
    oss << "Average Dependencies per Resource: " 
        << (m_dependencies.empty() ? 0.0 : static_cast<double>(totalDeps) / m_dependencies.size()) << "\n";
    
    return oss.str();
}

// ============================================================================
// 引用计数管理
// ============================================================================

void ResourceDependencyTracker::UpdateReferenceCount(const std::string& resourceName, size_t refCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it != m_dependencies.end()) {
        it->second.referenceCount = refCount;
    }
}

size_t ResourceDependencyTracker::GetReferenceCount(const std::string& resourceName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_dependencies.find(resourceName);
    if (it != m_dependencies.end()) {
        return it->second.referenceCount;
    }
    
    return 0;
}

// ============================================================================
// 辅助方法
// ============================================================================

std::string ResourceDependencyTracker::ResourceTypeToString(ResourceType type) const {
    switch (type) {
        case ResourceType::Texture:  return "Texture";
        case ResourceType::Mesh:     return "Mesh";
        case ResourceType::Material: return "Material";
        case ResourceType::Shader:   return "Shader";
        case ResourceType::SpriteAtlas: return "SpriteAtlas";
        default:                     return "Unknown";
    }
}

} // namespace Render

