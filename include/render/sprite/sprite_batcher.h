#pragma once

#include "render/types.h"
#include "render/renderable.h"
#include "render/texture.h"
#include "render/render_state.h"
#include <vector>
#include <cstdint>

namespace Render {

class Renderer;

class SpriteBatcher {
public:
    SpriteBatcher();
    ~SpriteBatcher();

    void Clear();

    void AddSprite(const Ref<Texture>& texture,
                   const Rect& sourceRect,
                   const Vector2& size,
                   const Color& tint,
                   const Matrix4& modelMatrix,
                   const Matrix4& viewMatrix,
                   const Matrix4& projectionMatrix,
                   bool screenSpace,
                   uint32_t layer,
                   int32_t sortOrder,
                   BlendMode blendMode = BlendMode::Alpha);

    void BuildBatches();
    [[nodiscard]] size_t GetBatchCount() const noexcept;
    [[nodiscard]] uint32_t GetBatchLayer(size_t index) const;
    [[nodiscard]] int32_t GetBatchSortOrder(size_t index) const;
    void DrawBatch(size_t index, RenderState* renderState);

    struct SpriteBatchInfo {
        Ref<Texture> texture;
        BlendMode blendMode = BlendMode::Alpha;
        bool screenSpace = true;
        uint32_t viewHash = 0;
        uint32_t projectionHash = 0;
        Matrix4 viewMatrix = Matrix4::Identity();
        Matrix4 projectionMatrix = Matrix4::Identity();
        uint32_t instanceCount = 0;
        uint32_t layer = 0;
        int32_t sortOrder = 0;
    };

    [[nodiscard]] bool GetBatchInfo(size_t index, SpriteBatchInfo& outInfo) const;

private:
    struct SpriteBatchKey {
        Texture* texturePtr = nullptr;
        BlendMode blendMode = BlendMode::Alpha;
        bool screenSpace = true;
        uint32_t viewHash = 0;
        uint32_t projectionHash = 0;
        uint32_t layer = 0;

        bool operator==(const SpriteBatchKey& other) const noexcept {
            return texturePtr == other.texturePtr &&
                   blendMode == other.blendMode &&
                   screenSpace == other.screenSpace &&
                   viewHash == other.viewHash &&
                   projectionHash == other.projectionHash &&
                   layer == other.layer;
        }
    };

    struct SpriteEntry {
        SpriteBatchKey key;
        Matrix4 modelMatrix = Matrix4::Identity();
        Matrix4 viewMatrix = Matrix4::Identity();
        Matrix4 projectionMatrix = Matrix4::Identity();
        Vector4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
        Vector4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        Ref<Texture> texture;
        uint32_t layer = 0;
        int32_t sortOrder = 0;
    };

    struct InstancePayload {
        Matrix4 model;
        Vector4 uvRect;
        Vector4 tint;
    };

    struct SpriteDrawBatch {
        SpriteBatchKey key;
        Matrix4 viewMatrix = Matrix4::Identity();
        Matrix4 projectionMatrix = Matrix4::Identity();
        Ref<Texture> texture;
        std::vector<InstancePayload> instances;
        uint32_t layer = 0;
        int32_t sortOrder = 0;
    };

    static uint32_t HashMatrix(const Matrix4& matrix);
    static Vector4 NormalizeUVRect(const Rect& sourceRect, const Ref<Texture>& texture);

    GLuint m_instanceBuffer = 0;
    std::vector<SpriteEntry> m_entries;
    std::vector<SpriteDrawBatch> m_batches;
};

class SpriteBatchRenderable : public Renderable {
public:
    SpriteBatchRenderable();
    SpriteBatchRenderable(SpriteBatchRenderable&&) noexcept = default;
    SpriteBatchRenderable& operator=(SpriteBatchRenderable&&) noexcept = default;
    SpriteBatchRenderable(const SpriteBatchRenderable&) = delete;
    SpriteBatchRenderable& operator=(const SpriteBatchRenderable&) = delete;

    void SetBatch(SpriteBatcher* batcher, size_t batchIndex);

    void Render(RenderState* renderState) override;
    void SubmitToRenderer(Renderer* renderer) override;
    [[nodiscard]] AABB GetBoundingBox() const override;
    [[nodiscard]] SpriteBatcher* GetBatcher() const { return m_batcher; }
    [[nodiscard]] size_t GetBatchIndex() const { return m_batchIndex; }

private:
    SpriteBatcher* m_batcher = nullptr;
    size_t m_batchIndex = 0;
};

} // namespace Render


