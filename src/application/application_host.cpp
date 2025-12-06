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
#include "render/application/application_host.h"

#include <stdexcept>
#include <utility>

#include "render/async_resource_loader.h"
#include "render/ecs/world.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/uniform_manager.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Render::Application {

ApplicationHost::~ApplicationHost() {
    Shutdown();
}

bool ApplicationHost::Initialize(const Config& config) {
    if (m_initialized) {
        Logger::GetInstance().Warning("ApplicationHost::Initialize called twice");
        return true;
    }

    m_context.renderer = config.renderer;
    m_context.uniformManager = config.uniformManager;
    m_context.resourceManager = config.resourceManager;
    m_context.asyncLoader = config.asyncLoader;
    m_context.globalEventBus = &m_eventBus;

    m_world = config.world;
    m_ownsWorld = false;

    if (!m_context.IsValid()) {
        Logger::GetInstance().Error("ApplicationHost::Initialize failed: AppContext missing required dependencies");
        ResetContext();
        return false;
    }

    if (!CreateWorldIfNeeded(config.createWorldIfMissing)) {
        Logger::GetInstance().Error("ApplicationHost::Initialize failed: World is not available");
        ResetContext();
        return false;
    }
    m_context.world = m_world.get();

    if (!m_context.uniformManager) {
        Logger::GetInstance().Warning(
            "[ApplicationHost] UniformManager is null. Ensure UniformSystem registers required uniforms.");
    }

    m_moduleRegistry.Initialize(m_world.get(), &m_context);
    m_sceneManager.Initialize(&m_context, &m_moduleRegistry);

    m_initialized = true;
    return true;
}

void ApplicationHost::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // 等待所有异步加载完成，避免在模块卸载期间仍有回调
    if (m_context.asyncLoader && m_context.asyncLoader->IsInitialized()) {
        m_context.asyncLoader->WaitForAll();
    }

    m_sceneManager.Shutdown();
    m_moduleRegistry.Shutdown();
    m_eventBus.Clear();

    if (m_ownsWorld) {
        m_world.reset();
    } else {
        m_world = nullptr;
    }

    ResetContext();
    m_initialized = false;
    m_ownsWorld = false;
}

void ApplicationHost::RegisterSceneFactory(std::string sceneId, SceneFactory factory) {
    m_sceneManager.RegisterSceneFactory(std::move(sceneId), std::move(factory));
}

bool ApplicationHost::PushScene(std::string_view sceneId, SceneEnterArgs args) {
    return m_sceneManager.PushScene(sceneId, std::move(args));
}

bool ApplicationHost::ReplaceScene(std::string_view sceneId, SceneEnterArgs args) {
    return m_sceneManager.ReplaceScene(sceneId, std::move(args));
}

void ApplicationHost::UpdateFrame(const FrameUpdateArgs& args) {
    if (!m_initialized) {
        Logger::GetInstance().Warning("ApplicationHost::UpdateFrame called before Initialize");
        return;
    }

    Logger::GetInstance().InfoFormat(
        "[ApplicationHost] UpdateFrame begin frame=%llu dt=%.4f abs=%.2f",
        static_cast<unsigned long long>(args.frameIndex),
        args.deltaTime,
        args.absoluteTime);

    m_context.lastFrame = args;

    Logger::GetInstance().Info("[ApplicationHost] PreFrame modules");
    m_moduleRegistry.InvokePhase(ModulePhase::PreFrame, args);

    Logger::GetInstance().Info("[ApplicationHost] SceneManager.Update");
    m_sceneManager.Update(args);

    Logger::GetInstance().Info("[ApplicationHost] PostFrame modules");
    m_moduleRegistry.InvokePhase(ModulePhase::PostFrame, args);

    Logger::GetInstance().Info("[ApplicationHost] UpdateFrame end");
}

void ApplicationHost::UpdateWorld(float deltaTime) {
    if (!m_initialized || !m_world) {
        return;
    }
    m_world->Update(deltaTime);
}

bool ApplicationHost::CreateWorldIfNeeded(bool allowCreate) {
    if (m_world) {
        if (!m_world->IsInitialized()) {
            m_world->Initialize();
        }
        m_context.world = m_world.get();
        return true;
    }

    // If caller provided external world in config
    if (!allowCreate) {
        return false;
    }

    try {
        m_world = std::make_shared<ECS::World>();
        m_world->Initialize();
        m_ownsWorld = true;
        m_context.world = m_world.get();
        return true;
    } catch (const std::exception& ex) {
        Logger::GetInstance().Error(std::string("ApplicationHost::CreateWorldIfNeeded failed: ") + ex.what());
        m_world.reset();
        return false;
    }
}

void ApplicationHost::ResetContext() {
    m_context = AppContext{};
    m_context.globalEventBus = nullptr;
}

} // namespace Render::Application


