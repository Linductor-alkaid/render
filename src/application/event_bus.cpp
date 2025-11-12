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


