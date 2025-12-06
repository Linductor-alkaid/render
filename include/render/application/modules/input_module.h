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

#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "render/application/app_module.h"
#include "render/application/operation_mapping.h"

union SDL_Event;

namespace Render::Application {

class InputModule final : public AppModule {
public:
    InputModule();
    ~InputModule() override;

    std::string_view Name() const override { return "InputModule"; }
    ModuleDependencies Dependencies() const override { return {"CoreRenderModule"}; }
    int Priority(ModulePhase phase) const override;

    void OnRegister(ECS::World& world, AppContext& ctx) override;
    void OnUnregister(ECS::World& world, AppContext& ctx) override;

    void OnPreFrame(const FrameUpdateArgs& frame, AppContext& ctx) override;

    bool IsKeyDown(int scancode) const;
    bool WasKeyPressed(int scancode) const;
    bool WasKeyReleased(int scancode) const;
    bool WasQuitRequested() const { return m_quitRequested; }
    
    // 操作映射管理
    void SetShortcutContext(std::string_view contextName);
    OperationMappingManager& GetOperationMapping() { return m_operationMapping; }

private:
    void ProcessSDLEvent(const SDL_Event& event);
    void BroadcastEvents(AppContext& ctx);
    void ClearTransientStates();
    
    // Blender风格操作映射
    void ProcessBlenderOperations(const SDL_Event& event, AppContext& ctx);
    KeyCombo GetCurrentKeyCombo() const;
    
    // 手势检测
    void ProcessMouseGesture(const SDL_Event& event, AppContext& ctx);
    void StartGesture(Events::GestureType type, int x, int y, uint32_t button, AppContext& ctx);
    void UpdateGesture(int x, int y, AppContext& ctx);
    void EndGesture(AppContext& ctx);

    std::unordered_set<int> m_keysDown;
    std::unordered_set<int> m_keysPressed;
    std::unordered_set<int> m_keysReleased;
    bool m_quitRequested = false;
    bool m_registered = false;
    
    // 操作映射
    OperationMappingManager m_operationMapping;
    
    // 手势状态
    struct GestureState {
        Events::GestureType type = Events::GestureType::Click;
        int startX = 0;
        int startY = 0;
        int lastX = 0;
        int lastY = 0;
        uint32_t button = 0;
        bool active = false;
    } m_gestureState;
    
    // 修饰键状态
    bool m_ctrlDown = false;
    bool m_shiftDown = false;
    bool m_altDown = false;
};

} // namespace Render::Application


