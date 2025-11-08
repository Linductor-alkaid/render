#include "render/sprite/sprite_atlas.h"
#include "render/error.h"
#include "render/ecs/components.h"
#include <algorithm>

namespace Render {

void SpriteAtlas::SetTexture(const Ref<Texture>& texture) {
    m_texture = texture;
    if (m_texture) {
        m_textureSize = Vector2(static_cast<float>(m_texture->GetWidth()),
                                static_cast<float>(m_texture->GetHeight()));
    }
}

void SpriteAtlas::SetTextureSize(int width, int height) {
    m_textureSize = Vector2(static_cast<float>(std::max(width, 1)),
                            static_cast<float>(std::max(height, 1)));
}

void SpriteAtlas::AddFrame(const std::string& name, const SpriteAtlasFrame& frame) {
    m_frames[name] = frame;
}

bool SpriteAtlas::HasFrame(const std::string& name) const {
    return m_frames.find(name) != m_frames.end();
}

const SpriteAtlasFrame& SpriteAtlas::GetFrame(const std::string& name) const {
    auto it = m_frames.find(name);
    if (it == m_frames.end()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceNotFound,
                   "SpriteAtlas: frame not found: " + name));
        static SpriteAtlasFrame fallback{};
        return fallback;
    }
    return it->second;
}

void SpriteAtlas::AddAnimation(const std::string& name, const SpriteAtlasAnimation& animation) {
    m_animations[name] = animation;
}

bool SpriteAtlas::HasAnimation(const std::string& name) const {
    return m_animations.find(name) != m_animations.end();
}

const SpriteAtlasAnimation& SpriteAtlas::GetAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    if (it == m_animations.end()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceNotFound,
                   "SpriteAtlas: animation not found: " + name));
        static SpriteAtlasAnimation fallback{};
        return fallback;
    }
    return it->second;
}

void SpriteAtlas::PopulateSpriteSheet(SpriteSheet& sheet) const {
    sheet.SetTexture(m_texture);

    for (const auto& [name, atlasFrame] : m_frames) {
        SpriteFrame spriteFrame;
        spriteFrame.uv = atlasFrame.uv;
        spriteFrame.size = atlasFrame.size;
        spriteFrame.pivot = atlasFrame.pivot;
        sheet.AddFrame(name, spriteFrame);
    }
}

void SpriteAtlas::PopulateAnimationComponent(ECS::SpriteAnimationComponent& component,
                                             const std::string& defaultClip,
                                             bool autoPlay) const {
    component.clips.clear();

    for (const auto& [name, atlasAnim] : m_animations) {
        ECS::SpriteAnimationClip clip;
        clip.frameDuration = atlasAnim.frameDuration;
        clip.playbackMode = atlasAnim.playbackMode;
        clip.loop = atlasAnim.playbackMode == SpritePlaybackMode::Loop;

        clip.frames.reserve(atlasAnim.frames.size());
        for (const auto& frameKey : atlasAnim.frames) {
            if (!HasFrame(frameKey)) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::ResourceNotFound,
                    "SpriteAtlas: animation '" + name + "' references missing frame '" + frameKey + "'"));
                continue;
            }
            const auto& frame = GetFrame(frameKey);
            clip.frames.push_back(frame.uv);
        }

        component.clips[name] = clip;
    }

    if (!component.clips.empty()) {
        const std::string selectedClip =
            !defaultClip.empty() && component.HasClip(defaultClip)
                ? defaultClip
                : component.clips.begin()->first;

        if (component.HasClip(selectedClip)) {
            if (autoPlay) {
                component.Play(selectedClip, true);
            } else {
                component.currentClip = selectedClip;
                component.currentFrame = 0;
                component.playing = false;
                component.dirty = true;
            }
        }
    }
}

} // namespace Render


