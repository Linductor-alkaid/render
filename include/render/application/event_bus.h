#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Render::Application {

struct EventBase {
    virtual ~EventBase() = default;
};

class EventBus {
public:
    using ListenerId = uint64_t;

    EventBus();

    template <typename EventT>
    ListenerId Subscribe(std::function<void(const EventT&)> callback, int priority = 0);

    void Unsubscribe(ListenerId id);

    template <typename EventT>
    void Publish(const EventT& event);

    void Clear();

private:
    struct ListenerRecord {
        ListenerRecord(ListenerId listenerId,
                       std::type_index eventType,
                       int listenerPriority,
                       std::function<void(const EventBase&)> cb)
            : id(listenerId)
            , type(eventType)
            , priority(listenerPriority)
            , callback(std::move(cb)) {}

        ListenerId id;
        std::type_index type;
        int priority;
        std::function<void(const EventBase&)> callback;
    };

    std::atomic<ListenerId> m_nextId;
    mutable std::mutex m_mutex;
    std::vector<ListenerRecord> m_listeners;
};

template <typename EventT>
EventBus::ListenerId EventBus::Subscribe(std::function<void(const EventT&)> callback, int priority) {
    static_assert(std::is_base_of_v<EventBase, EventT>, "EventT must derive from EventBase");

    ListenerId id = m_nextId.fetch_add(1, std::memory_order_relaxed);
    ListenerRecord record{
        id,
        std::type_index(typeid(EventT)),
        priority,
        [func = std::move(callback)](const EventBase& base) {
            func(static_cast<const EventT&>(base));
        }};

    std::scoped_lock lock(m_mutex);
    auto insertPos = std::lower_bound(
        m_listeners.begin(), m_listeners.end(), record,
        [](const ListenerRecord& lhs, const ListenerRecord& rhs) { return lhs.priority > rhs.priority; });
    m_listeners.insert(insertPos, std::move(record));
    return id;
}

template <typename EventT>
void EventBus::Publish(const EventT& event) {
    static_assert(std::is_base_of_v<EventBase, EventT>, "EventT must derive from EventBase");

    std::vector<ListenerRecord> listenersCopy;
    {
        std::scoped_lock lock(m_mutex);
        listenersCopy = m_listeners;
    }

    for (const auto& listener : listenersCopy) {
        if (listener.type == std::type_index(typeid(EventT))) {
            listener.callback(event);
        }
    }
}

} // namespace Render::Application


