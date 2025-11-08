#pragma once

#include "render/types.h"
#include "render/texture.h"
#include "render/sprite/sprite_sheet.h"
#include <string>
#include <unordered_map>

namespace Render {

namespace ECS {
struct SpriteAnimationComponent;
}

/**
 * @brief 图集中的单帧数据
 */
struct SpriteAtlasFrame {
    Rect uv{0, 0, 1, 1};              ///< UV 或像素区域
    Vector2 size{1.0f, 1.0f};         ///< 帧尺寸（像素）
    Vector2 pivot{0.5f, 0.5f};        ///< 枢轴
    Vector2 originalSize{1.0f, 1.0f}; ///< 原始尺寸（未裁剪）
    Vector2 offset{0.0f, 0.0f};       ///< 相对于原始尺寸的偏移
    float duration = 0.0f;            ///< 帧时长（可选，0 表示使用动画默认值）
};

/**
 * @brief 图集中的动画定义
 */
struct SpriteAtlasAnimation {
    std::vector<std::string> frames;          ///< 帧名称序列
    float frameDuration = 0.1f;               ///< 默认帧时长
    float playbackSpeed = 1.0f;               ///< 播放速度倍率
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop; ///< 播放模式
};

/**
 * @brief 精灵图集对象
 */
class SpriteAtlas {
public:
    SpriteAtlas() = default;

    void SetName(const std::string& name) { m_name = name; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    void SetTextureName(const std::string& textureName) { m_textureName = textureName; }
    [[nodiscard]] const std::string& GetTextureName() const { return m_textureName; }

    void SetTexture(const Ref<Texture>& texture);
    [[nodiscard]] Ref<Texture> GetTexture() const { return m_texture; }

    void SetTextureSize(int width, int height);
    [[nodiscard]] Vector2 GetTextureSize() const { return m_textureSize; }

    void AddFrame(const std::string& name, const SpriteAtlasFrame& frame);
    [[nodiscard]] bool HasFrame(const std::string& name) const;
    [[nodiscard]] const SpriteAtlasFrame& GetFrame(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<std::string, SpriteAtlasFrame>& GetAllFrames() const {
        return m_frames;
    }

    void AddAnimation(const std::string& name, const SpriteAtlasAnimation& animation);
    [[nodiscard]] bool HasAnimation(const std::string& name) const;
    [[nodiscard]] const SpriteAtlasAnimation& GetAnimation(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<std::string, SpriteAtlasAnimation>& GetAllAnimations() const {
        return m_animations;
    }

    /**
     * @brief 使用图集帧填充 SpriteSheet
     */
    void PopulateSpriteSheet(SpriteSheet& sheet) const;

    /**
     * @brief 使用图集动画填充 SpriteAnimationComponent
     * @param component 目标组件
     * @param defaultClip 默认播放的动画名称（为空则使用第一个）
     * @param autoPlay 是否自动播放
     */
    void PopulateAnimationComponent(ECS::SpriteAnimationComponent& component,
                                    const std::string& defaultClip = std::string(),
                                    bool autoPlay = false) const;

private:
    std::string m_name;
    std::string m_textureName;
    Ref<Texture> m_texture;
    Vector2 m_textureSize{1.0f, 1.0f};
    std::unordered_map<std::string, SpriteAtlasFrame> m_frames;
    std::unordered_map<std::string, SpriteAtlasAnimation> m_animations;
};

using SpriteAtlasPtr = Ref<SpriteAtlas>;

} // namespace Render


