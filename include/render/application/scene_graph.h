#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "render/application/scene_types.h"

namespace Render {
class ResourceManager;
namespace ECS {
class World;
}

namespace Application {

class Scene;
class AppContext;

class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    using Ptr = std::shared_ptr<SceneNode>;
    using WeakPtr = std::weak_ptr<SceneNode>;

    explicit SceneNode(std::string_view name);
    virtual ~SceneNode() = default;

    const std::string& GetName() const noexcept { return m_name; }

    void AddChild(const Ptr& child);
    void RemoveChild(const Ptr& child);
    std::vector<Ptr> GetChildren() const;
    Ptr GetParent() const { return m_parent.lock(); }

    void SetActive(bool active) { m_active = active; }
    bool IsActive() const noexcept { return m_active; }

    void RegisterRequiredResource(std::string identifier,
                                  std::string type,
                                  ResourceScope scope = ResourceScope::Scene);
    void RegisterOptionalResource(std::string identifier,
                                  std::string type,
                                  ResourceScope scope = ResourceScope::Scene);

    SceneResourceManifest CollectManifest() const;

protected:
    Scene& GetScene() const;
    AppContext& GetContext() const;
    ECS::World& GetWorld() const;
    ResourceManager& GetResourceManager() const;

    virtual void OnAttach(Scene& scene, AppContext& context);
    virtual void OnDetach();
    virtual void OnEnter(const SceneEnterArgs& args);
    virtual void OnExit();
    virtual void OnUpdate(const FrameUpdateArgs& frameArgs);

private:
    friend class SceneGraph;

    void Attach(Scene& scene, AppContext& context);
    void DetachInternal();
    void Enter(const SceneEnterArgs& args);
    void ExitInternal();
    void Update(const FrameUpdateArgs& frameArgs);

    void Traverse(const std::function<void(SceneNode&)>& visitor);
    void TraverseConst(const std::function<void(const SceneNode&)>& visitor) const;

    std::string m_name;
    WeakPtr m_parent;
    std::vector<Ptr> m_children;
    bool m_active = true;
    bool m_attached = false;
    bool m_entered = false;
    Scene* m_scene = nullptr;
    AppContext* m_context = nullptr;
    SceneResourceManifest m_ownManifest;
};

class SceneGraph {
public:
    SceneGraph() = default;
    explicit SceneGraph(SceneNode::Ptr root);

    void SetRoot(SceneNode::Ptr root);
    SceneNode::Ptr GetRoot() const { return m_root; }

    SceneResourceManifest BuildManifest() const;

    void Attach(Scene& scene, AppContext& context);
    void Enter(const SceneEnterArgs& args);
    void Update(const FrameUpdateArgs& frameArgs);
    void Exit();
    void Detach();

private:
    SceneNode::Ptr m_root;
    bool m_attached = false;
    bool m_entered = false;
};

} // namespace Application
} // namespace Render


