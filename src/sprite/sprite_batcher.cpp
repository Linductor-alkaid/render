#include "render/sprite/sprite_batcher.h"
#include "render/logger.h"
#include "render/mesh.h"
#include "render/renderable.h"
#include "render/renderer.h"
#include "render/shader.h"
#include "render/math_utils.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>

namespace Render {

namespace {
constexpr float kEpsilon = 1e-6f;
} // namespace

SpriteBatcher::SpriteBatcher() = default;

SpriteBatcher::~SpriteBatcher() {
    if (m_instanceBuffer != 0) {
        GL_THREAD_CHECK();
        glDeleteBuffers(1, &m_instanceBuffer);
        m_instanceBuffer = 0;
    }
}

void SpriteBatcher::Clear() {
    m_entries.clear();
    m_batches.clear();
}

uint32_t SpriteBatcher::HashMatrix(const Matrix4& matrix) {
    uint32_t hash = 2166136261u;
    const float* data = matrix.data();
    for (int i = 0; i < 16; ++i) {
        uint32_t value;
        std::memcpy(&value, &data[i], sizeof(uint32_t));
        hash ^= value;
        hash *= 16777619u;
    }
    return hash;
}

Vector4 SpriteBatcher::NormalizeUVRect(const Rect& sourceRect, const Ref<Texture>& texture) {
    Rect rect = sourceRect;
    if (!texture) {
        return Vector4(rect.x, rect.y, rect.width, rect.height);
    }

    const float texWidth = static_cast<float>(texture->GetWidth());
    const float texHeight = static_cast<float>(texture->GetHeight());

    float uMin = rect.x;
    float vMin = rect.y;
    float uMax = rect.x + rect.width;
    float vMax = rect.y + rect.height;

    const bool usePixels = (std::fabs(uMin) > 1.0f || std::fabs(vMin) > 1.0f ||
                            std::fabs(uMax) > 1.0f || std::fabs(vMax) > 1.0f);

    if (usePixels) {
        if (texWidth > kEpsilon) {
            uMin /= texWidth;
            uMax /= texWidth;
        }
        if (texHeight > kEpsilon) {
            vMin /= texHeight;
            vMax /= texHeight;
        }
    }

    const float uWidth = std::clamp(uMax - uMin, 0.0f, 1.0f);
    const float vHeight = std::clamp(vMax - vMin, 0.0f, 1.0f);

    return Vector4(
        std::clamp(uMin, 0.0f, 1.0f),
        std::clamp(vMin, 0.0f, 1.0f),
        uWidth <= kEpsilon ? 1.0f : uWidth,
        vHeight <= kEpsilon ? 1.0f : vHeight
    );
}

void SpriteBatcher::AddSprite(const Ref<Texture>& texture,
                              const Rect& sourceRect,
                              const Vector2& size,
                              const Color& tint,
                              const Matrix4& modelMatrix,
                              const Matrix4& viewMatrix,
                              const Matrix4& projectionMatrix,
                              bool screenSpace,
                              uint32_t layer,
                              uint32_t sortOrder,
                              BlendMode blendMode) {
    if (!texture) {
        return;
    }

    SpriteBatcher::SpriteEntry entry{};
    entry.texture = texture;
    entry.key.texturePtr = texture.get();
    entry.key.blendMode = blendMode;
    entry.key.screenSpace = screenSpace;
    entry.key.viewHash = HashMatrix(viewMatrix);
    entry.key.projectionHash = HashMatrix(projectionMatrix);
    entry.key.blendMode = blendMode;

    entry.modelMatrix = modelMatrix;
    entry.viewMatrix = viewMatrix;
    entry.projectionMatrix = projectionMatrix;
    entry.uvRect = NormalizeUVRect(sourceRect, texture);
    entry.tint = tint.ToVector4();
    entry.layer = layer;
    entry.sortOrder = sortOrder;

    m_entries.emplace_back(std::move(entry));
}

void SpriteBatcher::BuildBatches() {
    m_batches.clear();

    if (m_entries.empty()) {
        return;
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const SpriteBatcher::SpriteEntry& a, const SpriteBatcher::SpriteEntry& b) {
        if (a.layer != b.layer) {
            return a.layer < b.layer;
        }
        if (a.sortOrder != b.sortOrder) {
            return a.sortOrder < b.sortOrder;
        }
        if (a.key.texturePtr != b.key.texturePtr) {
            return a.key.texturePtr < b.key.texturePtr;
        }
        if (a.key.viewHash != b.key.viewHash) {
            return a.key.viewHash < b.key.viewHash;
        }
        if (a.key.projectionHash != b.key.projectionHash) {
            return a.key.projectionHash < b.key.projectionHash;
        }
        return false;
    });

