/**
 * @file 39_sprite_api_test.cpp
 * @brief Sprite API 与即时渲染示例
 */

#include "render/renderer.h"
#include "render/logger.h"
#include "render/sprite/sprite.h"
#include "render/sprite/sprite_renderer.h"
#include "render/texture_loader.h"
#include <SDL3/SDL.h>

using namespace Render;

int main() {
    Logger::GetInstance().Info("=== Sprite API Test ===");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logger::GetInstance().Error(std::string("SDL init failed: ") + SDL_GetError());
        return -1;
    }

    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize("Sprite API Test", 800, 600)) {
        Logger::GetInstance().Error("Renderer initialize failed");
        SDL_Quit();
        return -1;
    }

    auto texture = TextureLoader::GetInstance().LoadTexture("sprite_api_test", "textures/test.jpg", true);
    if (!texture) {
        Logger::GetInstance().Error("Failed to load texture textures/test.jpg");
        renderer->Shutdown();
        SDL_Quit();
        return -1;
    }

    Sprite sprite;
    sprite.SetTexture(texture);
    SpriteFrame frame;
    frame.uv = Rect(0, 0, 1, 1);
    frame.size = Vector2(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()));
    sprite.SetFrame(frame);

    SpriteRenderer spriteRenderer(renderer.get());

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        renderer->BeginFrame();
        renderer->Clear(true, true, false);

        spriteRenderer.Begin();
        spriteRenderer.Draw(sprite, Vector3(400.0f, 300.0f, 0.0f));
        spriteRenderer.End();

        renderer->FlushRenderQueue();
        renderer->EndFrame();
        renderer->Present();

        SDL_Delay(16);
    }

    renderer->Shutdown();
    SDL_Quit();
    return 0;
}


