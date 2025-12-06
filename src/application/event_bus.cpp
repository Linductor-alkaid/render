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
#include "render/application/event_bus.h"

#include <algorithm>

namespace Render::Application {

EventBus::EventBus()
    : m_nextId(1) {}

void EventBus::Unsubscribe(ListenerId id) {
    std::scoped_lock lock(m_mutex);
    m_listeners.erase(
        std::remove_if(m_listeners.begin(), m_listeners.end(),
                       [&](const ListenerRecord& record) { return record.id == id; }),
        m_listeners.end());
}

void EventBus::Clear() {
    std::scoped_lock lock(m_mutex);
    m_listeners.clear();
}

} // namespace Render::Application


