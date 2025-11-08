#include "render/text/font.h"
#include "render/logger.h"
#include "render/texture.h"
#include "render/error.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <mutex>

namespace Render {

namespace {
std::mutex g_ttfMutex;
int g_ttfRefCount = 0;

SDL_Color ToSDLColor(const Color& color) {
    auto clamp = [](float v) {
        return static_cast<Uint8>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    SDL_Color result{};
    result.r = clamp(color.r);
    result.g = clamp(color.g);
    result.b = clamp(color.b);
    result.a = clamp(color.a);
    return result;
}
} // namespace

Font::Font()
    : m_font(nullptr)
    , m_pointSize(0.0f) {
}

Font::~Font() {
    Close();
}

bool Font::AcquireTTF() {
    std::lock_guard<std::mutex> lock(g_ttfMutex);
    if (g_ttfRefCount == 0) {
        if (!TTF_Init()) {
            Logger::GetInstance().ErrorFormat("[Font] TTF_Init failed: %s", SDL_GetError());
            return false;
        }
    }
    ++g_ttfRefCount;
    return true;
}

void Font::ReleaseTTF() {
    std::lock_guard<std::mutex> lock(g_ttfMutex);
    if (g_ttfRefCount > 0) {
        --g_ttfRefCount;
        if (g_ttfRefCount == 0) {
            TTF_Quit();
        }
    }
}

bool Font::LoadFromFile(const std::string& filepath, float pointSize) {
    if (filepath.empty()) {
        Logger::GetInstance().Error("[Font] LoadFromFile received empty path");
        return false;
    }
    if (pointSize <= 0.0f) {
        Logger::GetInstance().ErrorFormat("[Font] Invalid point size: %.2f", pointSize);
        return false;
    }

    if (!AcquireTTF()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

    TTF_Font* font = TTF_OpenFont(filepath.c_str(), pointSize);
    if (!font) {
        Logger::GetInstance().ErrorFormat("[Font] Failed to open font '%s': %s",
                                          filepath.c_str(),
                                          SDL_GetError());
        ReleaseTTF();
        return false;
    }

    m_font = font;
    m_filepath = filepath;
    m_pointSize = pointSize;
    return true;
}

void Font::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
        ReleaseTTF();
    }
    ClearCache();
    m_filepath.clear();
    m_pointSize = 0.0f;
}

void Font::ClearCache() {
    m_renderCache.clear();
    m_cacheUsage.clear();
}

bool Font::IsValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_font != nullptr;
}

int Font::GetAscent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_font ? TTF_GetFontAscent(m_font) : 0;
}

int Font::GetDescent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_font ? TTF_GetFontDescent(m_font) : 0;
}

int Font::GetHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_font ? TTF_GetFontHeight(m_font) : 0;
}

int Font::GetLineSkip() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_font ? TTF_GetFontLineSkip(m_font) : 0;
}

RasterizedText Font::RenderText(const std::string& text, int wrapWidth) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    RasterizedText result;
    if (!m_font) {
        Logger::GetInstance().Warning("[Font] RenderText called on invalid font");
        return result;
    }

    std::string cacheKey = text;
    cacheKey.push_back('\n');
    cacheKey.append(std::to_string(wrapWidth));

    auto cacheIt = m_renderCache.find(cacheKey);
    if (cacheIt != m_renderCache.end()) {
        result.texture = cacheIt->second.texture;
        result.size = cacheIt->second.size;
        TouchCacheKey(cacheKey);
        return result;
    }

    result = RenderInternal(text, wrapWidth);

    if (result.texture || !text.empty()) {
        CacheEntry entry;
        entry.texture = result.texture;
        entry.size = result.size;
        m_renderCache[cacheKey] = entry;
        TouchCacheKey(cacheKey);
        while (m_cacheUsage.size() > kMaxCacheEntries) {
            const std::string& oldKey = m_cacheUsage.back();
            m_renderCache.erase(oldKey);
            m_cacheUsage.pop_back();
        }
    }

    return result;
}

void Font::TouchCacheKey(const std::string& key) const {
    auto it = std::find(m_cacheUsage.begin(), m_cacheUsage.end(), key);
    if (it != m_cacheUsage.end()) {
        m_cacheUsage.erase(it);
    }
    m_cacheUsage.push_front(key);
}

RasterizedText Font::RenderInternal(const std::string& text, int wrapWidth) const {
    RasterizedText result;

    if (!m_font) {
        Logger::GetInstance().Warning("[Font] RenderText called on invalid font");
        return result;
    }

    if (text.empty()) {
        result.size = Vector2(0.0f, static_cast<float>(TTF_GetFontLineSkip(m_font)));
        return result;
    }

    const SDL_Color color = ToSDLColor(Color::White());
    SDL_Surface* surface = nullptr;

    const char* raw = text.c_str();
    const size_t length = text.size();

    if (wrapWidth > 0) {
        surface = TTF_RenderText_Blended_Wrapped(m_font, raw, length, color, wrapWidth);
    } else {
        surface = TTF_RenderText_Blended(m_font, raw, length, color);
    }

    if (!surface) {
        Logger::GetInstance().ErrorFormat("[Font] Failed to render text: %s", SDL_GetError());
        return result;
    }

    SDL_Surface* converted = surface;
    if (SDL_PIXELFORMAT_RGBA32 != surface->format) {
        converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!converted) {
            Logger::GetInstance().ErrorFormat("[Font] Failed to convert surface: %s", SDL_GetError());
            return result;
        }
    }

    Vector2 textureSize{static_cast<float>(converted->w), static_cast<float>(converted->h)};
    Ref<Texture> texture = CreateTextureFromSurface(converted, textureSize);
    SDL_DestroySurface(converted);

    if (!texture) {
        return result;
    }

    result.texture = std::move(texture);
    result.size = textureSize;
    return result;
}

Ref<Texture> Font::CreateTextureFromSurface(SDL_Surface* surface, Vector2& outSize) const {
    if (!surface) {
        return nullptr;
    }

    Ref<Texture> texture = CreateRef<Texture>();
    if (!texture->CreateFromData(surface->pixels,
                                 surface->w,
                                 surface->h,
                                 TextureFormat::RGBA,
                                 false)) {
        Logger::GetInstance().Error("[Font] Failed to create texture from rendered text");
        return nullptr;
    }

    texture->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    texture->SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
    outSize = Vector2(static_cast<float>(surface->w), static_cast<float>(surface->h));
    return texture;
}

} // namespace Render


