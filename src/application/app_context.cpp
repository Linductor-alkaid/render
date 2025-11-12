#include "render/application/app_context.h"

#include <stdexcept>

#include "render/async_resource_loader.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/uniform_manager.h"

namespace Render::Application {

bool AppContext::IsValid() const noexcept {
    return renderer != nullptr && resourceManager != nullptr && asyncLoader != nullptr;
}

void AppContext::ValidateOrThrow(const std::string& source) const {
    if (IsValid()) {
        return;
    }

    std::string message = "AppContext validation failed in " + source +
                          ": renderer/resourceManager/asyncLoader must be initialized.";
    Logger::GetInstance().Error(message);
    throw std::runtime_error(message);
}

} // namespace Render::Application

