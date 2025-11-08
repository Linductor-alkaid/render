#include "render/debug/sprite_animation_debugger.h"

#if defined(DEBUG) || defined(_DEBUG)

#include <algorithm>

namespace Render::ECS {

SpriteAnimationDebugger& SpriteAnimationDebugger::GetInstance() {
    static SpriteAnimationDebugger instance;
    return instance;
}

void SpriteAnimationDebugger::SetEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enabled;
    if (!m_enabled) {
        m_data.clear();
        m_watched.clear();
        m_watchAll = false;
    }
}

bool SpriteAnimationDebugger::IsEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

void SpriteAnimationDebugger::WatchAllEntities(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchAll = enable;
    if (!m_watchAll) {
        m_watched.clear();
        m_data.clear();
    }
}

bool SpriteAnimationDebugger::IsWatchingAll() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_watchAll;
}

void SpriteAnimationDebugger::WatchEntity(EntityID entity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !entity.IsValid()) {
        return;
    }
    uint64_t key = ToKey(entity);
    m_watched[key] = true;
    GetOrCreateDataLocked(key);
}

void SpriteAnimationDebugger::UnwatchEntity(EntityID entity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!entity.IsValid()) {
        return;
    }
    uint64_t key = ToKey(entity);
    m_watched.erase(key);
    m_data.erase(key);
}

bool SpriteAnimationDebugger::IsEntityWatched(EntityID entity) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!entity.IsValid()) {
        return false;
    }
    uint64_t key = ToKey(entity);
    if (m_watchAll) {
        return true;
    }
    return m_watched.find(key) != m_watched.end();
}

std::vector<EntityID> SpriteAnimationDebugger::GetWatchedEntities() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<EntityID> result;
    result.reserve(m_watched.size());
    for (const auto& [key, _] : m_watched) {
        EntityID entity;
        entity.index = static_cast<uint32_t>(key & 0xFFFFFFFFull);
        entity.version = static_cast<uint32_t>((key >> 32) & 0xFFFFFFFFull);
        result.push_back(entity);
    }
    return result;
}

void SpriteAnimationDebugger::QueueCommand(EntityID entity, const Command& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !entity.IsValid()) {
        return;
    }
    uint64_t key = ToKey(entity);
    auto& data = GetOrCreateDataLocked(key);
    data.pendingCommands.push_back(command);
}

void SpriteAnimationDebugger::ApplyPendingCommands(EntityID entity, SpriteAnimationComponent& animComp) {
    std::vector<Command> commands;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_enabled || !entity.IsValid()) {
            return;
        }
        uint64_t key = ToKey(entity);
        if (!ShouldCaptureLocked(key)) {
            return;
        }
        auto it = m_data.find(key);
        if (it == m_data.end()) {
            return;
        }
        commands.swap(it->second.pendingCommands);
    }

    for (const auto& cmd : commands) {
        switch (cmd.type) {
        case CommandType::SetBool:
            animComp.SetBoolParameter(cmd.parameter, cmd.boolValue);
            animComp.dirty = true;
            break;
        case CommandType::SetFloat:
            animComp.SetFloatParameter(cmd.parameter, cmd.floatValue);
            animComp.dirty = true;
            break;
        case CommandType::Trigger:
            animComp.SetTrigger(cmd.parameter);
            break;
        case CommandType::ResetTrigger:
            animComp.ResetTrigger(cmd.parameter);
            break;
        case CommandType::ForceState:
            if (!cmd.parameter.empty()) {
                bool changed = animComp.ForceState(cmd.parameter, true);
                if (changed) {
                    animComp.dirty = true;
                }
            }
            break;
        case CommandType::QueueEvent:
            animComp.QueueDebugEvent(cmd.eventData);
            break;
        case CommandType::ClearEvents:
            animComp.debugEventQueue.clear();
            animComp.events.clear();
            break;
        }
    }
}

