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
#include "render/application/scenes/boot_scene.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "render/application/events/frame_events.h"
#include "render/application/module_registry.h"
#include "render/camera.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"
#include "render/ecs/world.h"
#include "render/logger.h"
#include "render/material.h"
#include "render/math_utils.h"
#include "render/mesh_loader.h"
#include "render/render_layer.h"
#include "render/resource_manager.h"
#include "render/shader_cache.h"
#include "render/lighting/light.h"

namespace Render::Application {

namespace {
constexpr const char* kCubeMeshName = "boot.demo.mesh";
constexpr const char* kCubeMaterialName = "boot.demo.material";
constexpr const char* kCubeShaderName = "boot.demo.shader";

class CubeNode final : public SceneNode {
public:
    CubeNode()
        : SceneNode("CubeNode") {
        RegisterRequiredResource(kCubeMeshName, "mesh");
        RegisterRequiredResource(kCubeMaterialName, "material");
        RegisterRequiredResource(kCubeShaderName, "shader");
    }

protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto& resourceManager = *ctx.resourceManager;
        if (!resourceManager.HasMesh(kCubeMeshName)) {
            auto mesh = MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::White());
            resourceManager.RegisterMesh(kCubeMeshName, mesh);
        }

        auto shader = ShaderCache::GetInstance().LoadShader(
            kCubeShaderName, "shaders/material_phong.vert", "shaders/material_phong.frag");

        if (!resourceManager.HasMaterial(kCubeMaterialName)) {
            auto material = std::make_shared<Material>();
            material->SetName(kCubeMaterialName);
            material->SetShader(shader);
            material->SetAmbientColor(Color(0.15f, 0.15f, 0.15f, 1.0f));
            material->SetDiffuseColor(Color(0.2f, 0.6f, 1.0f, 1.0f));
            material->SetSpecularColor(Color(0.9f, 0.9f, 0.9f, 1.0f));
            material->SetShininess(64.0f);
            resourceManager.RegisterMaterial(kCubeMaterialName, material);
        }
    }

    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();

        ECS::EntityDescriptor descriptor;
        descriptor.name = "BootScene.Cube";
        descriptor.tags = {"boot", "demo"};
        m_entity = world.CreateEntity(descriptor);

        ECS::TransformComponent transform{};
        transform.SetPosition(Vector3::Zero());
        world.AddComponent(*m_entity, transform);

        auto& resourceManager = GetResourceManager();

