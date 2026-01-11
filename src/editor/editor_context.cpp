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
#include "render/editor/editor_context.h"

#include <nlohmann/json.hpp>

#include "render/application/application_host.h"
#include "render/editor/editor_state.h"
#include "render/json_serializer.h"
#include "render/logger.h"
#include "render/resource_manager.h"

namespace Render::Editor {

void EditorContext::Initialize(Application::ApplicationHost* appHost, EditorState* editorState) {
    if (m_initialized) {
        Logger::GetInstance().Warning("[EditorContext] Initialize called twice");
        return;
    }

    if (!appHost) {
        Logger::GetInstance().Error("[EditorContext] Initialize failed: appHost is null");
        return;
    }

    if (!editorState) {
        Logger::GetInstance().Error("[EditorContext] Initialize failed: editorState is null");
        return;
    }

    m_appHost = appHost;
    m_editorState = editorState;
    m_initialized = true;

    Logger::GetInstance().Info("[EditorContext] Initialized");
}

void EditorContext::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_appHost = nullptr;
    m_editorState = nullptr;
    m_initialized = false;

    Logger::GetInstance().Info("[EditorContext] Shutdown");
}

ECS::World& EditorContext::GetWorld() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetWorld called before Initialize");
    }
    return m_appHost->GetWorld();
}

ResourceManager& EditorContext::GetResourceManager() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetResourceManager called before Initialize");
    }
    auto& ctx = m_appHost->GetContext();
    if (!ctx.resourceManager) {
        throw std::runtime_error("[EditorContext] ResourceManager is null");
    }
    return *ctx.resourceManager;
}

Application::SceneManager& EditorContext::GetSceneManager() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetSceneManager called before Initialize");
    }
    return m_appHost->GetSceneManager();
}

Application::ModuleRegistry& EditorContext::GetModuleRegistry() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetModuleRegistry called before Initialize");
    }
    return m_appHost->GetModuleRegistry();
}

Renderer& EditorContext::GetRenderer() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetRenderer called before Initialize");
    }
    auto& ctx = m_appHost->GetContext();
    if (!ctx.renderer) {
        throw std::runtime_error("[EditorContext] Renderer is null");
    }
    return *ctx.renderer;
}

Application::ApplicationHost& EditorContext::GetApplicationHost() const {
    if (!m_appHost) {
        throw std::runtime_error("[EditorContext] GetApplicationHost called before Initialize");
    }
    return *m_appHost;
}

EditorState& EditorContext::GetState() const {
    if (!m_editorState) {
        throw std::runtime_error("[EditorContext] GetState called before Initialize");
    }
    return *m_editorState;
}

bool EditorContext::LoadConfig(const std::string& path) {
    nlohmann::json j;
    if (!JsonSerializer::LoadFromFile(path, j)) {
        Logger::GetInstance().WarningFormat(
            "[EditorContext] Failed to load config from: %s, using defaults",
            path.c_str());
        return false;
    }

    try {
        // 加载配置字段
        if (j.contains("projectPath")) {
            m_config.projectPath = j["projectPath"].get<std::string>();
        }
        if (j.contains("lastScenePath")) {
            m_config.lastScenePath = j["lastScenePath"].get<std::string>();
        }
        if (j.contains("autoSave")) {
            m_config.autoSave = j["autoSave"].get<bool>();
        }
        if (j.contains("autoSaveInterval")) {
            m_config.autoSaveInterval = j["autoSaveInterval"].get<int>();
        }
        if (j.contains("darkTheme")) {
            m_config.darkTheme = j["darkTheme"].get<bool>();
        }
        if (j.contains("windowWidth")) {
            m_config.windowWidth = j["windowWidth"].get<int>();
        }
        if (j.contains("windowHeight")) {
            m_config.windowHeight = j["windowHeight"].get<int>();
        }
        if (j.contains("showGrid")) {
            m_config.showGrid = j["showGrid"].get<bool>();
        }
        if (j.contains("showAxis")) {
            m_config.showAxis = j["showAxis"].get<bool>();
        }
        if (j.contains("gridSize")) {
            m_config.gridSize = j["gridSize"].get<float>();
        }

        Logger::GetInstance().InfoFormat("[EditorContext] Config loaded from: %s", path.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[EditorContext] Failed to parse config: %s",
            e.what());
        return false;
    }
}

bool EditorContext::SaveConfig(const std::string& path) const {
    try {
        nlohmann::json j;
        j["projectPath"] = m_config.projectPath;
        j["lastScenePath"] = m_config.lastScenePath;
        j["autoSave"] = m_config.autoSave;
        j["autoSaveInterval"] = m_config.autoSaveInterval;
        j["darkTheme"] = m_config.darkTheme;
        j["windowWidth"] = m_config.windowWidth;
        j["windowHeight"] = m_config.windowHeight;
        j["showGrid"] = m_config.showGrid;
        j["showAxis"] = m_config.showAxis;
        j["gridSize"] = m_config.gridSize;

        if (!JsonSerializer::SaveToFile(j, path)) {
            Logger::GetInstance().ErrorFormat(
                "[EditorContext] Failed to save config to: %s",
                path.c_str());
            return false;
        }

        Logger::GetInstance().InfoFormat("[EditorContext] Config saved to: %s", path.c_str());
        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat(
            "[EditorContext] Failed to save config: %s",
            e.what());
        return false;
    }
}

} // namespace Render::Editor
