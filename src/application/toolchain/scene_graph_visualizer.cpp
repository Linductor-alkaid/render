#include "render/application/toolchain/scene_graph_visualizer.h"

#include "render/application/scene_graph.h"
#include "render/logger.h"

#include <algorithm>
#include <sstream>

namespace Render::Application {

SceneGraphVisualizerDataSource::SceneGraphVisualizerDataSource() = default;

void SceneGraphVisualizerDataSource::SetSceneGraph(SceneGraph* sceneGraph) {
    m_sceneGraph = sceneGraph;
}

std::optional<SceneNodeInfo> SceneGraphVisualizerDataSource::GetRootNodeInfo() const {
    if (!m_sceneGraph) {
        return std::nullopt;
    }

    auto root = m_sceneGraph->GetRoot();
    if (!root) {
        return std::nullopt;
    }

    return ExtractNodeInfo(*root);
}

std::optional<SceneNodeInfo> SceneGraphVisualizerDataSource::GetNodeInfo(const std::string& nodeName) const {
    if (!m_sceneGraph) {
        return std::nullopt;
    }

    std::optional<SceneNodeInfo> result;

    // 递归查找节点
    std::function<bool(const SceneNode::Ptr&)> findNode = [&](const SceneNode::Ptr& node) -> bool {
        if (!node) {
            return false;
        }

        if (node->GetName() == nodeName) {
            result = ExtractNodeInfo(*node);
            return true;
        }

        for (const auto& child : node->GetChildren()) {
            if (findNode(child)) {
                return true;
            }
        }

        return false;
    };

    auto root = m_sceneGraph->GetRoot();
    if (root) {
        findNode(root);
    }

    return result;
}

std::vector<SceneNodeInfo> SceneGraphVisualizerDataSource::GetChildNodeInfos(const std::string& nodeName) const {
    std::vector<SceneNodeInfo> infos;

    if (!m_sceneGraph) {
        return infos;
    }

    std::optional<SceneNode::Ptr> targetNode;

    // 查找目标节点
    std::function<bool(const SceneNode::Ptr&)> findNode = [&](const SceneNode::Ptr& node) -> bool {
        if (!node) {
            return false;
        }

        if (node->GetName() == nodeName) {
            targetNode = node;
            return true;
        }

        for (const auto& child : node->GetChildren()) {
            if (findNode(child)) {
                return true;
            }
        }

        return false;
    };

    auto root = m_sceneGraph->GetRoot();
    if (root) {
        findNode(root);
    }

    if (targetNode.has_value()) {
        for (const auto& child : targetNode.value()->GetChildren()) {
            if (child) {
                infos.push_back(ExtractNodeInfo(*child));
            }
        }
    }

    return infos;
}

std::vector<SceneNodeInfo> SceneGraphVisualizerDataSource::GetAllNodeInfos() const {
    std::vector<SceneNodeInfo> infos;

    if (!m_sceneGraph) {
        return infos;
    }

    ForEachNode([&](const SceneNodeInfo& info, int) {
        infos.push_back(info);
    });

    return infos;
}

void SceneGraphVisualizerDataSource::ForEachNode(
    std::function<void(const SceneNodeInfo&, int depth)> callback) const {
    if (!callback || !m_sceneGraph) {
        return;
    }

    auto root = m_sceneGraph->GetRoot();
    if (root) {
        TraverseNode(root, 0, callback);
    }
}

std::string SceneGraphVisualizerDataSource::GetTreeStructure() const {
    if (!m_sceneGraph) {
        return "(Empty)";
    }

    std::ostringstream oss;
    bool isFirst = true;

    std::function<void(const SceneNode::Ptr&, int, const std::string&)> printNode = 
        [&](const SceneNode::Ptr& node, int depth, const std::string& prefix) {
            if (!node) {
                return;
            }

            if (!isFirst) {
                oss << "\n";
            }
            isFirst = false;

            oss << prefix << node->GetName();
            oss << " [" << (node->IsActive() ? "Active" : "Inactive") << "]";

            auto children = node->GetChildren();
            if (!children.empty()) {
                for (size_t i = 0; i < children.size(); ++i) {
                    bool isLast = (i == children.size() - 1);
                    std::string newPrefix = prefix + (isLast ? "  " : "│ ");
                    std::string childPrefix = prefix + (isLast ? "└─" : "├─");
                    
                    printNode(children[i], depth + 1, childPrefix);
                }
            }
        };

    auto root = m_sceneGraph->GetRoot();
    if (root) {
        printNode(root, 0, "");
    } else {
        oss << "(No root)";
    }

    return oss.str();
}

bool SceneGraphVisualizerDataSource::IsEmpty() const {
    if (!m_sceneGraph) {
        return true;
    }

    return m_sceneGraph->GetRoot() == nullptr;
}

SceneGraphVisualizerDataSource::SceneGraphStats SceneGraphVisualizerDataSource::GetStats() const {
    SceneGraphStats stats;

    if (!m_sceneGraph) {
        return stats;
    }

    int currentMaxDepth = 0;

    ForEachNode([&](const SceneNodeInfo& info, int depth) {
        stats.totalNodes++;
        if (info.active) {
            stats.activeNodes++;
        }
        if (info.attached) {
            stats.attachedNodes++;
        }
        if (info.entered) {
            stats.enteredNodes++;
        }
        stats.totalResources += info.resourceCount;
        
        if (depth > currentMaxDepth) {
            currentMaxDepth = depth;
        }
    });

    stats.maxDepth = currentMaxDepth;
    return stats;
}

SceneNodeInfo SceneGraphVisualizerDataSource::ExtractNodeInfo(const SceneNode& node) const {
    SceneNodeInfo info;
    info.name = node.GetName();
    info.active = node.IsActive();
    
    // 注意：SceneNode的内部状态（attached、entered）是protected的
    // 这里我们需要通过其他方式获取，或者SceneNode需要提供公共接口
    // 目前先使用默认值
    info.attached = false;
    info.entered = false;

    auto children = node.GetChildren();
    info.childCount = children.size();
    
    for (const auto& child : children) {
        if (child) {
            info.childrenNames.push_back(child->GetName());
        }
    }

    // 资源数量需要通过Manifest获取
    // info.resourceCount = node.CollectManifest().required.size() + node.CollectManifest().optional.size();

    return info;
}

void SceneGraphVisualizerDataSource::TraverseNode(
    const SceneNode::Ptr& node,
    int depth,
    std::function<void(const SceneNodeInfo&, int)>& callback) const {
    if (!node) {
        return;
    }

    SceneNodeInfo info = ExtractNodeInfo(*node);
    callback(info, depth);

    for (const auto& child : node->GetChildren()) {
        TraverseNode(child, depth + 1, callback);
    }
}

} // namespace Render::Application

