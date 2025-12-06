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
#include <render/render_batching.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/shader_cache.h>
#include <render/logger.h>
#include <render/ecs/world.h>
#include <render/ecs/components.h>
#include <render/ecs/systems.h>
#include <render/types.h>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using namespace Render;
using namespace Render::ECS;

namespace {

std::string ToString(BatchingMode mode) {
    switch (mode) {
        case BatchingMode::Disabled: return "Disabled";
        case BatchingMode::CpuMerge: return "CpuMerge";
        case BatchingMode::GpuInstancing: return "GpuInstancing";
        default: return "Unknown";
    }
}

struct StatsAccumulator {
    uint64_t frames = 0;
    uint64_t drawCalls = 0;
    uint64_t batchCount = 0;
    uint64_t batchedDrawCalls = 0;
    uint64_t instancedDrawCalls = 0;
    uint64_t instancedInstances = 0;
    uint64_t fallbackDrawCalls = 0;
    uint64_t fallbackBatches = 0;
    uint64_t batchedTriangles = 0;
    uint64_t batchedVertices = 0;
    uint64_t workerProcessed = 0;
    uint32_t workerMaxQueueDepth = 0;
    double  workerWaitTimeMs = 0.0;

    void Accumulate(const RenderStats& stats) {
        frames++;
        drawCalls += stats.drawCalls;
        batchCount += stats.batchCount;
        batchedDrawCalls += stats.batchedDrawCalls;
        instancedDrawCalls += stats.instancedDrawCalls;
        instancedInstances += stats.instancedInstances;
        fallbackDrawCalls += stats.fallbackDrawCalls;
        fallbackBatches += stats.fallbackBatches;
        batchedTriangles += stats.batchedTriangles;
        batchedVertices += stats.batchedVertices;
        workerProcessed += stats.workerProcessed;
        workerMaxQueueDepth = std::max(workerMaxQueueDepth, stats.workerMaxQueueDepth);
        workerWaitTimeMs += stats.workerWaitTimeMs;
    }

    void LogSummary(BatchingMode mode) const {
        Logger::GetInstance().InfoFormat(
            "[BatchingBenchmark] Mode=%s | frames=%llu | avgDrawCalls=%.2f | avgBatchCount=%.2f | "
            "avgBatchedDrawCalls=%.2f | avgInstancedDrawCalls=%.2f | avgInstancedInstances=%.2f | "
            "avgFallbackDrawCalls=%.2f | maxWorkerQueue=%u | avgWorkerProcessed=%.2f | totalWaitMs=%.3f",
            ToString(mode).c_str(),
            static_cast<unsigned long long>(frames),
            frames ? static_cast<double>(drawCalls) / frames : 0.0,
            frames ? static_cast<double>(batchCount) / frames : 0.0,
            frames ? static_cast<double>(batchedDrawCalls) / frames : 0.0,
            frames ? static_cast<double>(instancedDrawCalls) / frames : 0.0,
            frames ? static_cast<double>(instancedInstances) / frames : 0.0,
            frames ? static_cast<double>(fallbackDrawCalls) / frames : 0.0,
            workerMaxQueueDepth,
            frames ? static_cast<double>(workerProcessed) / frames : 0.0,
            workerWaitTimeMs
        );
    }
};

struct SceneConfig {
    Vector3 cameraPosition{0.0f, 0.0f, 20.0f};
    Color diffuseColor{0.4f, 0.7f, 1.0f, 1.0f};
};

} // namespace

