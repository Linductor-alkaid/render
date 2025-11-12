#include "render/application/scene_types.h"

#include <algorithm>

namespace Render::Application {

namespace {
bool ContainsResource(const std::vector<ResourceRequest>& list, const ResourceRequest& request) {
    return std::any_of(list.begin(), list.end(), [&](const ResourceRequest& existing) {
        return existing.identifier == request.identifier && existing.type == request.type;
    });
}
} // namespace

void SceneResourceManifest::Merge(const SceneResourceManifest& other) {
    for (const auto& request : other.required) {
        if (!ContainsResource(required, request)) {
            required.push_back(request);
        }
    }

    for (const auto& request : other.optional) {
        if (ContainsResource(required, request) || ContainsResource(optional, request)) {
            continue;
        }
        optional.push_back(request);
    }
}

} // namespace Render::Application


