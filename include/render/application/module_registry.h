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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <optional>

#include "render/application/app_context.h"
#include "render/application/app_module.h"

namespace Render {

namespace ECS {
class World;
}

namespace Application {

class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();

    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    void Initialize(ECS::World* world, AppContext* ctx);
    void Shutdown();

    bool RegisterModule(std::unique_ptr<AppModule> module, bool activateImmediately = true);
    void UnregisterModule(std::string_view name);

    bool ActivateModule(std::string_view name);
    void DeactivateModule(std::string_view name);

    void ForEachModule(const std::function<void(const AppModule&)>& visitor) const;
    AppModule* GetModule(std::string_view name);
    const AppModule* GetModule(std::string_view name) const;

    void InvokePhase(ModulePhase phase, const FrameUpdateArgs& frameArgs);

    // ========================================================================
    // 工具链集成接口（Phase 2.5）
    // ========================================================================

    /**
     * @brief 模块状态信息
     */
    struct ModuleState {
        std::string name;
        bool active = false;
        bool registered = false;
        ModuleDependencies dependencies;
        int preFramePriority = 0;
        int postFramePriority = 0;
    };

    /**
     * @brief 获取模块状态
     * @param name 模块名称
     * @return 模块状态，如果模块不存在返回 std::nullopt
     */
    std::optional<ModuleState> GetModuleState(std::string_view name) const;

    /**
     * @brief 获取所有模块状态列表
     * @return 模块状态列表（按名称排序）
     */
    std::vector<ModuleState> GetAllModuleStates() const;

    /**
     * @brief 检查模块是否激活
     */
    bool IsModuleActive(std::string_view name) const;

    /**
     * @brief 检查模块是否已注册
     */
    bool IsModuleRegistered(std::string_view name) const;

private:
    struct ModuleRecord {
        std::unique_ptr<AppModule> module;
        bool active = false;
        bool registered = false;
    };

    bool CanRegister(const AppModule& module) const;
    bool ResolveDependencies(const AppModule& module, std::vector<std::string>& missing) const;
    void SortForPhase(ModulePhase phase);

    ECS::World* m_world = nullptr;
    AppContext* m_context = nullptr;

    std::unordered_map<std::string, ModuleRecord> m_modules;
    std::vector<AppModule*> m_sortedPreFrame;
    std::vector<AppModule*> m_sortedPostFrame;
};

} // namespace Application
} // namespace Render


