#pragma once

#include "render/application/app_context.h"
#include "render/application/event_bus.h"

namespace Render::Application {

struct FrameBeginEvent final : public EventBase {
    FrameUpdateArgs frame{};
};

struct FrameTickEvent final : public EventBase {
    FrameUpdateArgs frame{};
};

struct FrameEndEvent final : public EventBase {
    FrameUpdateArgs frame{};
};

} // namespace Render::Application


