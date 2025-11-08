#pragma once

#include "render/types.h"
#include <memory>
#include <mutex>
#include <string>
#include <deque>
#include <unordered_map>

struct SDL_Surface;
struct TTF_Font;

namespace Render {

class Texture;

/**
 * @brief 字体光栅化结果
 */
struct RasterizedText {
    Ref<Texture> texture;   ///< 生成的纹理
    Vector2 size = Vector2::Zero(); ///< 文本纹理尺寸（像素）
};

/**
 * @brief TrueType 字体封装
 *
 * - 使用 SDL_ttf 进行字体加载和文本光栅化
 * - 线程安全：所有公共方法均加锁保护
 * - 纹理创建遵循引擎 Texture 管理约定
 */
class Font : public std::enable_shared_from_this<Font> {
public:
    Font();
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    /**
     * @brief 从文件加载字体
     * @param filepath 字体文件路径（TTF/OTF）
     * @param pointSize 字号（磅值）
     * @return 加载成功返回 true
     */
    bool LoadFromFile(const std::string& filepath, float pointSize);

    /**
     * @brief 释放字体资源
     */
    void Close();

    /**
     * @brief 字体是否已成功加载
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief 获取字体文件路径
     */
    [[nodiscard]] const std::string& GetFilePath() const { return m_filepath; }

    /**
     * @brief 获取字号
     */
    [[nodiscard]] float GetPointSize() const { return m_pointSize; }

    /**
     * @brief 获取字体上升高度（像素）
     */
    [[nodiscard]] int GetAscent() const;

    /**
     * @brief 获取字体下降高度（像素）
     */
    [[nodiscard]] int GetDescent() const;

    /**
     * @brief 获取字体总高度（像素）
     */
    [[nodiscard]] int GetHeight() const;

    /**
     * @brief 获取行距（像素）
     */
    [[nodiscard]] int GetLineSkip() const;

    /**
     * @brief 光栅化 UTF-8 文本到纹理
     * @param text UTF-8 文本
     * @param wrapWidth 文本换行宽度（像素），0 表示不换行
     * @return 光栅化结果（纹理 + 尺寸），失败时 texture 为空
     */
    [[nodiscard]] RasterizedText RenderText(const std::string& text, int wrapWidth = 0) const;

private:
    [[nodiscard]] RasterizedText RenderInternal(const std::string& text, int wrapWidth) const;
    [[nodiscard]] Ref<Texture> CreateTextureFromSurface(SDL_Surface* surface, Vector2& outSize) const;

    static bool AcquireTTF();
    static void ReleaseTTF();
    void ClearCache();
    void TouchCacheKey(const std::string& key) const;

    struct CacheEntry {
        Ref<Texture> texture;
        Vector2 size{0.0f, 0.0f};
    };

    mutable std::unordered_map<std::string, CacheEntry> m_renderCache;
    mutable std::deque<std::string> m_cacheUsage;
    static constexpr size_t kMaxCacheEntries = 64;

    mutable std::mutex m_mutex;
    TTF_Font* m_font;
    std::string m_filepath;
    float m_pointSize;
};

using FontPtr = Ref<Font>;

} // namespace Render


