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
#include "render/application/scene_graph.h"

#include <algorithm>
#include <cassert>

#include "render/application/app_context.h"
#include "render/application/scene.h"
#include "render/resource_manager.h"
#include "render/ecs/world.h"

namespace Render::Application {

// ============================== SceneNode ==============================

SceneNode::SceneNode(std::string_view name)
    : m_name(name) {}

void SceneNode::AddChild(const Ptr& child) {
    if (!child) {
        return;
    }
    if (child.get() == this) {
        return;
    }
    if (auto currentParent = child->m_parent.lock()) {
        if (currentParent.get() == this) {
            return;
        }
        currentParent->RemoveChild(child);
    }
    child->m_parent = shared_from_this();
    m_children.push_back(child);
    if (m_attached && !child->m_attached) {
        child->Attach(*m_scene, *m_context);
    }
    if (m_entered && !child->m_entered) {
        child->Enter(SceneEnterArgs{});
    }
}

void SceneNode::RemoveChild(const Ptr& child) {
    if (!child) {
        return;
    }
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it == m_children.end()) {
        return;
    }
    (*it)->ExitInternal();
    (*it)->DetachInternal();
    (*it)->m_parent.reset();
    m_children.erase(it);
}

std::vector<SceneNode::Ptr> SceneNode::GetChildren() const {
    return m_children;
}

void SceneNode::RegisterRequiredResource(std::string identifier,
                                         std::string type,
                                         ResourceScope scope) {
    ResourceRequest request{std::move(identifier), std::move(type), scope, false};
    m_ownManifest.required.push_back(std::move(request));
}

void SceneNode::RegisterOptionalResource(std::string identifier,
                                         std::string type,
                                         ResourceScope scope) {
    ResourceRequest request{std::move(identifier), std::move(type), scope, true};
    m_ownManifest.optional.push_back(std::move(request));
}

SceneResourceManifest SceneNode::CollectManifest() const {
    SceneResourceManifest manifest = m_ownManifest;
    for (const auto& child : m_children) {
        if (child) {
            manifest.Merge(child->CollectManifest());
        }
    }
    return manifest;
}

Scene& SceneNode::GetScene() const {
    assert(m_scene != nullptr && "SceneNode accessed without scene");
    return *m_scene;
}

AppContext& SceneNode::GetContext() const {
    assert(m_context != nullptr && "SceneNode accessed without context");
    return *m_context;
}

ECS::World& SceneNode::GetWorld() const {
    auto& ctx = GetContext();
    assert(ctx.world != nullptr && "SceneNode requires valid World");
    return *ctx.world;
}

ResourceManager& SceneNode::GetResourceManager() const {
    auto& ctx = GetContext();
    assert(ctx.resourceManager != nullptr && "SceneNode requires ResourceManager");
    return *ctx.resourceManager;
}

void SceneNode::OnAttach(Scene&, AppContext&) {}
void SceneNode::OnDetach() {}
void SceneNode::OnEnter(const SceneEnterArgs&) {}
void SceneNode::OnExit() {}
void SceneNode::OnUpdate(const FrameUpdateArgs&) {}

void SceneNode::Attach(Scene& scene, AppContext& context) {
    if (m_attached) {
        return;
    }
    m_scene = &scene;
    m_context = &context;
    OnAttach(scene, context);
    m_attached = true;
    for (auto& child : m_children) {
        if (child) {
            child->Attach(scene, context);
        }
    }
}

void SceneNode::DetachInternal() {
    if (!m_attached) {
        return;
    }
    for (auto& child : m_children) {
        if (child) {
            child->DetachInternal();
        }
    }
    OnDetach();
    m_attached = false;
    m_context = nullptr;
    m_scene = nullptr;
}

void SceneNode::Enter(const SceneEnterArgs& args) {
    if (m_entered || !m_active) {
        return;
    }
    OnEnter(args);
    m_entered = true;
    for (auto& child : m_children) {
        if (child) {
            child->Enter(args);
        }
    }
}

void SceneNode::ExitInternal() {
    if (!m_entered) {
        return;
    }
    for (auto& child : m_children) {
        if (child) {
            child->ExitInternal();
        }
    }
    OnExit();
    m_entered = false;
}

void SceneNode::Update(const FrameUpdateArgs& frameArgs) {
    if (!m_active) {
        return;
    }
    OnUpdate(frameArgs);
    for (auto& child : m_children) {
        if (child) {
            child->Update(frameArgs);
        }
    }
}

void SceneNode::Traverse(const std::function<void(SceneNode&)>& visitor) {
    visitor(*this);
    for (auto& child : m_children) {
        if (child) {
            child->Traverse(visitor);
        }
    }
}

void SceneNode::TraverseConst(const std::function<void(const SceneNode&)>& visitor) const {
    visitor(*this);
    for (const auto& child : m_children) {
        if (child) {
            child->TraverseConst(visitor);
        }
    }
}

// ============================== SceneGraph ==============================

SceneGraph::SceneGraph(SceneNode::Ptr root)
    : m_root(std::move(root)) {}

void SceneGraph::SetRoot(SceneNode::Ptr root) {
    if (m_root && m_attached) {
        m_root->ExitInternal();
        m_root->DetachInternal();
    }
    m_root = std::move(root);
    m_attached = false;
    m_entered = false;
}

SceneResourceManifest SceneGraph::BuildManifest() const {
    if (!m_root) {
        return {};
    }
    return m_root->CollectManifest();
}

void SceneGraph::Attach(Scene& scene, AppContext& context) {
    if (!m_root || m_attached) {
        return;
    }
    m_root->Attach(scene, context);
    m_attached = true;
}

void SceneGraph::Enter(const SceneEnterArgs& args) {
    if (!m_root || m_entered) {
        return;
    }
    m_root->Enter(args);
    m_entered = true;
}

void SceneGraph::Update(const FrameUpdateArgs& frameArgs) {
    if (!m_root) {
        return;
    }
    m_root->Update(frameArgs);
}

void SceneGraph::Exit() {
    if (!m_root || !m_entered) {
        return;
    }
    m_root->ExitInternal();
    m_entered = false;
}

void SceneGraph::Detach() {
    if (!m_root || !m_attached) {
        return;
    }
    m_root->DetachInternal();
    m_attached = false;
}

} // namespace Render::Application


