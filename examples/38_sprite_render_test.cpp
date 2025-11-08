/**
 * @file 38_sprite_render_test.cpp
 * @brief Sprite 渲染系统示例程序
 *
 * 演示内容：
 * - 初始化 Renderer 与 ECS World
 * - 创建 SpriteRenderSystem 并提交精灵实体
 * - 加载纹理并展示多种 sourceRect / tint 效果
 */

#include <SDL3/SDL.h>
#include <iostream>
#include <vector>

#include "render/renderer.h"
#include "render/logger.h"
#include "render/render_state.h"
#include "render/texture_loader.h"
#include "render/resource_manager.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"
#include "render/ecs/systems.h"

using namespace Render;
using namespace Render::ECS;

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr float kDemoDurationSeconds = 6.0f;

Ref<Texture> LoadOrCreateTestTexture() {
    auto& textureLoader = TextureLoader::GetInstance();

    // 尝试加载现有测试资源
    auto texture = textureLoader.LoadTexture("sprite_demo", "textures/test.jpg", true);
    if (texture) {
        return texture;
    }

    texture = textureLoader.LoadTexture("sprite_demo", "textures/test.png", true);
    if (texture) {
        return texture;
    }

    // 兜底：生成棋盘格纹理
    const int texWidth = 256;
    const int texHeight = 256;
    std::vector<unsigned char> checkerboard(texWidth * texHeight * 4);

    for (int y = 0; y < texHeight; ++y) {
        for (int x = 0; x < texWidth; ++x) {
            int index = (y * texWidth + x) * 4;
            bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;
            unsigned char color = isWhite ? 240 : 90;

            checkerboard[index + 0] = color;
            checkerboard[index + 1] = color;
            checkerboard[index + 2] = color;
            checkerboard[index + 3] = 255;
        }
    }

    texture = textureLoader.CreateTexture("sprite_checkerboard",
                                          checkerboard.data(),
                                          texWidth,
                                          texHeight,
                                          TextureFormat::RGBA,
                                          true);
    return texture;
}

SpriteRenderComponent CreateSpriteComponent(const Ref<Texture>& texture,
                                            const Vector2& size,
                                            const Rect& sourceRect,
                                            const Color& tint) {
    SpriteRenderComponent sprite;
    sprite.texture = texture;
    sprite.resourcesLoaded = static_cast<bool>(texture);
    sprite.asyncLoading = false;
    sprite.size = size;
    sprite.sourceRect = sourceRect;
    sprite.tintColor = tint;
    sprite.visible = true;
    sprite.layerID = 850; // UI 顶层
    return sprite;
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToFile(true);
    Logger::GetInstance().Info("=== Sprite Render Test ===");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logger::GetInstance().Error(std::string("初始化 SDL 失败: ") + SDL_GetError());
        return -1;
    }

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite Render Test", kWindowWidth, kWindowHeight)) {
        Logger::GetInstance().Error("Renderer 初始化失败");
        SDL_Quit();
        return -1;
    }

    renderer->SetClearColor(Color(0.08f, 0.09f, 0.12f, 1.0f));

    auto world = std::make_shared<World>();
    world->Initialize();

    world->RegisterComponent<TransformComponent>();
    world->RegisterComponent<SpriteRenderComponent>();

    world->RegisterSystem<TransformSystem>();
    world->RegisterSystem<SpriteRenderSystem>(renderer.get());
    world->PostInitialize();

    auto texture = LoadOrCreateTestTexture();
    if (!texture) {
        Logger::GetInstance().Error("无法创建或加载测试纹理");
        world->Shutdown();
        renderer->Shutdown();
        SDL_Quit();
        return -1;
    }

    Logger::GetInstance().InfoFormat("Sprite 纹理尺寸: %dx%d",
                                     texture->GetWidth(), texture->GetHeight());

    std::vector<EntityID> sprites;
    sprites.reserve(3);

    // 居中主精灵
    EntityID centerSprite = world->CreateEntity({.name = "CenterSprite"});
    TransformComponent centerTransform;
    centerTransform.SetPosition(Vector3(kWindowWidth * 0.5f, kWindowHeight * 0.5f, 0.0f));
    world->AddComponent(centerSprite, centerTransform);
    world->AddComponent(centerSprite, CreateSpriteComponent(
        texture,
        Vector2(256.0f, 256.0f),
        Rect(0.0f, 0.0f, 1.0f, 1.0f),
        Color(1.0f, 1.0f, 1.0f, 1.0f)));
    sprites.push_back(centerSprite);

    // 右侧：裁剪 1/4 区域
    EntityID croppedSprite = world->CreateEntity({.name = "CroppedSprite"});
    TransformComponent croppedTransform;
    croppedTransform.SetPosition(Vector3(kWindowWidth * 0.75f, kWindowHeight * 0.55f, 0.0f));
    world->AddComponent(croppedSprite, croppedTransform);
    world->AddComponent(croppedSprite, CreateSpriteComponent(
        texture,
        Vector2(192.0f, 192.0f),
        Rect(0.0f, 0.0f, 0.5f, 0.5f),
        Color(1.0f, 0.7f, 0.7f, 0.85f)));
    sprites.push_back(croppedSprite);

    // 左侧：小尺寸 + 半透明叠加
    EntityID tintSprite = world->CreateEntity({.name = "TintSprite"});
    TransformComponent tintTransform;
    tintTransform.SetPosition(Vector3(kWindowWidth * 0.25f, kWindowHeight * 0.45f, 0.0f));
    world->AddComponent(tintSprite, tintTransform);
    world->AddComponent(tintSprite, CreateSpriteComponent(
        texture,
        Vector2(160.0f, 160.0f),
        Rect(0.25f, 0.25f, 0.5f, 0.5f),
        Color(0.6f, 0.9f, 1.0f, 0.6f)));
    sprites.push_back(tintSprite);

    bool running = true;
    float elapsedSeconds = 0.0f;

    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 lastCounter = SDL_GetPerformanceCounter();

    while (running && elapsedSeconds < kDemoDurationSeconds) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        Uint64 currentCounter = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>(currentCounter - lastCounter) / frequency;
        lastCounter = currentCounter;
        elapsedSeconds += deltaTime;

        renderer->BeginFrame();
        renderer->Clear(true, true, false);

        world->Update(deltaTime);

        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(16);
    }

    Logger::GetInstance().Info("Sprite 渲染示例结束");

    world->Shutdown();
    renderer->Shutdown();
    SDL_Quit();
    return 0;
}


