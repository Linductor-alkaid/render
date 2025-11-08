#pragma once

#include "render/sprite/sprite.h"
#include <unordered_map>

namespace Render {

class SpriteSheet {
public:
    SpriteSheet();

    void SetTexture(const Ref<Texture>& texture);
    [[nodiscard]] Ref<Texture> GetTexture() const;

    void AddFrame(const std::string& name, const SpriteFrame& frame);
    [[nodiscard]] bool HasFrame(const std::string& name) const;
    [[nodiscard]] const SpriteFrame& GetFrame(const std::string& name) const;

    [[nodiscard]] const std::unordered_map<std::string, SpriteFrame>& GetAllFrames() const;

private:
    Ref<Texture> m_texture;
    std::unordered_map<std::string, SpriteFrame> m_frames;
};

} // namespace Render