        ECS::MeshRenderComponent meshComp;
        meshComp.meshName = kCubeMeshName;
        meshComp.materialName = kCubeMaterialName;
        meshComp.mesh = resourceManager.GetMesh(kCubeMeshName);
        meshComp.material = resourceManager.GetMaterial(kCubeMaterialName);
        meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
        meshComp.asyncLoading = false;
        meshComp.layerID = Layers::World::Midground.value;
        meshComp.SetDiffuseColor(Color(0.3f, 0.7f, 1.0f, 1.0f));
        meshComp.materialOverride.specularColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
        meshComp.materialOverride.shininess = 64.0f;
        world.AddComponent(*m_entity, meshComp);
    }

    void OnUpdate(const FrameUpdateArgs& frame) override {
        if (!m_entity.has_value()) {
            return;
        }
        auto& world = GetWorld();
        if (!world.IsValidEntity(*m_entity)) {
            return;
        }
        auto& transform = world.GetComponent<ECS::TransformComponent>(*m_entity);
        const float offset = 0.1f * std::sin(static_cast<float>(frame.absoluteTime));
        auto position = transform.GetPosition();
        position.x() = offset;
        transform.SetPosition(position);
        const float rotationSpeed = 45.0f;
        float angle = rotationSpeed * static_cast<float>(frame.absoluteTime);
        transform.SetRotation(MathUtils::FromEulerDegrees(20.0f, angle, 0.0f));
    }

    void OnExit() override {
        if (!m_entity.has_value()) {
            return;
        }
        auto& world = GetWorld();
        if (world.IsValidEntity(*m_entity)) {
            world.DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

class CameraNode final : public SceneNode {
public:
    CameraNode()
        : SceneNode("CameraNode") {}

protected:
    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "BootScene.Camera", .active = true});

        ECS::TransformComponent cameraTransform{};
        cameraTransform.SetPosition(Vector3(0.0f, 1.5f, 4.0f));
        cameraTransform.transform->LookAt(Vector3::Zero());
        world.AddComponent(*m_entity, cameraTransform);

        ECS::CameraComponent cameraComp;
        cameraComp.camera = std::make_shared<Camera>();
        auto* renderer = GetContext().renderer;
        if (renderer) {
            const float width = static_cast<float>(renderer->GetWidth());
            const float height = static_cast<float>(renderer->GetHeight());
            cameraComp.camera->SetPerspective(60.0f, width / std::max(1.0f, height), 0.1f, 100.0f);
        } else {
            cameraComp.camera->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        }
        cameraComp.depth = 0;
        cameraComp.clearColor = Color(0.05f, 0.08f, 0.12f, 1.0f);
        cameraComp.layerMask = 0xFFFFFFFFu;  // 确保所有层都可见（包括世界层和UI层）
        world.AddComponent(*m_entity, cameraComp);
    }

    void OnExit() override {
        if (!m_entity.has_value()) {
            return;
        }
        auto& world = GetWorld();
        if (world.IsValidEntity(*m_entity)) {
            world.DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

class PointLightNode final : public SceneNode {
public:
    PointLightNode()
        : SceneNode("PointLightNode") {}

protected:
    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "BootScene.Light", .active = true});

        ECS::TransformComponent lightTransform{};
        lightTransform.SetPosition(Vector3(2.0f, 3.0f, 2.0f));
        world.AddComponent(*m_entity, lightTransform);

        ECS::LightComponent lightComp;
        lightComp.type = ECS::LightType::Point;
        lightComp.color = Color(1.0f, 0.95f, 0.85f);
        lightComp.intensity = 4.0f;
        lightComp.range = 10.0f;
        lightComp.attenuation = 0.35f;
        lightComp.enabled = true;
        world.AddComponent(*m_entity, lightComp);
    }

    void OnExit() override {
        if (!m_entity.has_value()) {
            return;
        }
        auto& world = GetWorld();
        if (world.IsValidEntity(*m_entity)) {
            world.DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

} // namespace

void BootScene::OnAttach(AppContext& ctx, ModuleRegistry&) {
    m_context = &ctx;
    SubscribeFrameEvents();

    Logger::GetInstance().InfoFormat(
        "[BootScene] OnAttach: renderer=%p resourceManager=%p asyncLoader=%p world=%p",
        static_cast<void*>(ctx.renderer),
        static_cast<void*>(ctx.resourceManager),
        static_cast<void*>(ctx.asyncLoader),
        ctx.world);

    if (ctx.world) {
        ECS::World& world = *ctx.world;
        world.RegisterComponent<ECS::TransformComponent>();
        world.RegisterComponent<ECS::MeshRenderComponent>();
        world.RegisterComponent<ECS::CameraComponent>();
        world.RegisterComponent<ECS::LightComponent>();
        world.RegisterComponent<ECS::NameComponent>();

        if (!world.GetSystem<ECS::TransformSystem>()) {
            world.RegisterSystem<ECS::TransformSystem>();
        }
        if (!world.GetSystem<ECS::CameraSystem>()) {
            world.RegisterSystem<ECS::CameraSystem>();
        }
        if (ctx.renderer) {
            if (!world.GetSystem<ECS::MeshRenderSystem>()) {
                world.RegisterSystem<ECS::MeshRenderSystem>(ctx.renderer);
            }
            if (!world.GetSystem<ECS::LightSystem>()) {
                world.RegisterSystem<ECS::LightSystem>(ctx.renderer);
            }
            if (!world.GetSystem<ECS::UniformSystem>()) {
                world.RegisterSystem<ECS::UniformSystem>(ctx.renderer);
            }
            if (ctx.asyncLoader && !world.GetSystem<ECS::ResourceLoadingSystem>()) {
                auto* loaderSystem = world.RegisterSystem<ECS::ResourceLoadingSystem>(ctx.asyncLoader);
                if (loaderSystem) {
                    loaderSystem->SetMaxTasksPerFrame(16);
                    Logger::GetInstance().InfoFormat(
                        "[BootScene] ResourceLoadingSystem registered (maxPerFrame=%zu)",
                        loaderSystem->GetMaxTasksPerFrame());
                }
            } else {
                Logger::GetInstance().Info("[BootScene] Async loader unavailable - ResourceLoadingSystem not registered");
            }
        }

        world.PostInitialize();
    }

    auto root = std::make_shared<SceneNode>("BootScene.Root");
    root->AddChild(std::make_shared<CubeNode>());
    root->AddChild(std::make_shared<CameraNode>());
    root->AddChild(std::make_shared<PointLightNode>());
    m_sceneGraph.SetRoot(root);
    m_sceneGraph.Attach(*this, ctx);
}

void BootScene::OnDetach(AppContext&) {
    m_sceneGraph.Detach();
    UnsubscribeFrameEvents();
    m_context = nullptr;
}

SceneResourceManifest BootScene::BuildManifest() const {
    return m_sceneGraph.BuildManifest();
}

void BootScene::OnEnter(const SceneEnterArgs& args) {
    Logger::GetInstance().Info("[BootScene] Enter");
    m_sceneGraph.Enter(args);
    if (args.previousSnapshot.has_value()) {
        Logger::GetInstance().Info("[BootScene] Restored from previous snapshot");
    }

    if (m_context && m_context->world) {
        Logger::GetInstance().Info("[BootScene] Dump world statistics on enter");
        m_context->world->PrintStatistics();
    }
}

void BootScene::OnUpdate(const FrameUpdateArgs& frame) {
    static uint64_t s_lastLogFrame = std::numeric_limits<uint64_t>::max();
    if (frame.frameIndex == 0 || frame.frameIndex - s_lastLogFrame >= 120) {
        Logger::GetInstance().DebugFormat(
            "[BootScene] OnUpdate frame=%llu dt=%.4f absTime=%.2f",
            static_cast<unsigned long long>(frame.frameIndex),
            frame.deltaTime,
            frame.absoluteTime);
        s_lastLogFrame = frame.frameIndex;
    }

    m_sceneGraph.Update(frame);
}

SceneSnapshot BootScene::OnExit(const SceneExitArgs&) {
    Logger::GetInstance().Info("[BootScene] Exit");
    m_sceneGraph.Exit();
    SceneSnapshot snapshot;
    snapshot.sceneId = std::string(Name());
    return snapshot;
}

void BootScene::SubscribeFrameEvents() {
    if (!m_context || !m_context->globalEventBus) {
        return;
    }

    if (m_frameListener != 0) {
        return;
    }

    m_frameListener = m_context->globalEventBus->Subscribe<FrameTickEvent>(
        [this](const FrameTickEvent& event) {
            Logger::GetInstance().DebugFormat("[BootScene] Frame tick %.3f (frame %llu)",
                                              event.frame.deltaTime,
                                              static_cast<unsigned long long>(event.frame.frameIndex));
        },
        /*priority=*/10);
}

void BootScene::UnsubscribeFrameEvents() {
    if (!m_context || !m_context->globalEventBus || m_frameListener == 0) {
        return;
    }
    m_context->globalEventBus->Unsubscribe(m_frameListener);
    m_frameListener = 0;
}

} // namespace Render::Application


