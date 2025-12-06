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
#include "render/application/app_context.h"

#include <stdexcept>

#include "render/async_resource_loader.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/uniform_manager.h"

namespace Render::Application {

bool AppContext::IsValid() const noexcept {
    return renderer != nullptr && resourceManager != nullptr && asyncLoader != nullptr;
}

void AppContext::ValidateOrThrow(const std::string& source) const {
    if (IsValid()) {
        return;
    }

    std::string message = "AppContext validation failed in " + source +
                          ": renderer/resourceManager/asyncLoader must be initialized.";
    Logger::GetInstance().Error(message);
    throw std::runtime_error(message);
}

} // namespace Render::Application

