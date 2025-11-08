#pragma once

#include "render/types.h"
#include "render/texture.h"
#include <memory>
#include <vector>
#include <string>

namespace Render {

struct SpriteFrame {
    Rect uv;
    Vector2 size;
    Vector2 pivot;
};

class Sprite {
public:
    Sprite();

    void SetTexture(const Ref<Texture>& texture);
    [[nodiscard]] Ref<Texture> GetTexture() const;

    void SetFrame(const SpriteFrame& frame);
    [[nodiscard]] const SpriteFrame& GetFrame() const;

    void SetTint(const Color& color);
    [[nodiscard]] Color GetTint() const;

    void SetFlip(bool flipX, bool flipY);
    [[nodiscard]] bool IsFlipX() const;
    [[nodiscard]] bool IsFlipY() const;

    void SetUserData(int userData);
    [[nodiscard]] int GetUserData() const;

private:
    Ref<Texture> m_texture;
    SpriteFrame m_frame{};
    Color m_tint{1.0f, 1.0f, 1.0f, 1.0f};
    bool m_flipX = false;
    bool m_flipY = false;
    int m_userData = 0;
};

} // namespace Render


