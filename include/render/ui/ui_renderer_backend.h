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
#pragma once

namespace Render::Application {
struct AppContext;
struct FrameUpdateArgs;
} // namespace Render::Application

namespace Render::UI {

class UICanvas;
class UIWidgetTree;
class UIDebugConfig;
class UIThemeManager;

/**
 * @brief UI渲染后端类型
 */
enum class UIRendererBackendType {
    Custom,  // 使用现有的UIRendererBridge
    ImGui    // 使用ImGui后端
};

/**
 * @brief UI渲染后端抽象接口
 * 
 * 定义统一的UI渲染后端接口，支持多种实现（Custom、ImGui等）
 */
class IUIRendererBackend {
public:
    virtual ~IUIRendererBackend() = default;

    /**
     * @brief 初始化后端
     * @param ctx 应用上下文
     */
    virtual void Initialize(Application::AppContext& ctx) = 0;

    /**
     * @brief 关闭并清理后端
     * @param ctx 应用上下文
     */
    virtual void Shutdown(Application::AppContext& ctx) = 0;

    /**
     * @brief 准备帧渲染（在帧开始前调用）
     * @param frame 帧更新参数
     * @param canvas UI画布
     * @param tree UI Widget树
     * @param ctx 应用上下文
     */
    virtual void PrepareFrame(const Application::FrameUpdateArgs& frame,
                             UICanvas& canvas,
                             UIWidgetTree& tree,
                             Application::AppContext& ctx) = 0;

    /**
     * @brief 刷新并提交渲染（在帧结束时调用）
     * @param frame 帧更新参数
     * @param canvas UI画布
     * @param tree UI Widget树
     * @param ctx 应用上下文
     */
    virtual void Flush(const Application::FrameUpdateArgs& frame,
                      UICanvas& canvas,
                      UIWidgetTree& tree,
                      Application::AppContext& ctx) = 0;

    /**
     * @brief 设置调试配置
     * @param config 调试配置指针（可为nullptr）
     */
    virtual void SetDebugConfig(const UIDebugConfig* config) = 0;

    /**
     * @brief 设置主题管理器
     * @param themeManager 主题管理器指针（可为nullptr）
     */
    virtual void SetThemeManager(UIThemeManager* themeManager) = 0;
};

} // namespace Render::UI

