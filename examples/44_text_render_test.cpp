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
#include "render/renderer.h"
#include "render/logger.h"
#include "render/resource_manager.h"
#include "render/text/font.h"
#include "render/text/text.h"
#include "render/text/text_renderer.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <string>

using namespace Render;

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowTitle[] = "44 - Text Render Test";
constexpr char kFontPath[] = "assets/fonts/NotoSansSC-Regular.ttf";

Color AnimatedColor(float t) {
    // 简单的彩虹循环
    const float speed = 0.75f;
    float phase = t * speed;
    float r = 0.6f + 0.4f * std::sin(phase * 2.0f + 0.0f);
    float g = 0.6f + 0.4f * std::sin(phase * 2.0f + 2.0f);
    float b = 0.6f + 0.4f * std::sin(phase * 2.0f + 4.0f);
    return Color(r, g, b, 1.0f);
}

Vector3 CenterTop(const TextPtr& text, float y)
{
    text->EnsureUpdated();
    float x = kWindowWidth * 0.5f;
    return Vector3(x, y, 0.0f);
}

Vector3 CenterBottom(const TextPtr& text, float bottomMargin)
{
    text->EnsureUpdated();
    Vector2 size = text->GetSize();
    float x = kWindowWidth * 0.5f;
    float y = kWindowHeight - size.y() - bottomMargin;
    return Vector3(x, y, 0.0f);
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Logger::GetInstance().SetLogToConsole(true);
    Logger::GetInstance().SetLogToFile(false);
    Logger::GetInstance().SetLogLevel(LogLevel::Debug);

    LOG_INFO("=== Text Render Test ===");

    Renderer* renderer = Renderer::Create();
    if (!renderer) {
        LOG_ERROR("无法创建 Renderer 实例");
        return -1;
    }

    if (!renderer->Initialize(kWindowTitle, kWindowWidth, kWindowHeight)) {
        LOG_ERROR("Renderer 初始化失败");
        Renderer::Destroy(renderer);
        return -1;
    }

    renderer->SetBatchingMode(BatchingMode::CpuMerge);
    renderer->SetVSync(true);
    renderer->SetClearColor(Color(0.08f, 0.10f, 0.14f, 1.0f));

    FontPtr font = CreateRef<Font>();
    if (!font->LoadFromFile(kFontPath, 32.0f)) {
        LOG_ERROR(std::string("字体加载失败: ") + kFontPath);
        Renderer::Destroy(renderer);
        return -1;
    }

    ResourceManager::GetInstance().RegisterFont("ui.default", font);

    TextPtr headline = CreateRef<Text>(font);
    headline->SetString("RenderEngine 文本渲染示例\n"
                        "• 支持 UTF-8 字符\n"
                        "• 自动换行（SetWrapWidth）\n"
                        "• 动态颜色与实时更新");
    headline->SetWrapWidth(720);
    headline->SetColor(Color::White());
    headline->SetAlignment(TextAlignment::Center);

    TextPtr footer = CreateRef<Text>(font);
    footer->SetString("按 ESC 退出，文本颜色随时间变换。");
    footer->SetColor(Color(0.8f, 0.85f, 0.9f, 1.0f));
    footer->SetAlignment(TextAlignment::Center);

    TextRenderer textRenderer(renderer);

    bool running = true;
    float timeAccumulator = 0.0f;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        float deltaTime = renderer->GetDeltaTime();
        timeAccumulator += deltaTime;

        headline->SetColor(AnimatedColor(timeAccumulator));

        renderer->BeginFrame();
        renderer->Clear();

        textRenderer.Begin();
        textRenderer.Draw(headline, CenterTop(headline, 80.0f));
        textRenderer.Draw(footer, CenterBottom(footer, 60.0f));
        textRenderer.End();

        renderer->EndFrame();
        renderer->Present();
    }

    Renderer::Destroy(renderer);
    LOG_INFO("Text Render Test exiting.");
    return 0;
}


