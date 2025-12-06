/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include <render/renderer.h>
#include <render/logger.h>
#include <render/material.h>
#include <render/shader_cache.h>
#include <render/resource_manager.h>
#include <render/geometry_preset.h>
#include <render/math_utils.h>
#include <render/texture_loader.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Render;
using namespace Render::ECS;

namespace {

struct ShapeEntry {
    std::string meshName;
    Vector3 position;
    Vector3 rotationSpeed;
    Color color;
};

std::vector<ShapeEntry> BuildShapeLayout(float spacing = 2.5f) {
    return {
        {"geometry::cube",        Vector3(-spacing, 1.0f, -spacing), Vector3(20.0f, 15.0f, 0.0f), Color(0.9f, 0.4f, 0.3f, 1.0f)},
        {"geometry::sphere",      Vector3(0.0f,     1.0f, -spacing), Vector3(10.0f, 35.0f, 0.0f), Color(0.3f, 0.7f, 0.9f, 1.0f)},
        {"geometry::cylinder",    Vector3(spacing,  1.0f, -spacing), Vector3(0.0f, 45.0f, 0.0f), Color(0.6f, 0.8f, 0.3f, 1.0f)},
        {"geometry::cone",        Vector3(-spacing, 1.0f, 0.0f),     Vector3(40.0f, 0.0f, 20.0f), Color(0.9f, 0.9f, 0.3f, 1.0f)},
        {"geometry::torus",       Vector3(0.0f,     1.0f, 0.0f),     Vector3(0.0f, 30.0f, 25.0f), Color(0.8f, 0.3f, 0.9f, 1.0f)},
        {"geometry::capsule",     Vector3(spacing,  1.0f, 0.0f),     Vector3(25.0f, 0.0f, 35.0f), Color(0.4f, 0.9f, 0.6f, 1.0f)},
        {"geometry::quad_xy",     Vector3(-spacing, 0.0f, spacing),  Vector3(0.0f, 0.0f, 0.0f),  Color(0.7f, 0.7f, 0.7f, 1.0f)},
        {"geometry::triangle",    Vector3(0.0f,     0.0f, spacing),  Vector3(0.0f, 0.0f, 0.0f),  Color(0.9f, 0.5f, 0.2f, 1.0f)},
        {"geometry::circle",      Vector3(spacing,  0.0f, spacing),  Vector3(0.0f, 0.0f, 0.0f),  Color(0.3f, 0.5f, 0.9f, 1.0f)}
    };
}

TexturePtr LoadNormalMapTexture() {
    auto& loader = TextureLoader::GetInstance();
    const std::vector<std::string> candidates = {
        "textures/faxiantest.jpeg",
        "textures/faxiantest.png",
        "textures/faxintest.jpeg",
        "textures/faxintest.png"
    };

    for (const auto& path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }

        std::string cacheKey = "geometry_catalog_normal_" + path;
        auto texture = loader.LoadTexture(cacheKey, path, true);
        if (texture && texture->IsValid()) {
            texture->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
            texture->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
            Logger::GetInstance().InfoFormat("[GeometryCatalogTest] Normal map loaded: %s", path.c_str());
            return texture;
        }
    }

    Logger::GetInstance().Warning("[GeometryCatalogTest] Normal map not found, skipping normal map demo");
    return nullptr;
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().Info("[GeometryCatalogTest] === Geometry Catalog Demo ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("Geometry Catalog Test", 1600, 900)) {
        Logger::GetInstance().Error("[GeometryCatalogTest] Failed to initialize renderer");
        return -1;
    }
    renderer->SetClearColor(Color(0.06f, 0.08f, 0.11f, 1.0f));
    renderer->SetVSync(true);

    auto& resourceManager = ResourceManager::GetInstance();
    resourceManager.RegisterDefaultGeometry();

    auto& shaderCache = ShaderCache::GetInstance();
    auto phongShader = shaderCache.LoadShader(
        "geometry_catalog_phong",
        "shaders/material_phong.vert",
        "shaders/material_phong.frag"
    );
    if (!phongShader || !phongShader->IsValid()) {
        Logger::GetInstance().Error("[GeometryCatalogTest] Failed to load material_phong shader");
        Renderer::Destroy(renderer);
        return -1;
    }

    auto world = std::make_shared<World>();
    world->Initialize();
    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<ActiveComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<MeshRenderSystem>(renderer);

    world->PostInitialize();

    EntityID cameraEntity = world->CreateEntity({.name = "MainCamera", .active = true});
    TransformComponent cameraTransform;
    Vector3 cameraStartPos(0.0f, 4.0f, 12.0f);
    cameraTransform.SetPosition(cameraStartPos);
    cameraTransform.LookAt(Vector3(0.0f, 1.0f, 0.0f));
    world->AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComponent;
    cameraComponent.camera = std::make_shared<Camera>();
    cameraComponent.camera->SetPerspective(55.0f, static_cast<float>(renderer->GetWidth()) / renderer->GetHeight(), 0.1f, 200.0f);
    cameraComponent.active = true;
    world->AddComponent(cameraEntity, cameraComponent);

    std::vector<EntityID> shapeEntities;
    std::vector<ShapeEntry> shapes = BuildShapeLayout();
    shapeEntities.reserve(shapes.size());

    TexturePtr sharedNormalMap = LoadNormalMapTexture();
    const std::unordered_set<std::string> normalMapShapes = {
        "geometry::sphere",
        "geometry::torus"
    };

