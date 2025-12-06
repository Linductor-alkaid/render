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
#include "render/application/scene_types.h"

#include <algorithm>

namespace Render::Application {

namespace {
bool ContainsResource(const std::vector<ResourceRequest>& list, const ResourceRequest& request) {
    return std::any_of(list.begin(), list.end(), [&](const ResourceRequest& existing) {
        return existing.identifier == request.identifier && existing.type == request.type;
    });
}
} // namespace

void SceneResourceManifest::Merge(const SceneResourceManifest& other) {
    for (const auto& request : other.required) {
        if (!ContainsResource(required, request)) {
            required.push_back(request);
        }
    }

    for (const auto& request : other.optional) {
        if (ContainsResource(required, request) || ContainsResource(optional, request)) {
            continue;
        }
        optional.push_back(request);
    }
}

} // namespace Render::Application


