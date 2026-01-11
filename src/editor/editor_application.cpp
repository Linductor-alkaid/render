/*
 * Copyright (c) 2026 Li Chaoyu
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
#include "render/editor/editor_application.h"

#include <stdexcept>

#include "render/application/application_host.h"
#include "render/editor/editor_context.h"
#include "render/editor/editor_state.h"
#include "render/logger.h"

namespace Render::Editor {

EditorApplication::EditorApplication() = default;

EditorApplication::~EditorApplication() {
    Shutdown();
}

bool EditorApplication::Initialize(const Config& config) {
    if (m_initialized) {
        Logger::GetInstance().Warning("[EditorApplication] Initialize called twice");
        return true;
    }

    m_config = config;

    // 创建ApplicationHost
    m_appHost = std::make_unique<Application::ApplicationHost>();

    // 初始化ApplicationHost
    if (!m_appHost->Initialize(config.appConfig)) {
        Logger::GetInstance().Error("[EditorApplication] Failed to initialize ApplicationHost");
        m_appHost.reset();
        return false;
    }

    // 创建EditorState
    m_editorState = std::make_unique<EditorState>();

    // 创建EditorContext
    m_editorContext = std::make_unique<EditorContext>();
    m_editorContext->Initialize(m_appHost.get(), m_editorState.get());

    // 加载编辑器配置
    if (!m_config.configPath.empty()) {
        m_editorContext->LoadConfig(m_config.configPath);
    }

    // 设置项目路径
    if (!m_config.projectPath.empty()) {
        m_editorContext->GetConfig().projectPath = m_config.projectPath;
    }

    m_initialized = true;
    Logger::GetInstance().Info("[EditorApplication] Initialized successfully");

    return true;
}

void EditorApplication::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::GetInstance().Info("[EditorApplication] Shutting down...");

    // 保存编辑器配置
    if (m_editorContext && !m_config.configPath.empty()) {
        m_editorContext->SaveConfig(m_config.configPath);
    }

    // 关闭EditorContext
    if (m_editorContext) {
        m_editorContext->Shutdown();
        m_editorContext.reset();
    }

    // 关闭EditorState(不需要特殊清理)
    m_editorState.reset();

    // 关闭ApplicationHost
    if (m_appHost) {
        m_appHost->Shutdown();
        m_appHost.reset();
    }

    m_initialized = false;
    Logger::GetInstance().Info("[EditorApplication] Shutdown complete");
}

void EditorApplication::Update(const Application::FrameUpdateArgs& args) {
    if (!m_initialized || !m_appHost) {
        Logger::GetInstance().Warning("[EditorApplication] Update called before Initialize");
        return;
    }

    // 更新ApplicationHost
    m_appHost->UpdateFrame(args);
    m_appHost->UpdateWorld(args.deltaTime);
}

EditorContext& EditorApplication::GetContext() const {
    if (!m_editorContext) {
        throw std::runtime_error("[EditorApplication] GetContext called before Initialize");
    }
    return *m_editorContext;
}

EditorState& EditorApplication::GetState() const {
    if (!m_editorState) {
        throw std::runtime_error("[EditorApplication] GetState called before Initialize");
    }
    return *m_editorState;
}

Application::ApplicationHost& EditorApplication::GetApplicationHost() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorApplication] GetApplicationHost called before Initialize");
    }
    return *m_appHost;
}

} // namespace Render::Editor