void SpriteAnimationDebugger::CaptureSnapshot(EntityID entity,
                                              const SpriteAnimationComponent& animComp,
                                              const SpriteRenderComponent& spriteComp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !entity.IsValid()) {
        return;
    }
    uint64_t key = ToKey(entity);
    if (!ShouldCaptureLocked(key)) {
        return;
    }
    auto& data = GetOrCreateDataLocked(key);
    data.stateMachine = animComp.GetStateMachineDebug();
    data.spriteSnapshot = spriteComp;
}

void SpriteAnimationDebugger::AppendEvents(EntityID entity,
                                           const std::vector<SpriteAnimationEvent>& events,
                                           EventOrigin origin,
                                           const std::string& note) {
    if (events.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !entity.IsValid()) {
        return;
    }
    uint64_t key = ToKey(entity);
    if (!ShouldCaptureLocked(key)) {
        return;
    }
    auto& data = GetOrCreateDataLocked(key);
    for (const auto& evt : events) {
        EventLogEntry entry;
        entry.event = evt;
        entry.origin = origin;
        entry.note = note;
        entry.sequence = ++m_eventSequence;
        data.eventLog.push_back(entry);
        if (data.eventLog.size() > kMaxEventLog) {
            data.eventLog.pop_front();
        }
    }
}

void SpriteAnimationDebugger::RecordScriptInvocation(EntityID entity,
                                                     const std::string& scriptName,
                                                     const SpriteAnimationEvent& event) {
    SpriteAnimationEvent evt = event;
    AppendEvents(entity,
                 std::vector<SpriteAnimationEvent>{evt},
                 EventOrigin::Script,
                 scriptName);
}

bool SpriteAnimationDebugger::GetSnapshot(EntityID entity,
                                          SpriteAnimationStateMachineDebug& outState,
                                          SpriteRenderComponent& outSprite,
                                          std::vector<EventLogEntry>& outEvents) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !entity.IsValid()) {
        return false;
    }
    uint64_t key = ToKey(entity);
    auto it = m_data.find(key);
    if (it == m_data.end()) {
        return false;
    }
    outState = it->second.stateMachine;
    outSprite = it->second.spriteSnapshot;
    outEvents.assign(it->second.eventLog.begin(), it->second.eventLog.end());
    return true;
}

void SpriteAnimationDebugger::DumpToLogger(EntityID entity) const {
    SpriteAnimationStateMachineDebug state;
    SpriteRenderComponent sprite;
    std::vector<EventLogEntry> events;
    if (!GetSnapshot(entity, state, sprite, events)) {
        Logger::GetInstance().InfoFormat("[SpriteAnimationDebugger] Entity %u 无调试快照", entity.index);
        return;
    }

    Logger::GetInstance().InfoFormat("[SpriteAnimationDebugger] Entity %u 状态机快照：", entity.index);
    Logger::GetInstance().InfoFormat("  当前状态: %s (默认: %s)", state.currentState.c_str(), state.defaultState.c_str());
    Logger::GetInstance().InfoFormat("  当前剪辑: %s  帧: %d  播放: %s  速度: %.3f  StateTime: %.3f",
                                     state.currentClip.c_str(),
                                     state.currentFrame,
                                     state.playing ? "Yes" : "No",
                                     state.playbackSpeed,
                                     state.stateTime);

    if (!state.boolParameters.empty()) {
        std::string line = "  Bool 参数: ";
        bool first = true;
        for (const auto& [name, value] : state.boolParameters) {
            if (!first) line += ", ";
            line += name + "=" + (value ? "true" : "false");
            first = false;
        }
        Logger::GetInstance().Info(line);
    }
    if (!state.floatParameters.empty()) {
        std::string line = "  Float 参数: ";
        bool first = true;
        for (const auto& [name, value] : state.floatParameters) {
            if (!first) line += ", ";
            line += name + "=" + std::to_string(value);
            first = false;
        }
        Logger::GetInstance().Info(line);
    }
    if (!state.activeTriggers.empty()) {
        std::string line = "  Active Triggers: ";
        bool first = true;
        for (const auto& name : state.activeTriggers) {
            if (!first) line += ", ";
            line += name;
            first = false;
        }
        Logger::GetInstance().Info(line);
    }

    Logger::GetInstance().InfoFormat("  状态数: %zu, 过渡数: %zu, 脚本绑定: %zu",
                                     state.states.size(),
                                     state.transitions.size(),
                                     state.scriptBindings.size());

    if (!events.empty()) {
        Logger::GetInstance().Info("[SpriteAnimationDebugger] 最近事件：");
        for (const auto& entry : events) {
            const auto& evt = entry.event;
            const char* originStr = entry.origin == EventOrigin::StateMachine
                                        ? "State"
                                        : (entry.origin == EventOrigin::Script ? "Script" : "Debug");
            Logger::GetInstance().InfoFormat(
                "    #%llu [%s] clip=%s frame=%d type=%d note=%s",
                static_cast<unsigned long long>(entry.sequence),
                originStr,
                evt.clip.c_str(),
                evt.frameIndex,
                static_cast<int>(evt.type),
                entry.note.c_str());
        }
    }
}

