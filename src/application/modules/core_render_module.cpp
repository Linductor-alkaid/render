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
#include "render/application/modules/core_render_module.h"

#include "render/application/app_context.h"
#include "render/application/event_bus.h"
#include "render/application/events/frame_events.h"
#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/ecs/systems.h"
#include "render/ecs/components.h"
#include "render/logger.h"

namespace Render::Application {

using namespace Render::ECS;

int CoreRenderModule::Priority(ModulePhase phase) const {
    switch (phase) {
        case ModulePhase::PreFrame:
            return 100;
        case ModulePhase::PostFrame:
            return 0;
        case ModulePhase::Register:
        default:
            return 0;
    }
}

void CoreRenderModule::OnRegister(ECS::World& world, AppContext& ctx) {
    if (!world.IsInitialized()) {
        world.Initialize();
    }
    if (!ctx.uniformManager || !ctx.renderer) {
        Logger::GetInstance().Warning("[CoreRenderModule] Missing renderer/uniform manager.");
    }
    
    // 注册核心组件和系统
    RegisterCoreComponents(world, ctx);
    RegisterCoreSystems(world, ctx);
    
    Logger::GetInstance().Info("[CoreRenderModule] Registered");
    m_registered = true;
    m_systemsRegistered = true;
    m_loggedAsyncLoaderMissing = false;
}

void CoreRenderModule::OnUnregister(ECS::World&, AppContext&) {
    Logger::GetInstance().Info("[CoreRenderModule] Unregistered");
    m_registered = false;
    m_systemsRegistered = false;
    m_loggedAsyncLoaderMissing = false;
}

void CoreRenderModule::RegisterCoreComponents(ECS::World& world, AppContext&) {
    // 注册核心ECS组件
    world.RegisterComponent<Render::ECS::TransformComponent>();
    world.RegisterComponent<Render::ECS::MeshRenderComponent>();
    world.RegisterComponent<Render::ECS::ModelComponent>();
    world.RegisterComponent<Render::ECS::SpriteRenderComponent>();
    world.RegisterComponent<Render::ECS::CameraComponent>();
    world.RegisterComponent<Render::ECS::LightComponent>();
    world.RegisterComponent<Render::ECS::GeometryComponent>();
    
    Logger::GetInstance().Info("[CoreRenderModule] Core components registered");
}

void CoreRenderModule::RegisterCoreSystems(ECS::World& world, AppContext& ctx) {
    if (!ctx.renderer) {
        Logger::GetInstance().Warning("[CoreRenderModule] Cannot register systems: renderer is null");
        return;
    }

    // 注册核心渲染系统（按优先级顺序）
    // 1. WindowSystem - 窗口管理（优先级3）
    world.RegisterSystem<Render::ECS::WindowSystem>(ctx.renderer);
    
    // 2. CameraSystem - 相机管理（优先级5）
    world.RegisterSystem<Render::ECS::CameraSystem>();
    
    // 3. TransformSystem - 变换更新（优先级10）
    world.RegisterSystem<Render::ECS::TransformSystem>();
    
    // 4. GeometrySystem - 几何生成（优先级15）
    world.RegisterSystem<Render::ECS::GeometrySystem>();
    
    // 5. ResourceLoadingSystem - 资源加载（优先级20）
    if (ctx.asyncLoader) {
        world.RegisterSystem<Render::ECS::ResourceLoadingSystem>(ctx.asyncLoader);
    } else {
        world.RegisterSystem<Render::ECS::ResourceLoadingSystem>();
        Logger::GetInstance().Warning("[CoreRenderModule] ResourceLoadingSystem registered without AsyncResourceLoader");
    }
    
    // 6. LightSystem - 光照管理（优先级50）
    world.RegisterSystem<Render::ECS::LightSystem>(ctx.renderer);
    
    // 7. UniformSystem - Uniform管理（优先级90）
    world.RegisterSystem<Render::ECS::UniformSystem>(ctx.renderer);
    
    // 8. MeshRenderSystem - 网格渲染（优先级100）
    world.RegisterSystem<Render::ECS::MeshRenderSystem>(ctx.renderer);
    
    // 9. ModelRenderSystem - 模型渲染（优先级105）
    world.RegisterSystem<Render::ECS::ModelRenderSystem>(ctx.renderer);
    
    // 10. SpriteAnimationSystem - 精灵动画（优先级180）
    world.RegisterSystem<Render::ECS::SpriteAnimationSystem>();
    
    // 11. SpriteRenderSystem - 精灵渲染（优先级200）
    world.RegisterSystem<Render::ECS::SpriteRenderSystem>(ctx.renderer);
    
    // 12. ResourceCleanupSystem - 资源清理（优先级1000）
    world.RegisterSystem<Render::ECS::ResourceCleanupSystem>();
    
    Logger::GetInstance().Info("[CoreRenderModule] Core rendering systems registered");
}

void CoreRenderModule::OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }
    Logger::GetInstance().DebugFormat(
        "[CoreRenderModule] OnPreFrame frame=%llu dt=%.4f abs=%.2f",
        static_cast<unsigned long long>(frame.frameIndex),
        frame.deltaTime,
        frame.absoluteTime);
    if (ctx.globalEventBus) {
        FrameBeginEvent beginEvent{};
        beginEvent.frame = frame;
        ctx.globalEventBus->Publish(beginEvent);

        FrameTickEvent tickEvent{};
        tickEvent.frame = frame;
        ctx.globalEventBus->Publish(tickEvent);
    }

    if (ctx.asyncLoader) {
        if (ctx.asyncLoader->IsInitialized()) {
            size_t processed = ctx.asyncLoader->ProcessCompletedTasks(m_asyncTasksPerFrame);
            if (processed > 0) {
                Logger::GetInstance().DebugFormat(
                    "[CoreRenderModule] ProcessCompletedTasks processed=%zu",
                    processed);
            }
        } else if (!m_loggedAsyncLoaderMissing) {
            Logger::GetInstance().Warning("[CoreRenderModule] AsyncResourceLoader not initialized; async tasks will be skipped.");
            m_loggedAsyncLoaderMissing = true;
        }
    } else if (!m_loggedAsyncLoaderMissing) {
        Logger::GetInstance().Warning("[CoreRenderModule] AppContext.asyncLoader is null; async tasks will be skipped.");
        m_loggedAsyncLoaderMissing = true;
    }

    ctx.lastFrame = frame;
}

void CoreRenderModule::OnPostFrame(const FrameUpdateArgs& frame, AppContext& ctx) {
    if (!m_registered) {
        return;
    }

    if (ctx.globalEventBus) {
        FrameEndEvent endEvent{};
        endEvent.frame = frame;
        ctx.globalEventBus->Publish(endEvent);
    }
}

} // namespace Render::Application


