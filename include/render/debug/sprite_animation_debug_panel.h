#pragma once

#include <string>
#include <vector>

#include "render/ecs/entity.h"

namespace Render::ECS {

class SpriteAnimationDebugPanel {
public:
    SpriteAnimationDebugPanel();

    void SetIncludeEvents(bool include);
    void SetIncludeParameters(bool include);
    void SetIncludeSpriteInfo(bool include);
    void SetMaxEventsPerEntity(size_t count);

    [[nodiscard]] bool IsIncludeEvents() const { return m_includeEvents; }
    [[nodiscard]] bool IsIncludeParameters() const { return m_includeParameters; }
    [[nodiscard]] bool IsIncludeSpriteInfo() const { return m_includeSpriteInfo; }
    [[nodiscard]] size_t GetMaxEventsPerEntity() const { return m_maxEventsPerEntity; }

    void WatchEntity(EntityID entity) const;
    void WatchAllEntities(bool enable) const;
    void UnwatchEntity(EntityID entity) const;

    [[nodiscard]] std::vector<std::string> BuildPanelLines() const;
    [[nodiscard]] std::string BuildPanelString() const;
    void RenderToLogger() const;

private:
    bool m_includeEvents = true;
    bool m_includeParameters = true;
    bool m_includeSpriteInfo = true;
    size_t m_maxEventsPerEntity = 5;
};

} // namespace Render::ECS