uint64_t SpriteAnimationDebugger::ToKey(EntityID entity) {
    return (static_cast<uint64_t>(entity.version) << 32) | static_cast<uint64_t>(entity.index);
}

SpriteAnimationDebugger::DebugData& SpriteAnimationDebugger::GetOrCreateDataLocked(uint64_t key) {
    return m_data[key];
}

bool SpriteAnimationDebugger::ShouldCaptureLocked(uint64_t key) const {
    return m_watchAll || m_watched.find(key) != m_watched.end();
}

} // namespace Render::ECS

#else

namespace Render::ECS {

SpriteAnimationDebugger& SpriteAnimationDebugger::GetInstance() {
    static SpriteAnimationDebugger instance;
    return instance;
}

void SpriteAnimationDebugger::SetEnabled(bool) {}
bool SpriteAnimationDebugger::IsEnabled() const { return false; }
void SpriteAnimationDebugger::WatchAllEntities(bool) {}
bool SpriteAnimationDebugger::IsWatchingAll() const { return false; }
void SpriteAnimationDebugger::WatchEntity(EntityID) {}
void SpriteAnimationDebugger::UnwatchEntity(EntityID) {}
bool SpriteAnimationDebugger::IsEntityWatched(EntityID) const { return false; }
std::vector<EntityID> SpriteAnimationDebugger::GetWatchedEntities() const { return {}; }
void SpriteAnimationDebugger::QueueCommand(EntityID, const Command&) {}
void SpriteAnimationDebugger::ApplyPendingCommands(EntityID, SpriteAnimationComponent&) {}
void SpriteAnimationDebugger::CaptureSnapshot(EntityID, const SpriteAnimationComponent&, const SpriteRenderComponent&) {}
void SpriteAnimationDebugger::AppendEvents(EntityID, const std::vector<SpriteAnimationEvent>&, EventOrigin, const std::string&) {}
void SpriteAnimationDebugger::RecordScriptInvocation(EntityID, const std::string&, const SpriteAnimationEvent&) {}
bool SpriteAnimationDebugger::GetSnapshot(EntityID, SpriteAnimationStateMachineDebug&, SpriteRenderComponent&, std::vector<EventLogEntry>&) const { return false; }
void SpriteAnimationDebugger::DumpToLogger(EntityID) const {}

uint64_t SpriteAnimationDebugger::ToKey(EntityID entity) {
    return (static_cast<uint64_t>(entity.version) << 32) | static_cast<uint64_t>(entity.index);
}

SpriteAnimationDebugger::DebugData& SpriteAnimationDebugger::GetOrCreateDataLocked(uint64_t key) {
    return m_data[key];
}

bool SpriteAnimationDebugger::ShouldCaptureLocked(uint64_t) const {
    return false;
}

} // namespace Render::ECS

#endif // DEBUG || _DEBUG