    auto appendInstance = [](SpriteDrawBatch& batch, const SpriteEntry& entry) {
        InstancePayload payload{};
        payload.model = entry.modelMatrix;
        payload.uvRect = entry.uvRect;
        payload.tint = entry.tint;
        batch.instances.push_back(payload);
    };

    SpriteDrawBatch currentBatch{};
    const auto& first = m_entries.front();
    currentBatch.key = first.key;
    currentBatch.viewMatrix = first.viewMatrix;
    currentBatch.projectionMatrix = first.projectionMatrix;
    currentBatch.texture = first.texture;
    currentBatch.layer = first.layer;
    currentBatch.sortOrder = first.sortOrder;
    appendInstance(currentBatch, first);

    for (size_t i = 1; i < m_entries.size(); ++i) {
        const auto& entry = m_entries[i];
        const bool sameKey = (entry.key == currentBatch.key);
        const bool sameLayer = entry.layer == currentBatch.layer;

        if (!sameKey || !sameLayer) {
            m_batches.push_back(std::move(currentBatch));
            currentBatch = {};
            currentBatch.key = entry.key;
            currentBatch.viewMatrix = entry.viewMatrix;
            currentBatch.projectionMatrix = entry.projectionMatrix;
            currentBatch.texture = entry.texture;
            currentBatch.layer = entry.layer;
            currentBatch.sortOrder = entry.sortOrder;
        }
        appendInstance(currentBatch, entry);
    }

    m_batches.push_back(std::move(currentBatch));
    m_entries.clear();
}

size_t SpriteBatcher::GetBatchCount() const noexcept {
    return m_batches.size();
}

uint32_t SpriteBatcher::GetBatchLayer(size_t index) const {
    if (index >= m_batches.size()) {
        return 0;
    }
    return m_batches[index].layer;
}

uint32_t SpriteBatcher::GetBatchSortOrder(size_t index) const {
    if (index >= m_batches.size()) {
        return 0;
    }
    return m_batches[index].sortOrder;
}

