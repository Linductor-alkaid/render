#pragma once

#include "render/application/scene_graph.h"

#include <optional>
#include <string>
#include <vector>
#include <functional>

namespace Render::Application {

/**
 * @brief 场景图节点信息（用于可视化）
 */
struct SceneNodeInfo {
    std::string name;
    bool active = true;
    bool attached = false;
    bool entered = false;
    size_t childCount = 0;
    std::vector<std::string> childrenNames;
    size_t resourceCount = 0;  // 注册的资源数量
};

/**
 * @brief 场景图可视化数据源接口
 * 
 * 为工具链提供场景图的可视化功能
 */
class SceneGraphVisualizerDataSource {
public:
    SceneGraphVisualizerDataSource();
    ~SceneGraphVisualizerDataSource() = default;

    /**
     * @brief 设置场景图根节点
     */
    void SetSceneGraph(SceneGraph* sceneGraph);

    /**
     * @brief 获取场景图根节点信息
     * @return 根节点信息，如果没有根节点返回 std::nullopt
     */
    std::optional<SceneNodeInfo> GetRootNodeInfo() const;

    /**
     * @brief 获取节点信息
     * @param nodeName 节点名称
     * @return 节点信息，如果节点不存在返回 std::nullopt
     */
    std::optional<SceneNodeInfo> GetNodeInfo(const std::string& nodeName) const;

    /**
     * @brief 获取节点的子节点信息列表
     * @param nodeName 节点名称
     * @return 子节点信息列表
     */
    std::vector<SceneNodeInfo> GetChildNodeInfos(const std::string& nodeName) const;

    /**
     * @brief 获取场景图的所有节点信息（扁平列表）
     */
    std::vector<SceneNodeInfo> GetAllNodeInfos() const;

    /**
     * @brief 遍历场景图的所有节点
     * @param callback 回调函数 (nodeInfo, depth)
     */
    void ForEachNode(std::function<void(const SceneNodeInfo&, int depth)> callback) const;

    /**
     * @brief 获取场景图的层级结构（树形）
     * @return 树形结构的字符串表示（用于显示）
     */
    std::string GetTreeStructure() const;

    /**
     * @brief 检查场景图是否为空
     */
    bool IsEmpty() const;

    /**
     * @brief 获取场景图的统计信息
     */
    struct SceneGraphStats {
        size_t totalNodes = 0;
        size_t activeNodes = 0;
        size_t attachedNodes = 0;
        size_t enteredNodes = 0;
        size_t totalResources = 0;
        int maxDepth = 0;
    };

    SceneGraphStats GetStats() const;

private:
    SceneGraph* m_sceneGraph = nullptr;

    /**
     * @brief 从SceneNode提取节点信息
     */
    SceneNodeInfo ExtractNodeInfo(const SceneNode& node) const;

    /**
     * @brief 递归遍历节点树
     */
    void TraverseNode(const SceneNode::Ptr& node, int depth, 
                     std::function<void(const SceneNodeInfo&, int)>& callback) const;
};

} // namespace Render::Application

