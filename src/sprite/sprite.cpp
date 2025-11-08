#include "render/sprite/sprite.h"

namespace Render {

Sprite::Sprite()
    : m_frame{Rect(0, 0, 1, 1), Vector2(1.0f, 1.0f), Vector2(0.5f, 0.5f)} {
}

void Sprite::SetTexture(const Ref<Texture>& texture) {
    m_texture = texture;
}

Ref<Texture> Sprite::GetTexture() const {
    return m_texture;
}

void Sprite::SetFrame(const SpriteFrame& frame) {
    m_frame = frame;
}

const SpriteFrame& Sprite::GetFrame() const {
    return m_frame;
}

void Sprite::SetTint(const Color& color) {
    m_tint = color;
}

Color Sprite::GetTint() const {
    return m_tint;
}

void Sprite::SetFlip(bool flipX, bool flipY) {
    m_flipX = flipX;
    m_flipY = flipY;
}

bool Sprite::IsFlipX() const {
    return m_flipX;
}

bool Sprite::IsFlipY() const {
    return m_flipY;
}

void Sprite::SetUserData(int userData) {
    m_userData = userData;
}

int Sprite::GetUserData() const {
    return m_userData;
}

} // namespace Render


