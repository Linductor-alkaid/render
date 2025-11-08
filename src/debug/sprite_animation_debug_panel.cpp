#include "render/debug/sprite_animation_debug_panel.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "render/debug/sprite_animation_debugger.h"
#include "render/logger.h"

namespace Render::ECS {

namespace {

std::string EventTypeToString(SpriteAnimationEvent::Type type) {
    switch (type) {
    case SpriteAnimationEvent::Type::ClipStarted:
        return "ClipStarted";
    case SpriteAnimationEvent::Type::ClipCompleted:
        return "ClipCompleted";
    case SpriteAnimationEvent::Type::FrameChanged:
        return "FrameChanged";
    default:
        return "Unknown";
    }
}

std::string OriginToString(SpriteAnimationDebugger::EventOrigin origin) {
    switch (origin) {
    case SpriteAnimationDebugger::EventOrigin::StateMachine:
        return "State";
    case SpriteAnimationDebugger::EventOrigin::Script:
        return "Script";
    case SpriteAnimationDebugger::EventOrigin::DebugCommand:
        return "Command";
    default:
        return "Unknown";
    }
}

} // namespace

SpriteAnimationDebugPanel::SpriteAnimationDebugPanel() = default;

void SpriteAnimationDebugPanel::SetIncludeEvents(bool include) {
    m_includeEvents = include;
}

void SpriteAnimationDebugPanel::SetIncludeParameters(bool include) {
    m_includeParameters = include;
}

void SpriteAnimationDebugPanel::SetIncludeSpriteInfo(bool include) {
    m_includeSpriteInfo = include;
}

void SpriteAnimationDebugPanel::SetMaxEventsPerEntity(size_t count) {
    m_maxEventsPerEntity = std::max<size_t>(1, count);
}

void SpriteAnimationDebugPanel::WatchEntity(EntityID entity) const {
    SpriteAnimationDebugger::GetInstance().WatchEntity(entity);
}

void SpriteAnimationDebugPanel::WatchAllEntities(bool enable) const {
    SpriteAnimationDebugger::GetInstance().WatchAllEntities(enable);
}

void SpriteAnimationDebugPanel::UnwatchEntity(EntityID entity) const {
    SpriteAnimationDebugger::GetInstance().UnwatchEntity(entity);
}

std::vector<std::string> SpriteAnimationDebugPanel::BuildPanelLines() const {
    std::vector<std::string> lines;
    lines.emplace_back("=== Sprite Animation Debug Panel ===");

    auto& debugger = SpriteAnimationDebugger::GetInstance();
    if (!debugger.IsEnabled()) {
        lines.emplace_back("SpriteAnimationDebugger is disabled.");
        return lines;
    }

    auto watchedEntities = debugger.GetWatchedEntities();
    if (watchedEntities.empty()) {
        lines.emplace_back("No watched entities. Use WatchEntity() or WatchAllEntities(true).");
        return lines;
    }

    lines.emplace_back("Entity | State | Clip | Frame | Playing | Speed | StateTime | Layer | Sort | ScreenSpace");

    for (const auto& entity : watchedEntities) {
        SpriteAnimationStateMachineDebug stateDebug;
        SpriteRenderComponent spriteSnapshot;
        std::vector<SpriteAnimationDebugger::EventLogEntry> events;

        if (!debugger.GetSnapshot(entity, stateDebug, spriteSnapshot, events)) {
            continue;
        }

        std::ostringstream oss;
        oss << "E(" << entity.index << ":" << entity.version << ") | "
            << stateDebug.currentState << " | "
            << stateDebug.currentClip << " | "
            << stateDebug.currentFrame << " | "
            << (stateDebug.playing ? "Yes" : "No") << " | "
            << std::fixed << std::setprecision(2) << stateDebug.playbackSpeed << " | "
            << std::fixed << std::setprecision(3) << stateDebug.stateTime;

        if (m_includeSpriteInfo) {
            oss << " | " << spriteSnapshot.layerID
                << " | " << spriteSnapshot.sortOrder
                << " | " << (spriteSnapshot.screenSpace ? "Screen" : "World");
        }

        lines.push_back(oss.str());

        if (m_includeParameters) {
            if (!stateDebug.boolParameters.empty()) {
                std::ostringstream boolLine;
                boolLine << "  Bool: ";
                bool first = true;
                for (const auto& [name, value] : stateDebug.boolParameters) {
                    if (!first) boolLine << ", ";
                    boolLine << name << "=" << (value ? "true" : "false");
                    first = false;
                }
                lines.push_back(boolLine.str());
            }

            if (!stateDebug.floatParameters.empty()) {
                std::ostringstream floatLine;
                floatLine << "  Float: ";
                bool first = true;
                for (const auto& [name, value] : stateDebug.floatParameters) {
                    if (!first) floatLine << ", ";
                    floatLine << name << "=" << std::fixed << std::setprecision(2) << value;
                    first = false;
                }
                lines.push_back(floatLine.str());
            }

            if (!stateDebug.activeTriggers.empty()) {
                std::ostringstream triggerLine;
                triggerLine << "  Triggers: ";
                bool first = true;
                for (const auto& name : stateDebug.activeTriggers) {
                    if (!first) triggerLine << ", ";
                    triggerLine << name;
                    first = false;
                }
                lines.push_back(triggerLine.str());
            }
        }

        if (m_includeEvents && !events.empty()) {
            lines.push_back("  Recent Events:");
            size_t count = std::min(events.size(), m_maxEventsPerEntity);
            auto beginIt = events.size() > count ? events.end() - static_cast<std::ptrdiff_t>(count) : events.begin();
            for (auto it = beginIt; it != events.end(); ++it) {
                const auto& logEntry = *it;
                const auto& evt = logEntry.event;
                std::ostringstream eventLine;
                eventLine << "    #" << static_cast<unsigned long long>(logEntry.sequence)
                          << " [" << OriginToString(logEntry.origin) << "] "
                          << EventTypeToString(evt.type)
                          << " clip=" << evt.clip
                          << " frame=" << evt.frameIndex;
                if (!logEntry.note.empty()) {
                    eventLine << " note=" << logEntry.note;
                }
                lines.push_back(eventLine.str());
            }
        }
    }

    return lines;
}

std::string SpriteAnimationDebugPanel::BuildPanelString() const {
    auto lines = BuildPanelLines();
    std::ostringstream oss;
    for (const auto& line : lines) {
        oss << line << '\n';
    }
    return oss.str();
}

void SpriteAnimationDebugPanel::RenderToLogger() const {
    auto lines = BuildPanelLines();
    for (const auto& line : lines) {
        Logger::GetInstance().Info(line);
    }
}

} // namespace Render::ECS