void SpriteBatcher::DrawBatch(size_t index, RenderState* renderState) {
    if (index >= m_batches.size() || !renderState) {
        return;
    }

    auto& batch = m_batches[index];
    if (batch.instances.empty() || !batch.texture) {
        return;
    }

    Ref<Mesh> quadMesh;
    Ref<Shader> shader;
    if (!SpriteRenderable::AcquireSharedResources(quadMesh, shader) || !quadMesh || !shader || !shader->IsValid()) {
        if (batch.instances.size() > 0) {
            Logger::GetInstance().Warning("[SpriteBatcher] Unable to draw batch due to missing shared resources");
        }
        return;
    }

    auto uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        Logger::GetInstance().Warning("[SpriteBatcher] Sprite shader lacks UniformManager");
        return;
    }

    renderState->SetBlendMode(batch.key.blendMode);
    renderState->SetDepthTest(false);
    renderState->SetDepthWrite(false);
    renderState->SetCullFace(CullFace::None);

    shader->Use();

    if (uniformMgr->HasUniform("uUseTexture")) {
        uniformMgr->SetBool("uUseTexture", true);
    }
    if (uniformMgr->HasUniform("uUseInstancing")) {
        uniformMgr->SetBool("uUseInstancing", true);
    }
    if (uniformMgr->HasUniform("uTintColor")) {
        uniformMgr->SetColor("uTintColor", Color::White());
    }
    if (uniformMgr->HasUniform("uView")) {
        uniformMgr->SetMatrix4("uView", batch.viewMatrix);
    }
    if (uniformMgr->HasUniform("uProjection")) {
        uniformMgr->SetMatrix4("uProjection", batch.projectionMatrix);
    }
    if (uniformMgr->HasUniform("uModel")) {
        uniformMgr->SetMatrix4("uModel", Matrix4::Identity());
    }
    if (uniformMgr->HasUniform("uUVRect")) {
        uniformMgr->SetVector4("uUVRect", Vector4(0.0f, 0.0f, 1.0f, 1.0f));
    }

    batch.texture->Bind(0);

    if (m_instanceBuffer == 0) {
        GL_THREAD_CHECK();
        glGenBuffers(1, &m_instanceBuffer);
    }

    const GLuint vao = quadMesh->GetVertexArrayID();
    if (vao == 0) {
        Logger::GetInstance().Warning("[SpriteBatcher] Invalid quad mesh VAO");
        shader->Unuse();
        return;
    }

    GL_THREAD_CHECK();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(batch.instances.size() * sizeof(InstancePayload)),
                 batch.instances.data(),
                 GL_DYNAMIC_DRAW);

    constexpr GLuint baseLocation = 4;
    const GLsizei stride = sizeof(InstancePayload);

    for (GLuint i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(baseLocation + i);
        glVertexAttribPointer(baseLocation + i, 4, GL_FLOAT, GL_FALSE,
                              stride,
                              reinterpret_cast<void*>(sizeof(float) * 4 * i));
        glVertexAttribDivisor(baseLocation + i, 1);
    }

    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(InstancePayload, uvRect)));
    glVertexAttribDivisor(8, 1);

    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(InstancePayload, tint)));
    glVertexAttribDivisor(9, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    quadMesh->DrawInstanced(static_cast<uint32_t>(batch.instances.size()));

    glBindVertexArray(0);

    if (uniformMgr->HasUniform("uUseInstancing")) {
        uniformMgr->SetBool("uUseInstancing", false);
    }

    shader->Unuse();
}

bool SpriteBatcher::GetBatchInfo(size_t index, SpriteBatchInfo& outInfo) const {
    if (index >= m_batches.size()) {
        return false;
    }

    const auto& batch = m_batches[index];
    outInfo.texture = batch.texture;
    outInfo.blendMode = batch.key.blendMode;
    outInfo.screenSpace = batch.key.screenSpace;
    outInfo.viewHash = batch.key.viewHash;
    outInfo.projectionHash = batch.key.projectionHash;
    outInfo.viewMatrix = batch.viewMatrix;
    outInfo.projectionMatrix = batch.projectionMatrix;
    outInfo.instanceCount = static_cast<uint32_t>(batch.instances.size());
    outInfo.layer = batch.layer;
    outInfo.sortOrder = batch.sortOrder;
    return batch.texture && !batch.instances.empty();
}

SpriteBatchRenderable::SpriteBatchRenderable()
    : Renderable(RenderableType::Sprite) {}

void SpriteBatchRenderable::SetBatch(SpriteBatcher* batcher, size_t batchIndex) {
    m_batcher = batcher;
    m_batchIndex = batchIndex;
}

void SpriteBatchRenderable::Render(RenderState* renderState) {
    if (m_batcher) {
        m_batcher->DrawBatch(m_batchIndex, renderState);
    }
}

void SpriteBatchRenderable::SubmitToRenderer(Renderer* renderer) {
    if (renderer) {
        renderer->SubmitRenderable(this);
    }
}

AABB SpriteBatchRenderable::GetBoundingBox() const {
    return AABB();
}

} // namespace Render


