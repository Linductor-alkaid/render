#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Render::Application {

struct EventBase {
    virtual ~EventBase() = default;
    
    // 事件标签，用于过滤
    std::unordered_set<std::string> tags;
    
    // 目标场景ID，如果为空则适用于所有场景
    std::string targetSceneId;
    
    // 添加标签
    void AddTag(std::string_view tag) {
        tags.insert(std::string(tag));
    }
    
    // 检查是否有标签
    bool HasTag(std::string_view tag) const {
        return tags.find(std::string(tag)) != tags.end();
    }
};

// 事件过滤器接口
class EventFilter {
public:
    virtual ~EventFilter() = default;
    virtual bool ShouldReceive(const EventBase& event) const = 0;
};

// 默认过滤器：接收所有事件
class DefaultEventFilter : public EventFilter {
public:
    bool ShouldReceive(const EventBase&) const override { return true; }
};

// 标签过滤器：只接收包含指定标签的事件
class TagEventFilter : public EventFilter {
public:
    explicit TagEventFilter(std::string_view tag) : m_tag(tag) {}
    bool ShouldReceive(const EventBase& event) const override {
        return event.HasTag(m_tag);
    }
private:
    std::string m_tag;
};

// 场景过滤器：只接收目标场景的事件
class SceneEventFilter : public EventFilter {
public:
    explicit SceneEventFilter(std::string_view sceneId) : m_sceneId(sceneId) {}
    bool ShouldReceive(const EventBase& event) const override {
        return event.targetSceneId.empty() || event.targetSceneId == m_sceneId;
    }
private:
    std::string m_sceneId;
};

// 组合过滤器：多个过滤器组合（AND逻辑）
class CompositeEventFilter : public EventFilter {
public:
    void AddFilter(std::shared_ptr<EventFilter> filter) {
        m_filters.push_back(filter);
    }
    bool ShouldReceive(const EventBase& event) const override {
        for (const auto& filter : m_filters) {
            if (!filter->ShouldReceive(event)) {
                return false;
            }
        }
        return true;
    }
private:
    std::vector<std::shared_ptr<EventFilter>> m_filters;
};

class EventBus {
public:
    using ListenerId = uint64_t;

    EventBus();

    // 订阅事件，支持过滤器
    template <typename EventT>
    ListenerId Subscribe(std::function<void(const EventT&)> callback, 
                        int priority = 0,
                        std::shared_ptr<EventFilter> filter = nullptr);

    void Unsubscribe(ListenerId id);

    template <typename EventT>
    void Publish(const EventT& event, std::string_view currentSceneId = "");

    void Clear();

private:
    struct ListenerRecord {
        ListenerRecord(ListenerId listenerId,
                       std::type_index eventType,
                       int listenerPriority,
                       std::function<void(const EventBase&)> cb,
                       std::shared_ptr<EventFilter> eventFilter)
            : id(listenerId)
            , type(eventType)
            , priority(listenerPriority)
            , callback(std::move(cb))
            , filter(std::move(eventFilter)) {}

        ListenerId id;
        std::type_index type;
        int priority;
        std::function<void(const EventBase&)> callback;
        std::shared_ptr<EventFilter> filter;
    };

    std::atomic<ListenerId> m_nextId;
    mutable std::mutex m_mutex;
    std::vector<ListenerRecord> m_listeners;
};

template <typename EventT>
EventBus::ListenerId EventBus::Subscribe(std::function<void(const EventT&)> callback, 
                                         int priority,
                                         std::shared_ptr<EventFilter> filter) {
    static_assert(std::is_base_of_v<EventBase, EventT>, "EventT must derive from EventBase");

    ListenerId id = m_nextId.fetch_add(1, std::memory_order_relaxed);
    
    // 如果没有提供过滤器，使用默认过滤器
    if (!filter) {
        filter = std::make_shared<DefaultEventFilter>();
    }
    
    ListenerRecord record{
        id,
        std::type_index(typeid(EventT)),
        priority,
        [func = std::move(callback)](const EventBase& base) {
            func(static_cast<const EventT&>(base));
        },
        std::move(filter)
    };

    std::scoped_lock lock(m_mutex);
    auto insertPos = std::lower_bound(
        m_listeners.begin(), m_listeners.end(), record,
        [](const ListenerRecord& lhs, const ListenerRecord& rhs) { return lhs.priority > rhs.priority; });
    m_listeners.insert(insertPos, std::move(record));
    return id;
}

template <typename EventT>
void EventBus::Publish(const EventT& event, std::string_view currentSceneId) {
    static_assert(std::is_base_of_v<EventBase, EventT>, "EventT must derive from EventBase");

    std::vector<ListenerRecord> listenersCopy;
    {
        std::scoped_lock lock(m_mutex);
        listenersCopy = m_listeners;
    }

    // 创建事件副本用于设置目标场景（如果需要）
    EventT eventCopy = event;
    if (eventCopy.targetSceneId.empty() && !currentSceneId.empty()) {
        eventCopy.targetSceneId = std::string(currentSceneId);
    }

    for (const auto& listener : listenersCopy) {
        if (listener.type == std::type_index(typeid(EventT))) {
            // 应用过滤器
            if (listener.filter && !listener.filter->ShouldReceive(eventCopy)) {
                continue;
            }
            listener.callback(eventCopy);
        }
    }
}

} // namespace Render::Application


