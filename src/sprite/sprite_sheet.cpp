#include "render/sprite/sprite_sheet.h"
#include "render/error.h"

namespace Render {

SpriteSheet::SpriteSheet()
    : m_texture(nullptr) {
}

void SpriteSheet::SetTexture(const Ref<Texture>& texture) {
    m_texture = texture;
}

Ref<Texture> SpriteSheet::GetTexture() const {
    return m_texture;
}

void SpriteSheet::AddFrame(const std::string& name, const SpriteFrame& frame) {
    m_frames[name] = frame;
}

bool SpriteSheet::HasFrame(const std::string& name) const {
    return m_frames.find(name) != m_frames.end();
}

const SpriteFrame& SpriteSheet::GetFrame(const std::string& name) const {
    auto it = m_frames.find(name);
    if (it == m_frames.end()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::ResourceNotFound, "SpriteSheet: frame not found: " + name));
        static SpriteFrame empty{Rect(0, 0, 1, 1), Vector2(1.0f, 1.0f), Vector2(0.5f, 0.5f)};
        return empty;
    }
    return it->second;
}

const std::unordered_map<std::string, SpriteFrame>& SpriteSheet::GetAllFrames() const {
    return m_frames;
}

} // namespace Render


