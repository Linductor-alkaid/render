#pragma once

#include "render/types.h"
#include "render/text/font.h"
#include <memory>
#include <shared_mutex>
#include <string>

namespace Render {

enum class TextAlignment {
    Left,
    Center,
    Right
};

/**
 * @brief 文本对象封装
 *
 * 提供对字体、文本内容、颜色和换行宽度的管理，
 * 并在必要时延迟生成对应的纹理。
 */
class Text : public std::enable_shared_from_this<Text> {
public:
    Text();
    explicit Text(const FontPtr& font);

    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;

    /**
     * @brief 设置字体
     */
    void SetFont(const FontPtr& font);

    /**
     * @brief 获取字体
     */
    [[nodiscard]] FontPtr GetFont() const;

    /**
     * @brief 设置 UTF-8 文本内容
     */
    void SetString(const std::string& text);

    /**
     * @brief 获取文本内容
     */
    [[nodiscard]] const std::string& GetString() const;

    /**
     * @brief 设置文本颜色
     */
    void SetColor(const Color& color);

    /**
     * @brief 获取文本颜色
     */
    [[nodiscard]] Color GetColor() const;

    /**
     * @brief 设置换行宽度（像素），0 表示不换行
     */
    void SetWrapWidth(int wrapWidth);

    /**
     * @brief 获取换行宽度（像素）
     */
    [[nodiscard]] int GetWrapWidth() const;

    /**
     * @brief 设置文本对齐方式
     */
    void SetAlignment(TextAlignment alignment);

    /**
     * @brief 获取文本对齐方式
     */
    [[nodiscard]] TextAlignment GetAlignment() const;

    /**
     * @brief 确保文本纹理已与最新内容同步
     * @return 成功生成或无需更新则返回 true
     */
    bool EnsureUpdated() const;

    /**
     * @brief 获取文本纹理
     */
    [[nodiscard]] Ref<Texture> GetTexture() const;

    /**
     * @brief 获取文本纹理尺寸（像素）
     */
    [[nodiscard]] Vector2 GetSize() const;

    /**
     * @brief 标记文本为脏，需要重新生成纹理
     */
    void MarkDirty() const;

private:
    bool UpdateTexture() const;

private:
    FontPtr m_font;
    mutable std::shared_mutex m_mutex;
    std::string m_text;
    Color m_color;
    int m_wrapWidth;
    mutable bool m_dirty;
    mutable Ref<Texture> m_texture;
    mutable Vector2 m_textureSize;
    TextAlignment m_alignment;
};

using TextPtr = Ref<Text>;

} // namespace Render


