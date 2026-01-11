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

#include <memory>
#include <string>

#include "render/application/application_host.h"

namespace Render {
namespace Application {
struct FrameUpdateArgs;
}

namespace Editor {

class EditorContext;
class EditorState;

/**
 * @brief 编辑器应用主类
 * 
 * 封装ApplicationHost,管理编辑器生命周期,集成所有编辑器组件。
 */
class EditorApplication {
public:
    /**
     * @brief 编辑器应用配置
     */
    struct Config {
        Application::ApplicationHost::Config appConfig;  // 引擎配置
        std::string projectPath;                        // 项目路径
        std::string configPath = "editor_config.json";  // 配置文件路径
    };

    EditorApplication();
    ~EditorApplication();

    EditorApplication(const EditorApplication&) = delete;
    EditorApplication& operator=(const EditorApplication&) = delete;

    /**
     * @brief 初始化编辑器应用
     * @param config 配置
     * @return 是否成功初始化
     */
    bool Initialize(const Config& config);

    /**
     * @brief 关闭编辑器应用
     */
    void Shutdown();

    /**
     * @brief 更新编辑器(应在主循环中每帧调用)
     * @param args 帧更新参数
     */
    void Update(const Application::FrameUpdateArgs& args);

    /**
     * @brief 获取编辑器上下文
     */
    [[nodiscard]] EditorContext& GetContext() const;

    /**
     * @brief 获取编辑器状态
     */
    [[nodiscard]] EditorState& GetState() const;

    /**
     * @brief 获取ApplicationHost
     */
    [[nodiscard]] Application::ApplicationHost& GetApplicationHost() const;

    /**
     * @brief 检查是否已初始化
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
    bool m_initialized = false;
    std::unique_ptr<Application::ApplicationHost> m_appHost;
    std::unique_ptr<EditorState> m_editorState;
    std::unique_ptr<EditorContext> m_editorContext;
    Config m_config{};
};

} // namespace Editor
} // namespace Render