int main() {
    Logger::GetInstance().InfoFormat("[BatchingBenchmark] === Render Batching Benchmark ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer->Initialize("Batching Benchmark", 1280, 720)) {
        Logger::GetInstance().ErrorFormat("[BatchingBenchmark] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        return 1;
    }

    renderer->SetVSync(false);
    renderer->SetClearColor(Color(0.05f, 0.05f, 0.08f, 1.0f));

    auto& shaderCache = ShaderCache::GetInstance();
    auto shader = shaderCache.LoadShader("basic_batch", "shaders/basic.vert", "shaders/basic.frag");
    if (!shader) {
        Logger::GetInstance().ErrorFormat("[BatchingBenchmark] Failed to load shader");
        Renderer::Destroy(renderer);
        return 1;
    }

    auto material = std::make_shared<Material>();
    material->SetShader(shader);
    material->SetDiffuseColor(Color(0.4f, 0.7f, 1.0f, 1.0f));
    material->SetBlendMode(BlendMode::None);

    auto mesh = MeshLoader::CreateCube(1.0f);
    if (!mesh) {
        Logger::GetInstance().ErrorFormat("[BatchingBenchmark] Failed to create cube mesh");
        Renderer::Destroy(renderer);
        return 1;
    }

    // 创建 ECS World
    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<MeshRenderComponent>();
    world->RegisterComponent<CameraComponent>();
    world->RegisterComponent<NameComponent>();
    world->RegisterComponent<ActiveComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<CameraSystem>();
    world->RegisterSystem<UniformSystem>(renderer);
    world->RegisterSystem<MeshRenderSystem>(renderer);

    world->PostInitialize();

    SceneConfig sceneConfig{};

    // 创建相机
    EntityID cameraEntity = world->CreateEntity({ .name = "MainCamera", .active = true });
    TransformComponent cameraTransform;
    cameraTransform.SetPosition(sceneConfig.cameraPosition);
    world->AddComponent(cameraEntity, cameraTransform);

    CameraComponent cameraComp;
    cameraComp.camera = std::make_shared<Camera>();
    cameraComp.camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 200.0f);
    cameraComp.active = true;
    world->AddComponent(cameraEntity, cameraComp);

    const int gridDim = 20;      // 400 instances
    const float spacing = 1.5f;
    const float offset = (gridDim - 1) * spacing * 0.5f;

    std::vector<EntityID> entities;
    entities.reserve(static_cast<size_t>(gridDim) * gridDim);

    for (int y = 0; y < gridDim; ++y) {
        for (int x = 0; x < gridDim; ++x) {
            EntityID entity = world->CreateEntity({ 
                .name = "Cube_" + std::to_string(y * gridDim + x), 
                .active = true 
            });

            // 设置变换
            TransformComponent transform;
            transform.SetPosition(Vector3(x * spacing - offset, y * spacing - offset, 0.0f));
            transform.SetScale(Vector3(0.9f, 0.9f, 0.9f));
            world->AddComponent(entity, transform);

            // 设置网格渲染组件
            MeshRenderComponent meshComp;
            meshComp.mesh = mesh;
            meshComp.material = material;
            meshComp.visible = true;
            meshComp.layerID = 300;
            meshComp.castShadows = false;
            meshComp.receiveShadows = false;
            meshComp.resourcesLoaded = true;
            world->AddComponent(entity, meshComp);

            entities.push_back(entity);
        }
    }

    Logger::GetInstance().InfoFormat("[BatchingBenchmark] Created %zu entities", entities.size());

    const int warmupFrames = 30;
    const int measureFrames = 180;
    bool running = true;

    std::array<BatchingMode, 3> testModes = {
        BatchingMode::Disabled,
        BatchingMode::CpuMerge,
        BatchingMode::GpuInstancing
    };

    for (BatchingMode mode : testModes) {
        if (!running) {
            break;
        }

        Logger::GetInstance().InfoFormat("[BatchingBenchmark] === Testing mode: %s ===", ToString(mode).c_str());
        renderer->SetBatchingMode(mode);

        StatsAccumulator stats;

        for (int frame = 0; frame < warmupFrames + measureFrames && running; ++frame) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                    event.type == SDL_EVENT_WINDOW_DESTROYED) {
                    running = false;
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }

            bool frameSucceeded = true;

            try {
                renderer->BeginFrame();
                renderer->Clear();

                // 设置shader uniforms
                if (auto uniformMgr = shader->GetUniformManager()) {
                    uniformMgr->SetColor("uColor", sceneConfig.diffuseColor);
                    uniformMgr->SetBool("uUseTexture", false);
                    uniformMgr->SetBool("uUseVertexColor", false);
                }

                // 使用ECS系统更新和渲染
                world->Update(0.016f);
                renderer->FlushRenderQueue();

                renderer->EndFrame();
            } catch (const std::exception& e) {
                frameSucceeded = false;
                Logger::GetInstance().ErrorFormat("[BatchingBenchmark] Rendering error: %s", e.what());
                try {
                    renderer->EndFrame();
                } catch (...) {
                    Logger::GetInstance().Error("[BatchingBenchmark] Failed to EndFrame after exception");
                }
            }

            try {
                renderer->Present();
            } catch (const std::exception& e) {
                frameSucceeded = false;
                Logger::GetInstance().ErrorFormat("[BatchingBenchmark] Present error: %s", e.what());
            }

            if (frameSucceeded && frame >= warmupFrames) {
                stats.Accumulate(renderer->GetStats());
            }

            SDL_Delay(1);
        }

        stats.LogSummary(mode);
    }

    world->Shutdown();
    Renderer::Destroy(renderer);
    Logger::GetInstance().InfoFormat("[BatchingBenchmark] Completed. Press any key to exit window.");
    return 0;
}