    int materialIndex = 0;
    for (const auto& shape : shapes) {
        auto mesh = GeometryPreset::GetMesh(resourceManager, shape.meshName);
        if (!mesh) {
            Logger::GetInstance().WarningFormat("[GeometryCatalogTest] Failed to fetch preset mesh: %s",
                                                shape.meshName.c_str());
            continue;
        }

        std::string materialName = shape.meshName + "_mat";
        Ref<Material> material;
        if (resourceManager.HasMaterial(materialName)) {
            material = resourceManager.GetMaterial(materialName);
        } else {
            material = CreateRef<Material>();
            material->SetName(materialName);
            material->SetShader(phongShader);
            material->SetDiffuseColor(shape.color);
            material->SetAmbientColor(Color(shape.color.r * 0.25f, shape.color.g * 0.25f, shape.color.b * 0.25f, shape.color.a));
            material->SetSpecularColor(Color(0.4f, 0.4f, 0.4f, 1.0f));
            material->SetShininess(32.0f);
            resourceManager.RegisterMaterial(materialName, material);
        }

        if (sharedNormalMap && normalMapShapes.count(shape.meshName) > 0) {
            material->SetTexture("normalMap", sharedNormalMap);
            Logger::GetInstance().DebugFormat(
                "[GeometryCatalogTest] Normal map assigned to %s", shape.meshName.c_str());
        }

        if (shape.meshName == "geometry::plane") {
            material->SetVector2Array("uExtraUVSetScales[0]", { Vector2(2.0f, 2.0f) });
            material->SetInt("uExtraUVSetCount", 1);
        } else if (shape.meshName == "geometry::torus") {
            material->SetVector2Array("uExtraUVSetScales[0]", { Vector2(1.5f, 0.75f) });
            material->SetInt("uExtraUVSetCount", 1);
        } else {
            material->SetVector2Array("uExtraUVSetScales[0]", {});
            material->SetInt("uExtraUVSetCount", 0);
        }

        if (shape.meshName == "geometry::cube") {
            material->SetColorArray("uExtraColorSets[0]", { Color(1.2f, 1.0f, 1.0f, 1.0f) });
            material->SetInt("uExtraColorSetCount", 1);
        } else if (shape.meshName == "geometry::capsule") {
            material->SetColorArray("uExtraColorSets[0]", { Color(0.9f, 1.1f, 1.1f, 1.0f) });
            material->SetInt("uExtraColorSetCount", 1);
        } else {
            material->SetColorArray("uExtraColorSets[0]", {});
            material->SetInt("uExtraColorSetCount", 0);
        }

        EntityID entity = world->CreateEntity({.name = shape.meshName, .active = true});

        TransformComponent transform;
        transform.SetPosition(shape.position);
        transform.SetScale(Vector3::Ones());
        world->AddComponent(entity, transform);

        MeshRenderComponent meshComp;
        meshComp.meshName = shape.meshName;
        meshComp.materialName = materialName;
        meshComp.mesh = mesh;
        meshComp.material = material;
        meshComp.resourcesLoaded = true;
        meshComp.castShadows = false;
        meshComp.receiveShadows = true;
        meshComp.SetDiffuseColor(shape.color);
        world->AddComponent(entity, meshComp);

        shapeEntities.push_back(entity);
        ++materialIndex;
    }

    if (shapeEntities.empty()) {
        Logger::GetInstance().Error("[GeometryCatalogTest] No shapes were created, aborting.");
        Renderer::Destroy(renderer);
        return -1;
    }

    Logger::GetInstance().Info("[GeometryCatalogTest] Controls: ESC - quit, TAB - toggle wireframe, SPACE - pause rotation");

    bool running = true;
    bool wireframe = false;
    bool paused = false;
    Uint64 prevTicks = SDL_GetTicks();
    float elapsed = 0.0f;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_TAB:
                    wireframe = !wireframe;
                    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
                    Logger::GetInstance().InfoFormat("[GeometryCatalogTest] Wireframe: %s", wireframe ? "ON" : "OFF");
                    break;
                case SDLK_SPACE:
                    paused = !paused;
                    Logger::GetInstance().InfoFormat("[GeometryCatalogTest] Rotation %s", paused ? "PAUSED" : "RESUMED");
                    break;
                default:
                    break;
                }
            }
        }

        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - prevTicks) * 0.001f;
        prevTicks = currentTicks;
        deltaTime = std::min(deltaTime, 0.033f);
        if (!paused) {
            elapsed += deltaTime;
        }

        for (size_t i = 0; i < shapeEntities.size(); ++i) {
            const auto& cfg = shapes[i];
            if (!cfg.rotationSpeed.isZero()) {
                auto& transform = world->GetComponent<TransformComponent>(shapeEntities[i]);
                Vector3 euler = cfg.rotationSpeed * elapsed;
                transform.SetRotation(MathUtils::FromEulerDegrees(euler.x(), euler.y(), euler.z()));
            }
        }

        renderer->BeginFrame();
        renderer->Clear();

        if (auto uniformMgr = phongShader->GetUniformManager()) {
            uniformMgr->SetVector3("uLightPos", Vector3(4.0f, 6.0f, 6.0f));
            uniformMgr->SetVector3("uViewPos", cameraTransform.GetPosition());
            uniformMgr->SetColor("uAmbientColor", Color(0.15f, 0.15f, 0.18f, 1.0f));
            uniformMgr->SetColor("uDiffuseColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
            uniformMgr->SetColor("uSpecularColor", Color(0.8f, 0.8f, 0.8f, 1.0f));
            uniformMgr->SetFloat("uShininess", 48.0f);
        }

        world->Update(deltaTime);
        renderer->FlushRenderQueue();

        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(16);
    }

    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    world->Shutdown();
    Renderer::Destroy(renderer);

    Logger::GetInstance().Info("[GeometryCatalogTest] Shutdown complete");
    return 0;
}


