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


