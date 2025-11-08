#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/ecs/entity.h"
#include "render/ecs/components.h"
#include "render/logger.h"

namespace Render::ECS {

/**
 * @brief Sprite 动画调试器
 *
 * 为调试面板提供运行时状态快照、事件日志与远程命令通道。
 * 默认仅在 Debug 构建中启用，可通过 WatchEntity() 指定观察对象。
 */
class SpriteAnimationDebugger {
public:
    enum class EventOrigin {
        StateMachine,
        Script,
        DebugCommand
    };

    struct EventLogEntry {
        SpriteAnimationEvent event;
        EventOrigin origin = EventOrigin::StateMachine;
        std::string note;
        uint64_t sequence = 0;
    };

    enum class CommandType {
        SetBool,
        SetFloat,
        Trigger,
        ResetTrigger,
        ForceState,
        QueueEvent,
        ClearEvents
    };

    struct Command {
        CommandType type = CommandType::SetBool;
        std::string parameter;
        float floatValue = 0.0f;
        bool boolValue = false;
        SpriteAnimationEvent eventData{};
    };

    static SpriteAnimationDebugger& GetInstance();

    void SetEnabled(bool enabled);
    [[nodiscard]] bool IsEnabled() const;
    void WatchAllEntities(bool enable);
    [[nodiscard]] bool IsWatchingAll() const;

    void WatchEntity(EntityID entity);
    void UnwatchEntity(EntityID entity);
    [[nodiscard]] bool IsEntityWatched(EntityID entity) const;
    [[nodiscard]] std::vector<EntityID> GetWatchedEntities() const;

    void QueueCommand(EntityID entity, const Command& command);
    void ApplyPendingCommands(EntityID entity, SpriteAnimationComponent& animComp);

    void CaptureSnapshot(EntityID entity,
                         const SpriteAnimationComponent& animComp,
                         const SpriteRenderComponent& spriteComp);
    void AppendEvents(EntityID entity,
                      const std::vector<SpriteAnimationEvent>& events,
                      EventOrigin origin = EventOrigin::StateMachine,
                      const std::string& note = std::string());
    void RecordScriptInvocation(EntityID entity,
                                const std::string& scriptName,
                                const SpriteAnimationEvent& event);

    [[nodiscard]] bool GetSnapshot(EntityID entity,
                                   SpriteAnimationStateMachineDebug& outState,
                                   SpriteRenderComponent& outSprite,
                                   std::vector<EventLogEntry>& outEvents) const;

    void DumpToLogger(EntityID entity) const;

private:
    struct DebugData {
        SpriteAnimationStateMachineDebug stateMachine;
        SpriteRenderComponent spriteSnapshot;
        std::deque<EventLogEntry> eventLog;
        std::vector<Command> pendingCommands;
    };

    SpriteAnimationDebugger() = default;

    static uint64_t ToKey(EntityID entity);

    DebugData& GetOrCreateDataLocked(uint64_t key);
    [[nodiscard]] bool ShouldCaptureLocked(uint64_t key) const;

    mutable std::mutex m_mutex;
    bool m_enabled = true;
    bool m_watchAll = false;
    std::unordered_map<uint64_t, bool> m_watched;
    std::unordered_map<uint64_t, DebugData> m_data;
    uint64_t m_eventSequence = 0;
    static constexpr size_t kMaxEventLog = 128;
};

} // namespace Render::ECS


