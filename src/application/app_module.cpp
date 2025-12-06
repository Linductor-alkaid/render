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
#include "render/application/app_module.h"

#include "render/ecs/world.h"

namespace Render::Application {

int AppModule::Priority(ModulePhase) const {
    return 0;
}

void AppModule::OnUnregister(ECS::World&, AppContext&) {}

void AppModule::OnPreFrame(const FrameUpdateArgs&, AppContext&) {}

void AppModule::OnPostFrame(const FrameUpdateArgs&, AppContext&) {}

} // namespace Render::Application


