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
#include "render/sprite/sprite_animator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Render {

SpriteAnimator::SpriteAnimator(Sprite* sprite)
    : m_sprite(sprite)
    , m_currentClipIndex(-1)
    , m_currentFrameIndex(0)
    , m_timeAccumulator(0.0f)
    , m_playbackSpeed(1.0f)
    , m_direction(1) {
}

void SpriteAnimator::SetSprite(Sprite* sprite) {
    m_sprite = sprite;
}

void SpriteAnimator::AddClip(const SpriteAnimationClip& clip) {
    m_clips.push_back(clip);
}

void SpriteAnimator::Play(const std::string& clipName, bool restart) {
    for (size_t i = 0; i < m_clips.size(); ++i) {
        if (m_clips[i].name == clipName) {
            if (static_cast<int>(i) == m_currentClipIndex && !restart) {
                return;
            }
            m_currentClipIndex = static_cast<int>(i);
            m_timeAccumulator = 0.0f;
            m_direction = m_playbackSpeed < 0.0f ? -1 : 1;
            m_currentFrameIndex = (m_direction < 0)
                                       ? static_cast<int>(m_clips[i].frames.empty()
                                                              ? 0
                                                              : m_clips[i].frames.size() - 1)
                                       : 0;
            return;
        }
    }
}

void SpriteAnimator::Stop() {
    m_currentClipIndex = -1;
    m_currentFrameIndex = 0;
    m_timeAccumulator = 0.0f;
}

void SpriteAnimator::Update(float deltaTime) {
    if (!m_sprite || m_currentClipIndex < 0 || m_currentClipIndex >= static_cast<int>(m_clips.size())) {
        return;
    }

    auto& clip = m_clips[m_currentClipIndex];
    if (clip.frames.empty() || clip.frameRate <= 0.0f || std::abs(m_playbackSpeed) <= std::numeric_limits<float>::epsilon()) {
        return;
    }

    const int frameCount = static_cast<int>(clip.frames.size());
    m_timeAccumulator += deltaTime;
    float effectiveRate = clip.frameRate * std::abs(m_playbackSpeed);
    float frameDuration = 1.0f / effectiveRate;
    int direction = (m_playbackSpeed < 0.0f) ? -1 : m_direction;

    auto resolvePingPongFrame = [&](int nextIndex, int dir) -> std::pair<int, int> {
        if (frameCount <= 1) {
            return {0, 0};
        }
        if (dir > 0 && nextIndex >= frameCount) {
            nextIndex = frameCount - 2;
            dir = -1;
        } else if (dir < 0 && nextIndex < 0) {
            nextIndex = frameCount > 1 ? 1 : 0;
            dir = 1;
        }
        return {nextIndex, dir};
    };

    while (m_timeAccumulator >= frameDuration) {
        m_timeAccumulator -= frameDuration;
        m_currentFrameIndex += direction;

        switch (clip.playbackMode) {
        case SpritePlaybackMode::Loop:
            if (direction > 0 && m_currentFrameIndex >= frameCount) {
                m_currentFrameIndex = 0;
            } else if (direction < 0 && m_currentFrameIndex < 0) {
                m_currentFrameIndex = frameCount - 1;
            }
            break;
        case SpritePlaybackMode::Once:
            if (direction > 0 && m_currentFrameIndex >= frameCount) {
                m_currentFrameIndex = frameCount - 1;
                m_timeAccumulator = 0.0f;
                direction = 0;
            } else if (direction < 0 && m_currentFrameIndex < 0) {
                m_currentFrameIndex = 0;
                m_timeAccumulator = 0.0f;
                direction = 0;
            }
            break;
        case SpritePlaybackMode::PingPong: {
            auto [idx, dir] = resolvePingPongFrame(m_currentFrameIndex, direction);
            m_currentFrameIndex = idx;
            direction = dir;
            break;
        }
        }

        if (direction == 0) {
            break;
        }
    }

    m_direction = direction == 0 ? (m_direction == 0 ? 1 : m_direction) : direction;
}

const std::string& SpriteAnimator::GetCurrentClip() const {
    static std::string empty;
    if (m_currentClipIndex < 0 || m_currentClipIndex >= static_cast<int>(m_clips.size())) {
        return empty;
    }
    return m_clips[m_currentClipIndex].name;
}

int SpriteAnimator::GetCurrentFrameIndex() const {
    return m_currentFrameIndex;
}

} // namespace Render


