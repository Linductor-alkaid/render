#include "render/geometry_preset.h"

#include "render/mesh_loader.h"
#include "render/resource_manager.h"
#include "render/logger.h"

namespace Render {

namespace {

Ref<Mesh> MakePlaneXZ() {
    return MeshLoader::CreatePlane();
}

Ref<Mesh> MakeCube() {
    return MeshLoader::CreateCube();
}

Ref<Mesh> MakeSphere() {
    return MeshLoader::CreateSphere();
}

Ref<Mesh> MakeCylinder() {
    return MeshLoader::CreateCylinder();
}

Ref<Mesh> MakeCone() {
    return MeshLoader::CreateCone();
}

Ref<Mesh> MakeTorus() {
    return MeshLoader::CreateTorus();
}

Ref<Mesh> MakeCapsule() {
    return MeshLoader::CreateCapsule();
}

Ref<Mesh> MakeQuadXZ() {
    return MeshLoader::CreatePlane(1.0f, 1.0f, 1, 1, Color::White());
}

Ref<Mesh> MakeQuadXY() {
    auto mesh = MeshLoader::CreateQuad();
    return mesh;
}

Ref<Mesh> MakeTriangle() {
    return MeshLoader::CreateTriangle();
}

Ref<Mesh> MakeCircle() {
    return MeshLoader::CreateCircle();
}

const std::unordered_map<std::string, GeometryPreset::PresetInfo>& BuildPresetMap() {
    static const std::unordered_map<std::string, GeometryPreset::PresetInfo> presets = {
        {"geometry::plane_xz", {"geometry::plane_xz", &MakePlaneXZ}},
        {"geometry::cube", {"geometry::cube", &MakeCube}},
        {"geometry::sphere", {"geometry::sphere", &MakeSphere}},
        {"geometry::cylinder", {"geometry::cylinder", &MakeCylinder}},
        {"geometry::cone", {"geometry::cone", &MakeCone}},
        {"geometry::torus", {"geometry::torus", &MakeTorus}},
        {"geometry::capsule", {"geometry::capsule", &MakeCapsule}},
        {"geometry::quad_xz", {"geometry::quad_xz", &MakeQuadXZ}},
        {"geometry::quad_xy", {"geometry::quad_xy", &MakeQuadXY}},
        {"geometry::triangle", {"geometry::triangle", &MakeTriangle}},
        {"geometry::circle", {"geometry::circle", &MakeCircle}},
    };
    return presets;
}

} // namespace

const std::unordered_map<std::string, GeometryPreset::PresetInfo>& GeometryPreset::GetPresetMap() {
    static const auto presets = BuildPresetMap();
    return presets;
}

bool GeometryPreset::HasPreset(const std::string& name) {
    const auto& map = GetPresetMap();
    return map.find(name) != map.end();
}

Ref<Mesh> GeometryPreset::GetMesh(ResourceManager& resourceManager, const std::string& name) {
    const auto& map = GetPresetMap();
    auto it = map.find(name);
    if (it == map.end()) {
        Logger::GetInstance().WarningFormat("[GeometryPreset] Unknown preset '%s'", name.c_str());
        return nullptr;
    }

    if (resourceManager.HasMesh(name)) {
        return resourceManager.GetMesh(name);
    }

    Ref<Mesh> mesh = nullptr;
    try {
        mesh = it->second.factory();
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[GeometryPreset] Failed to create mesh '%s': %s",
            name.c_str(), e.what());
        return nullptr;
    }

    if (!mesh) {
        Logger::GetInstance().ErrorFormat("[GeometryPreset] Factory for '%s' returned null mesh", name.c_str());
        return nullptr;
    }

    resourceManager.RegisterMesh(name, mesh);
    return mesh;
}

void GeometryPreset::RegisterDefaults(ResourceManager& resourceManager) {
    const auto& map = GetPresetMap();
    for (const auto& [name, info] : map) {
        if (resourceManager.HasMesh(name)) {
            continue;
        }

        try {
            Ref<Mesh> mesh = info.factory();
            if (!mesh) {
                Logger::GetInstance().WarningFormat("[GeometryPreset] Factory returned null mesh for '%s'", name.c_str());
                continue;
            }
            resourceManager.RegisterMesh(name, mesh);
            Logger::GetInstance().InfoFormat("[GeometryPreset] Registered preset mesh '%s'", name.c_str());
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[GeometryPreset] Failed to register preset '%s': %s",
                name.c_str(), e.what());
        }
    }
}

} // namespace Render


