#pragma once

#include "render/sprite/sprite_sheet.h"
#include <string>
#include <vector>

namespace Render {

struct SpriteAnimationClip {
    std::string name;
    std::vector<std::string> frames;
    float frameRate = 12.0f;
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop;
};

class SpriteAnimator {
public:
    explicit SpriteAnimator(Sprite* sprite = nullptr);

    void SetSprite(Sprite* sprite);
    void AddClip(const SpriteAnimationClip& clip);

    void Play(const std::string& clipName, bool restart = false);
    void Stop();

    void Update(float deltaTime);

    [[nodiscard]] const std::string& GetCurrentClip() const;
    [[nodiscard]] int GetCurrentFrameIndex() const;

    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }

private:
    Sprite* m_sprite;
    std::vector<SpriteAnimationClip> m_clips;
    int m_currentClipIndex;
    int m_currentFrameIndex;
    float m_timeAccumulator;
    float m_playbackSpeed = 1.0f;
    int m_direction = 1; // 1 正向, -1 反向
};

} // namespace Render


