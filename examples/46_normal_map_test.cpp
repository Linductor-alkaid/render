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
/**
 * @file 46_normal_map_test.cpp
 * @brief 法线贴图与切线空间验证示例
 *
 * 展示内容：
 * 1. 使用 MeshLoader 生成带切线空间的平面网格
 * 2. 同时加载漫反射与法线贴图，构建简单的 Phong 着色
 * 3. 通过键盘切换法线贴图/漫反射贴图，验证切线空间计算正确性
 *
 * 控制：
 * - ESC：退出
 * - N：启用/禁用法线贴图
 * - D：启用/禁用漫反射贴图
 * - L：启用/禁用旋转光源
 */

#include <render/renderer.h>
#include <render/shader_cache.h>
#include <render/mesh_loader.h>
#include <render/texture_loader.h>
#include <render/logger.h>

#include <SDL3/SDL.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace Render;

namespace {

Ref<Shader> gShader;
Ref<Mesh> gPlane;
TexturePtr gDiffuseMap;
TexturePtr gNormalMap;

bool gUseNormalMap = true;
bool gUseDiffuseMap = true;
bool gRotateLight = true;

float gRotationAngle = 0.0f;
float gLightTimer = 0.0f;
Vector3 gLightDirection(-0.3f, -1.0f, -0.2f);

bool LoadTextures() {
    auto& loader = TextureLoader::GetInstance();

    const std::vector<std::string> diffuseCandidates = {
        "textures/manfanshetest.png",
        "textures/manfanshetest.jpg",
        "textures/test.jpg",
        "textures/test.png"
    };

    gDiffuseMap = nullptr;
    for (const auto& path : diffuseCandidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        std::string cacheKey = "normal_map_test_diffuse_" + path;
        gDiffuseMap = loader.LoadTexture(cacheKey, path, true);
        if (gDiffuseMap && gDiffuseMap->IsValid()) {
            Logger::GetInstance().Info("漫反射贴图加载成功: " + path);
            break;
        }
    }

    if (gDiffuseMap) {
        gDiffuseMap->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
        gDiffuseMap->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
        gUseDiffuseMap = true;
    } else {
        Logger::GetInstance().Warning("未找到漫反射贴图，将使用常量颜色");
        gUseDiffuseMap = false;
    }

    const std::vector<std::string> normalCandidates = {
        "textures/faxiantest.jpeg",
        "textures/faxiantest.png",
        "textures/faxintest.jpeg",
        "textures/faxintest.png"
    };

    gNormalMap = nullptr;
    for (const auto& path : normalCandidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        std::string cacheKey = "normal_map_test_normal_" + path;
        gNormalMap = loader.LoadTexture(cacheKey, path, true);
        if (gNormalMap && gNormalMap->IsValid()) {
            Logger::GetInstance().Info("法线贴图加载成功: " + path);
            break;
        }
    }

    if (!gNormalMap) {
        Logger::GetInstance().Warning("未找到法线贴图，法线贴图功能将禁用");
        gUseNormalMap = false;
    }

    return true;
}

bool InitScene(Renderer& renderer) {
    Logger::GetInstance().Info("=== 正在初始化法线贴图测试场景 ===");

    gShader = ShaderCache::GetInstance().LoadShader(
        "normal_map_test",
        "shaders/normal_map.vert",
        "shaders/normal_map.frag"
    );
    if (!gShader) {
        Logger::GetInstance().Error("加载 normal_map 着色器失败");
        return false;
    }

    gPlane = MeshLoader::CreatePlane(2.0f, 2.0f, 4, 4, Color::White());
    if (!gPlane) {
        Logger::GetInstance().Error("创建平面网格失败");
        return false;
    }

    auto state = renderer.GetRenderState();
    state->SetDepthTest(true);
    state->SetCullFace(CullFace::None);
    state->SetClearColor(Color(0.08f, 0.09f, 0.12f, 1.0f));

    LoadTextures();

    gShader->Use();
    auto* uniforms = gShader->GetUniformManager();
    uniforms->RegisterTextureUniform("diffuseMap", 0);
    uniforms->RegisterTextureUniform("normalMap", 1);
    uniforms->SetVector3("uAmbientColor", Vector3(0.05f, 0.05f, 0.06f));
    uniforms->SetVector3("uDiffuseColor", Vector3(0.85f, 0.85f, 0.85f));
    uniforms->SetVector3("uSpecularColor", Vector3(0.25f, 0.25f, 0.25f));
    uniforms->SetFloat("uShininess", 24.0f);
    gShader->Unuse();

    Logger::GetInstance().Info("初始化完成：N=法线贴图, D=漫反射贴图, L=旋转光源");
    return true;
}

void UpdateScene(float deltaTime) {
    gRotationAngle += deltaTime * 30.0f;
    if (gRotationAngle >= 360.0f) {
        gRotationAngle -= 360.0f;
    }

    if (gRotateLight) {
        gLightTimer += deltaTime;
        float angle = gLightTimer * 0.8f;
        gLightDirection = Vector3(
            std::cos(angle) * 0.35f,
            -1.0f,
            std::sin(angle) * 0.35f - 0.2f
        );
    }
}

Matrix4 MakeViewMatrix() {
    Matrix4 view = Matrix4::Identity();
    view(2, 3) = -3.0f;
    view(1, 3) = -0.5f;

    float tilt = 20.0f * 3.14159265f / 180.0f;
    Matrix4 tiltMatrix = Matrix4::Identity();
    tiltMatrix(1, 1) = std::cos(tilt);
    tiltMatrix(1, 2) = -std::sin(tilt);
    tiltMatrix(2, 1) = std::sin(tilt);
    tiltMatrix(2, 2) = std::cos(tilt);

    return tiltMatrix * view;
}

Matrix4 MakeProjectionMatrix(float aspect) {
    const float fov = 45.0f * 3.14159265f / 180.0f;
    const float nearPlane = 0.1f;
    const float farPlane = 50.0f;
    const float tanHalfFov = std::tan(fov / 2.0f);

    Matrix4 projection = Matrix4::Zero();
    projection(0, 0) = 1.0f / (aspect * tanHalfFov);
    projection(1, 1) = 1.0f / tanHalfFov;
    projection(2, 2) = -(farPlane + nearPlane) / (farPlane - nearPlane);
    projection(2, 3) = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    projection(3, 2) = -1.0f;

    return projection;
}

void RenderScene(Renderer& renderer) {
    renderer.Clear(true, true, false);

    Matrix4 model = Matrix4::Identity();
    float angleRad = gRotationAngle * 3.14159265f / 180.0f;
    model(0, 0) = std::cos(angleRad);
    model(0, 2) = std::sin(angleRad);
    model(2, 0) = -std::sin(angleRad);
    model(2, 2) = std::cos(angleRad);

    Matrix4 view = MakeViewMatrix();
    Matrix4 projection = MakeProjectionMatrix(static_cast<float>(renderer.GetWidth()) / static_cast<float>(renderer.GetHeight()));

    gShader->Use();
    auto* uniforms = gShader->GetUniformManager();
    uniforms->SetMatrix4("uModel", model);
    uniforms->SetMatrix4("uView", view);
    uniforms->SetMatrix4("uProjection", projection);
    uniforms->SetVector3("uViewPos", Vector3(0.0f, 0.5f, 3.0f));

    Vector3 lightDir = gLightDirection;
    lightDir.normalize();
    uniforms->SetVector3("uLightDir", lightDir);
    uniforms->SetBool("hasDiffuseMap", gUseDiffuseMap && gDiffuseMap && gDiffuseMap->IsValid());
    if (gUseDiffuseMap && gDiffuseMap && gDiffuseMap->IsValid()) {
        Logger::GetInstance().Debug("漫反射贴图启用: size=" + std::to_string(gDiffuseMap->GetWidth()) + "x" + std::to_string(gDiffuseMap->GetHeight()));
    }
    uniforms->SetBool("hasNormalMap", gUseNormalMap && gNormalMap && gNormalMap->IsValid());

    if (gDiffuseMap && gUseDiffuseMap) {
        gDiffuseMap->Bind(0);
    }
    if (gNormalMap && gUseNormalMap) {
        gNormalMap->Bind(1);
    }

    gPlane->Draw();
    gShader->Unuse();
}

void HandleInput(const SDL_Event& event, bool& running) {
    if (event.type == SDL_EVENT_KEY_DOWN) {
        switch (event.key.key) {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_N:
                gUseNormalMap = !gUseNormalMap;
                Logger::GetInstance().Info(gUseNormalMap ? "启用法线贴图" : "禁用法线贴图");
                break;
            case SDLK_D:
                gUseDiffuseMap = !gUseDiffuseMap;
                Logger::GetInstance().Info(gUseDiffuseMap ? "启用漫反射贴图" : "禁用漫反射贴图");
                if (gDiffuseMap) {
                    Logger::GetInstance().Info(
                        "漫反射贴图状态: valid=" + std::to_string(gDiffuseMap->IsValid()) +
                        ", w=" + std::to_string(gDiffuseMap->GetWidth()) +
                        ", h=" + std::to_string(gDiffuseMap->GetHeight())
                    );
                }
                break;
            case SDLK_L:
                gRotateLight = !gRotateLight;
                Logger::GetInstance().Info(gRotateLight ? "启用旋转光源" : "静止光源");
                break;
            default:
                break;
        }
    }
}

void Cleanup() {
    Logger::GetInstance().Info("清理资源...");
    gPlane.reset();
    gShader.reset();
    gDiffuseMap.reset();
    gNormalMap.reset();
    ShaderCache::GetInstance().Clear();
}

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== 46_normal_map_test 启动 ===");
    Logger::GetInstance().Info("日志文件: " + Logger::GetInstance().GetCurrentLogFile());

    Renderer renderer;
    if (!renderer.Initialize("法线贴图测试 - 46_normal_map_test", 1280, 720)) {
        Logger::GetInstance().Error("Renderer 初始化失败");
        return -1;
    }

    if (!InitScene(renderer)) {
        renderer.Shutdown();
        return -1;
    }

    bool running = true;
    SDL_Event event;
    Uint64 lastTicks = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            HandleInput(event, running);
        }

        Uint64 currentTicks = SDL_GetTicks();
        float deltaTime = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        UpdateScene(deltaTime);

        renderer.BeginFrame();
        RenderScene(renderer);
        renderer.EndFrame();
        renderer.Present();

        SDL_Delay(1);
    }

    Cleanup();
    renderer.Shutdown();

    Logger::GetInstance().Info("=== 46_normal_map_test 结束 ===");
    return 0;
}

