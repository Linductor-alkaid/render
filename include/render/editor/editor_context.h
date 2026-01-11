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
#pragma once

#include <string>

namespace Render {

namespace ECS {
class World;
}

class Renderer;
class ResourceManager;

namespace Application {
class ApplicationHost;
class SceneManager;
class ModuleRegistry;
}

namespace Editor {

class EditorState;

/**
 * @brief 编辑器配置结构
 */
struct EditorConfig {
    std::string projectPath;
    std::string lastScenePath;
    bool autoSave = false;
    int autoSaveInterval = 300; // 秒

    // UI偏好设置
    bool darkTheme = true;
    int windowWidth = 1280;
    int windowHeight = 720;

    // 编辑器偏好设置
    bool showGrid = true;
    bool showAxis = true;
    float gridSize = 1.0f;
};

/**
 * @brief 编辑器上下文
 * 
 * 提供编辑器全局状态和服务访问接口。
 * 作为编辑器各模块之间的通信桥梁。
 */
class EditorContext {
public:
    EditorContext() = default;
    ~EditorContext() = default;

    EditorContext(const EditorContext&) = delete;
    EditorContext& operator=(const EditorContext&) = delete;

    /**
     * @brief 初始化编辑器上下文
     * @param appHost ApplicationHost引用
     * @param editorState EditorState引用
     */
    void Initialize(Application::ApplicationHost* appHost, EditorState* editorState);

    /**
     * @brief 关闭编辑器上下文
     */
    void Shutdown();

    // ========================================================================
    // 引擎服务访问
    // ========================================================================

    /**
     * @brief 获取ECS World
     */
    [[nodiscard]] ECS::World& GetWorld() const;

    /**
     * @brief 获取ResourceManager
     */
    [[nodiscard]] ResourceManager& GetResourceManager() const;

    /**
     * @brief 获取SceneManager
     */
    [[nodiscard]] Application::SceneManager& GetSceneManager() const;

    /**
     * @brief 获取ModuleRegistry
     */
    [[nodiscard]] Application::ModuleRegistry& GetModuleRegistry() const;

    /**
     * @brief 获取Renderer
     */
    [[nodiscard]] Renderer& GetRenderer() const;

    /**
     * @brief 获取ApplicationHost
     */
    [[nodiscard]] Application::ApplicationHost& GetApplicationHost() const;

    // ========================================================================
    // 编辑器状态访问
    // ========================================================================

    /**
     * @brief 获取EditorState
     */
    [[nodiscard]] EditorState& GetState() const;

    // ========================================================================
    // 配置管理
    // ========================================================================

    /**
     * @brief 加载配置
     * @param path 配置文件路径
     * @return 是否成功加载
     */
    bool LoadConfig(const std::string& path);

    /**
     * @brief 保存配置
     * @param path 配置文件路径
     * @return 是否成功保存
     */
    bool SaveConfig(const std::string& path) const;

    /**
     * @brief 获取配置
     */
    [[nodiscard]] const EditorConfig& GetConfig() const noexcept { return m_config; }

    /**
     * @brief 获取配置(可修改)
     */
    EditorConfig& GetConfig() noexcept { return m_config; }

    /**
     * @brief 检查是否已初始化
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
    bool m_initialized = false;
    Application::ApplicationHost* m_appHost = nullptr;
    EditorState* m_editorState = nullptr;
    EditorConfig m_config{};
};

} // namespace Editor
} // namespace Render
