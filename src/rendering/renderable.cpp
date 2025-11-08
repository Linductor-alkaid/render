#include "render/renderable.h"
#include "render/renderer.h"
#include "render/shader.h"
#include "render/logger.h"
#include "render/render_state.h"
#include "render/resource_manager.h"
#include "render/mesh_loader.h"
#include "render/shader_cache.h"
#include "render/math_utils.h"
#include "render/material_state_cache.h"
#include "render/text/text.h"

#include <algorithm>
#include <mutex>
#include <cstdint>
#include <functional>
#include <cstring>

namespace Render {

namespace {

constexpr const char* kSpriteMeshResourceName = "__engine_sprite_quad";
constexpr const char* kSpriteShaderResourceName = "__engine_sprite_shader";
constexpr const char* kSpriteShaderVertPath = "shaders/sprite.vert";
constexpr const char* kSpriteShaderFragPath = "shaders/sprite.frag";

constexpr const char* kTextMeshResourceName = "__engine_text_quad";
constexpr const char* kTextShaderResourceName = "__engine_text_shader";
constexpr const char* kTextShaderVertPath = "shaders/text.vert";
constexpr const char* kTextShaderFragPath = "shaders/text.frag";

struct SpriteSharedResources {
    std::mutex mutex;
    Ref<Mesh> quadMesh;
    Ref<Shader> shader;
    Matrix4 viewMatrix = Matrix4::Identity();
    Matrix4 projectionMatrix = Matrix4::Identity();
    bool matricesInitialized = false;
};

SpriteSharedResources& GetSpriteSharedResources() {
    static SpriteSharedResources resources;
    return resources;
}

struct TextSharedResources {
    std::mutex mutex;
    Ref<Mesh> quadMesh;
    Ref<Shader> shader;
    Matrix4 viewMatrix = Matrix4::Identity();
    Matrix4 projectionMatrix = Matrix4::Identity();
    bool matricesInitialized = false;
};

TextSharedResources& GetTextSharedResources() {
    static TextSharedResources resources;
    return resources;
}

uint32_t HashCombine(uint32_t seed, uint32_t value) {
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

uint32_t HashFloat(float value) {
    return static_cast<uint32_t>(std::hash<float>{}(value));
}

uint32_t HashColor(const Color& color) {
    uint32_t seed = 0;
    seed = HashCombine(seed, HashFloat(color.r));
    seed = HashCombine(seed, HashFloat(color.g));
    seed = HashCombine(seed, HashFloat(color.b));
    seed = HashCombine(seed, HashFloat(color.a));
    return seed;
}

uint32_t HashMatrix(const Matrix4& matrix) {
    const float* values = matrix.data();
    uint32_t hash = 2166136261u;
    for (int i = 0; i < 16; ++i) {
        uint32_t bits = 0;
        std::memcpy(&bits, &values[i], sizeof(uint32_t));
        hash ^= bits;
        hash *= 16777619u;
    }
    return hash;
}

bool EnsureSpriteResources(SpriteSharedResources& resources) {
    auto& resMgr = ResourceManager::GetInstance();

    if (!resources.quadMesh) {
        if (resMgr.HasMesh(kSpriteMeshResourceName)) {
            resources.quadMesh = resMgr.GetMesh(kSpriteMeshResourceName);
        } else {
            auto mesh = MeshLoader::CreateQuad(1.0f, 1.0f, Color::White());
            if (mesh) {
                if (!resMgr.HasMesh(kSpriteMeshResourceName)) {
                    resMgr.RegisterMesh(kSpriteMeshResourceName, mesh);
                }
                resources.quadMesh = mesh;
            }
        }
    }

    if (!resources.shader) {
        if (resMgr.HasShader(kSpriteShaderResourceName)) {
            resources.shader = resMgr.GetShader(kSpriteShaderResourceName);
        } else {
            auto shader = ShaderCache::GetInstance().LoadShader(
                kSpriteShaderResourceName,
                kSpriteShaderVertPath,
                kSpriteShaderFragPath
            );

            if (shader && shader->IsValid()) {
                if (!resMgr.HasShader(kSpriteShaderResourceName)) {
                    resMgr.RegisterShader(kSpriteShaderResourceName, shader);
                }
                resources.shader = shader;
            } else if (shader && !shader->IsValid()) {
                Logger::GetInstance().WarningFormat(
                    "[SpriteRenderable] Shader '%s' is invalid",
                    kSpriteShaderResourceName
                );
            }
        }
    }

    return resources.quadMesh != nullptr && resources.shader != nullptr;
}

bool EnsureTextResources(TextSharedResources& resources) {
    auto& resMgr = ResourceManager::GetInstance();

    if (!resources.quadMesh) {
        if (resMgr.HasMesh(kTextMeshResourceName)) {
            resources.quadMesh = resMgr.GetMesh(kTextMeshResourceName);
        } else {
            auto mesh = MeshLoader::CreateQuad(1.0f, 1.0f, Color::White());
            if (mesh) {
                if (!resMgr.HasMesh(kTextMeshResourceName)) {
                    resMgr.RegisterMesh(kTextMeshResourceName, mesh);
                }
                resources.quadMesh = mesh;
            }
        }
    }

    if (!resources.shader) {
        if (resMgr.HasShader(kTextShaderResourceName)) {
            resources.shader = resMgr.GetShader(kTextShaderResourceName);
        } else {
            auto shader = ShaderCache::GetInstance().LoadShader(
                kTextShaderResourceName,
                kTextShaderVertPath,
                kTextShaderFragPath
            );

            if (shader && shader->IsValid()) {
                if (!resMgr.HasShader(kTextShaderResourceName)) {
                    resMgr.RegisterShader(kTextShaderResourceName, shader);
                }
                resources.shader = shader;
            } else if (shader && !shader->IsValid()) {
                Logger::GetInstance().WarningFormat(
                    "[TextRenderable] Shader '%s' is invalid",
                    kTextShaderResourceName
                );
            }
        }
    }

    return resources.quadMesh != nullptr && resources.shader != nullptr;
}

void GetTextMatrices(TextSharedResources& resources, Matrix4& outView, Matrix4& outProjection, bool& initialized) {
    outView = resources.viewMatrix;
    outProjection = resources.projectionMatrix;
    initialized = resources.matricesInitialized;
}

} // namespace

uint32_t MaterialOverride::ComputeHash() const {
    if (!HasAnyOverride()) {
        return 0u;
    }

    uint32_t seed = 0;
    if (diffuseColor) {
        seed = HashCombine(seed, HashColor(*diffuseColor));
    }
    if (specularColor) {
        seed = HashCombine(seed, HashColor(*specularColor));
    }
    if (emissiveColor) {
        seed = HashCombine(seed, HashColor(*emissiveColor));
    }
    if (shininess) {
        seed = HashCombine(seed, HashFloat(*shininess));
    }
    if (metallic) {
        seed = HashCombine(seed, HashFloat(*metallic));
    }
    if (roughness) {
        seed = HashCombine(seed, HashFloat(*roughness));
    }
    if (opacity) {
        seed = HashCombine(seed, HashFloat(*opacity));
    }

    return seed == 0 ? 1u : seed;
}

// ============================================================
// Renderable 基类实现
// ============================================================

Renderable::Renderable(RenderableType type)
    : m_type(type) {
}

Renderable::Renderable(Renderable&& other) noexcept
    : m_type(other.m_type),
      m_transform(std::move(other.m_transform)),
      m_visible(other.m_visible),
      m_layerID(other.m_layerID),
      m_renderPriority(other.m_renderPriority),
      m_materialSortKey(other.m_materialSortKey),
      m_materialSortDirty(other.m_materialSortDirty),
      m_hasMaterialSortKey(other.m_hasMaterialSortKey),
      m_transparentHint(other.m_transparentHint) {
    // m_mutex 不可移动，使用默认构造
}

Renderable& Renderable::operator=(Renderable&& other) noexcept {
    if (this != &other) {
        m_type = other.m_type;
        m_transform = std::move(other.m_transform);
        m_visible = other.m_visible;
        m_layerID = other.m_layerID;
        m_renderPriority = other.m_renderPriority;
        m_materialSortKey = other.m_materialSortKey;
        m_materialSortDirty = other.m_materialSortDirty;
        m_hasMaterialSortKey = other.m_hasMaterialSortKey;
        m_transparentHint = other.m_transparentHint;
        // m_mutex 不可移动，保持原有状态
    }
    return *this;
}

void Renderable::SetTransform(const Ref<Transform>& transform) {
    std::unique_lock lock(m_mutex);
    m_transform = transform;
}

Ref<Transform> Renderable::GetTransform() const {
    std::shared_lock lock(m_mutex);
    return m_transform;
}

Matrix4 Renderable::GetWorldMatrix() const {
    std::shared_lock lock(m_mutex);
    if (m_transform) {
        return m_transform->GetWorldMatrix();
    }
    return Matrix4::Identity();
}

void Renderable::SetVisible(bool visible) {
    std::unique_lock lock(m_mutex);
    m_visible = visible;
}

bool Renderable::IsVisible() const {
    std::shared_lock lock(m_mutex);
    return m_visible;
}

void Renderable::SetLayerID(uint32_t layerID) {
    std::unique_lock lock(m_mutex);
    m_layerID = layerID;
}

uint32_t Renderable::GetLayerID() const {
    std::shared_lock lock(m_mutex);
    return m_layerID;
}

void Renderable::SetRenderPriority(int32_t priority) {
    std::unique_lock lock(m_mutex);
    m_renderPriority = priority;
}

int32_t Renderable::GetRenderPriority() const {
    std::shared_lock lock(m_mutex);
    return m_renderPriority;
}

void Renderable::SetMaterialSortKey(const MaterialSortKey& key) {
    std::unique_lock lock(m_mutex);
    m_materialSortKey = key;
    m_hasMaterialSortKey = true;
    m_materialSortDirty = false;
}

MaterialSortKey Renderable::GetMaterialSortKey() const {
    std::shared_lock lock(m_mutex);
    return m_materialSortKey;
}

bool Renderable::HasMaterialSortKey() const {
    std::shared_lock lock(m_mutex);
    return m_hasMaterialSortKey;
}

void Renderable::MarkMaterialSortKeyDirty() {
    std::unique_lock lock(m_mutex);
    m_materialSortDirty = true;
}

bool Renderable::IsMaterialSortKeyDirty() const {
    std::shared_lock lock(m_mutex);
    return m_materialSortDirty;
}

void Renderable::SetDepthHint(float depth) {
    std::unique_lock lock(m_mutex);
    m_depthHint = depth;
    m_hasDepthHint = true;
}

bool Renderable::HasDepthHint() const {
    std::shared_lock lock(m_mutex);
    return m_hasDepthHint;
}

float Renderable::GetDepthHint() const {
    std::shared_lock lock(m_mutex);
    return m_depthHint;
}

void Renderable::ClearDepthHint() {
    std::unique_lock lock(m_mutex);
    m_depthHint = 0.0f;
    m_hasDepthHint = false;
}

void Renderable::SetTransparentHint(bool transparent) {
    std::unique_lock lock(m_mutex);
    m_transparentHint = transparent;
}

bool Renderable::GetTransparentHint() const {
    std::shared_lock lock(m_mutex);
    return m_transparentHint;
}

// ============================================================
// MeshRenderable 实现
// ============================================================

MeshRenderable::MeshRenderable()
    : Renderable(RenderableType::Mesh) {
}

MeshRenderable::MeshRenderable(MeshRenderable&& other) noexcept
    : Renderable(std::move(other)),
      m_mesh(std::move(other.m_mesh)),
      m_material(std::move(other.m_material)),
      m_materialOverride(std::move(other.m_materialOverride)),
      m_castShadows(other.m_castShadows),
      m_receiveShadows(other.m_receiveShadows) {
}

MeshRenderable& MeshRenderable::operator=(MeshRenderable&& other) noexcept {
    if (this != &other) {
        Renderable::operator=(std::move(other));
        m_mesh = std::move(other.m_mesh);
        m_material = std::move(other.m_material);
        m_materialOverride = std::move(other.m_materialOverride);
        m_castShadows = other.m_castShadows;
        m_receiveShadows = other.m_receiveShadows;
    }
    return *this;
}

void MeshRenderable::Render(RenderState* renderState) {
    std::shared_lock lock(m_mutex);
    
    if (!m_visible || !m_mesh || !m_material) {
        Logger::GetInstance().DebugFormat("[MeshRenderable] Skip render: visible=%d, hasMesh=%d, hasMaterial=%d",
                                          m_visible, (m_mesh != nullptr), (m_material != nullptr));
        return;
    }
    
    // ✅ 绑定材质并应用渲染状态（带缓存）
    auto& stateCache = MaterialStateCache::Get();
    if (stateCache.ShouldBind(m_material.get(), renderState)) {
        m_material->Bind(renderState);
        stateCache.OnBind(m_material.get(), renderState);
    }
    
    // 获取着色器并设置模型矩阵
    auto shader = m_material->GetShader();
    if (shader && m_transform) {
        // ✅ 检查着色器是否有效
        if (!shader->IsValid()) {
            Logger::GetInstance().WarningFormat("[MeshRenderable] Shader is invalid, skipping render");
            return;
        }
        
        // ✅ 安全检查：确保 UniformManager 已初始化
        auto* uniformMgr = shader->GetUniformManager();
        if (!uniformMgr) {
            static bool warnedOnce = false;
            if (!warnedOnce) {
                Logger::GetInstance().WarningFormat("[MeshRenderable] Shader '%s' has null UniformManager", 
                                                   shader->GetName().c_str());
                warnedOnce = true;
            }
            return;  // 无法设置 uniform，跳过渲染
        }
        
        // ✅ 使用异常保护设置模型矩阵和MaterialOverride
        try {
            Matrix4 modelMatrix = m_transform->GetWorldMatrix();
            // 统一使用u前缀
            uniformMgr->SetMatrix4("uModel", modelMatrix);
            if (uniformMgr->HasUniform("uHasInstanceData")) {
                uniformMgr->SetBool("uHasInstanceData", false);
            }
            
            // ==================== ✅ 应用 MaterialOverride ====================
            // 在Material::Bind()之后应用覆盖，这样不会修改共享的Material对象
            // 而是直接设置shader uniform，实现每个实体有不同的外观
            if (m_materialOverride.HasAnyOverride()) {
                if (m_materialOverride.diffuseColor.has_value()) {
                    // 支持多种着色器uniform名称
                    uniformMgr->SetColor("uDiffuseColor", m_materialOverride.diffuseColor.value());  // material_phong.frag
                    uniformMgr->SetColor("material.diffuse", m_materialOverride.diffuseColor.value());  // 结构体形式
                    uniformMgr->SetColor("uColor", m_materialOverride.diffuseColor.value());  // basic.frag
                }
                if (m_materialOverride.specularColor.has_value()) {
                    uniformMgr->SetColor("uSpecularColor", m_materialOverride.specularColor.value());  // material_phong.frag
                    uniformMgr->SetColor("material.specular", m_materialOverride.specularColor.value());  // 结构体形式
                }
                if (m_materialOverride.emissiveColor.has_value()) {
                    uniformMgr->SetColor("material.emissive", m_materialOverride.emissiveColor.value());
                }
                if (m_materialOverride.shininess.has_value()) {
                    uniformMgr->SetFloat("uShininess", m_materialOverride.shininess.value());  // material_phong.frag
                    uniformMgr->SetFloat("material.shininess", m_materialOverride.shininess.value());  // 结构体形式
                }
                if (m_materialOverride.metallic.has_value()) {
                    uniformMgr->SetFloat("material.metallic", m_materialOverride.metallic.value());
                }
                if (m_materialOverride.roughness.has_value()) {
                    uniformMgr->SetFloat("material.roughness", m_materialOverride.roughness.value());
                }
                if (m_materialOverride.opacity.has_value()) {
                    uniformMgr->SetFloat("material.opacity", m_materialOverride.opacity.value());
                }
                
                // ✅ 如果opacity < 1.0，动态调整渲染状态
                if (m_materialOverride.opacity.has_value() && 
                    m_materialOverride.opacity.value() < 1.0f && 
                    renderState) {
                    renderState->SetBlendMode(BlendMode::Alpha);
                    renderState->SetDepthWrite(false);
                    renderState->SetDepthTest(true);
                }
            }
            
            static int renderCount = 0;
            if (renderCount < 5) {
                Logger::GetInstance().InfoFormat("[MeshRenderable] Render #%d: shader valid, model matrix set, overrides applied", renderCount);
                renderCount++;
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().ErrorFormat("[MeshRenderable] Exception setting uniforms: %s", e.what());
            return;
        }
    }
    
    // 绘制网格
    m_mesh->Draw();
}

void MeshRenderable::SubmitToRenderer(Renderer* renderer) {
    // 通过渲染队列机制，由MeshRenderSystem统一提交
    // 此方法保留用于直接提交的场景（非ECS模式）
    if (renderer) {
        renderer->SubmitRenderable(this);
    }
}

void MeshRenderable::SetMesh(const Ref<Mesh>& mesh) {
    std::unique_lock lock(m_mutex);
    m_mesh = mesh;
}

Ref<Mesh> MeshRenderable::GetMesh() const {
    std::shared_lock lock(m_mutex);
    return m_mesh;
}

void MeshRenderable::SetMaterial(const Ref<Material>& material) {
    std::unique_lock lock(m_mutex);
    m_material = material;
    m_materialSortDirty = true;
    m_hasMaterialSortKey = false;
}

Ref<Material> MeshRenderable::GetMaterial() const {
    std::shared_lock lock(m_mutex);
    return m_material;
}

void MeshRenderable::SetMaterialOverride(const MaterialOverride& override) {
    std::unique_lock lock(m_mutex);
    m_materialOverride = override;
    m_materialSortDirty = true;
}

MaterialOverride MeshRenderable::GetMaterialOverride() const {
    std::shared_lock lock(m_mutex);
    return m_materialOverride;
}

bool MeshRenderable::HasMaterialOverride() const {
    std::shared_lock lock(m_mutex);
    return m_materialOverride.HasAnyOverride();
}

void MeshRenderable::ClearMaterialOverride() {
    std::unique_lock lock(m_mutex);
    m_materialOverride.Clear();
    m_materialSortDirty = true;
}

void MeshRenderable::SetCastShadows(bool cast) {
    std::unique_lock lock(m_mutex);
    m_castShadows = cast;
    m_materialSortDirty = true;
}

bool MeshRenderable::GetCastShadows() const {
    std::shared_lock lock(m_mutex);
    return m_castShadows;
}

void MeshRenderable::SetReceiveShadows(bool receive) {
    std::unique_lock lock(m_mutex);
    m_receiveShadows = receive;
    m_materialSortDirty = true;
}

bool MeshRenderable::GetReceiveShadows() const {
    std::shared_lock lock(m_mutex);
    return m_receiveShadows;
}

AABB MeshRenderable::GetBoundingBox() const {
    std::shared_lock lock(m_mutex);
    
    if (!m_mesh) {
        return AABB();
    }
    
    // 使用 AccessVertices 回调方式计算包围盒
    AABB localBounds;
    bool boundsValid = false;
    
    m_mesh->AccessVertices([&localBounds, &boundsValid](const std::vector<Vertex>& vertices) {
        if (vertices.empty()) {
            return;
        }
        
        // 计算局部空间包围盒
        Vector3 minPoint = vertices[0].position;
        Vector3 maxPoint = vertices[0].position;
        
        for (const auto& vertex : vertices) {
            minPoint = minPoint.cwiseMin(vertex.position);
            maxPoint = maxPoint.cwiseMax(vertex.position);
        }
        
        localBounds = AABB(minPoint, maxPoint);
        boundsValid = true;
    });
    
    if (!boundsValid) {
        return AABB();
    }
    
    // 如果有变换，将包围盒转换到世界空间
    if (m_transform) {
        Matrix4 worldMatrix = m_transform->GetWorldMatrix();
        
        // 变换包围盒的8个顶点
        std::vector<Vector3> corners = {
            Vector3(localBounds.min.x(), localBounds.min.y(), localBounds.min.z()),
            Vector3(localBounds.max.x(), localBounds.min.y(), localBounds.min.z()),
            Vector3(localBounds.min.x(), localBounds.max.y(), localBounds.min.z()),
            Vector3(localBounds.max.x(), localBounds.max.y(), localBounds.min.z()),
            Vector3(localBounds.min.x(), localBounds.min.y(), localBounds.max.z()),
            Vector3(localBounds.max.x(), localBounds.min.y(), localBounds.max.z()),
            Vector3(localBounds.min.x(), localBounds.max.y(), localBounds.max.z()),
            Vector3(localBounds.max.x(), localBounds.max.y(), localBounds.max.z())
        };
        
        // 变换所有顶点并计算新的包围盒
        Vector3 worldMin = (worldMatrix * Vector4(corners[0].x(), corners[0].y(), corners[0].z(), 1.0f)).head<3>();
        Vector3 worldMax = worldMin;
        
        for (const auto& corner : corners) {
            Vector3 transformed = (worldMatrix * Vector4(corner.x(), corner.y(), corner.z(), 1.0f)).head<3>();
            worldMin = worldMin.cwiseMin(transformed);
            worldMax = worldMax.cwiseMax(transformed);
        }
        
        return AABB(worldMin, worldMax);
    }
    
    return localBounds;
}

// ============================================================
// SpriteRenderable 实现
// ============================================================

SpriteRenderable::SpriteRenderable()
    : Renderable(RenderableType::Sprite) {
    m_layerID = 800;  // UI_LAYER
}

SpriteRenderable::SpriteRenderable(SpriteRenderable&& other) noexcept
    : Renderable(std::move(other)),
      m_texture(std::move(other.m_texture)),
      m_sourceRect(other.m_sourceRect),
      m_size(other.m_size),
      m_tintColor(other.m_tintColor),
      m_viewMatrixOverride(other.m_viewMatrixOverride),
      m_projectionMatrixOverride(other.m_projectionMatrixOverride),
      m_useViewProjectionOverride(other.m_useViewProjectionOverride) {
    other.m_useViewProjectionOverride = false;
}

SpriteRenderable& SpriteRenderable::operator=(SpriteRenderable&& other) noexcept {
    if (this != &other) {
        Renderable::operator=(std::move(other));
        m_texture = std::move(other.m_texture);
        m_sourceRect = other.m_sourceRect;
        m_size = other.m_size;
        m_tintColor = other.m_tintColor;
        m_viewMatrixOverride = other.m_viewMatrixOverride;
        m_projectionMatrixOverride = other.m_projectionMatrixOverride;
        m_useViewProjectionOverride = other.m_useViewProjectionOverride;
        other.m_useViewProjectionOverride = false;
    }
    return *this;
}

void SpriteRenderable::Render(RenderState* renderState) {
    Ref<Texture> texture;
    Rect sourceRect;
    Vector2 size;
    Color tintColor;
    Ref<Transform> transform;
    bool useOverride = false;
    Matrix4 overrideView = Matrix4::Identity();
    Matrix4 overrideProjection = Matrix4::Identity();

    {
        std::shared_lock lock(m_mutex);
        if (!m_visible || !m_texture) {
            return;
        }
        texture = m_texture;
        sourceRect = m_sourceRect;
        size = m_size;
        tintColor = m_tintColor;
        transform = m_transform;
        useOverride = m_useViewProjectionOverride;
        if (useOverride) {
            overrideView = m_viewMatrixOverride;
            overrideProjection = m_projectionMatrixOverride;
        }
    }

    Ref<Mesh> quadMesh;
    Ref<Shader> shader;
    Matrix4 viewMatrix = Matrix4::Identity();
    Matrix4 projectionMatrix = Matrix4::Identity();
    bool matricesReady = false;

    {
        auto& shared = GetSpriteSharedResources();
        std::lock_guard<std::mutex> sharedLock(shared.mutex);
        if (!EnsureSpriteResources(shared)) {
            static bool loggedOnce = false;
            if (!loggedOnce) {
                Logger::GetInstance().Warning("[SpriteRenderable] Unable to initialize sprite rendering resources");
                loggedOnce = true;
            }
            return;
        }

        quadMesh = shared.quadMesh;
        shader = shared.shader;
        viewMatrix = shared.viewMatrix;
        projectionMatrix = shared.projectionMatrix;
        matricesReady = shared.matricesInitialized;
    }

    if (useOverride) {
        viewMatrix = overrideView;
        projectionMatrix = overrideProjection;
    } else if (!matricesReady) {
        viewMatrix = Matrix4::Identity();
        projectionMatrix = Matrix4::Identity();
    }

    if (!quadMesh || !shader) {
        return;
    }

    if (!shader->IsValid()) {
        Logger::GetInstance().Warning("[SpriteRenderable] Sprite shader is invalid, skip render");
        return;
    }

    if (renderState) {
        renderState->SetBlendMode(BlendMode::Alpha);
        renderState->SetDepthTest(false);
        renderState->SetDepthWrite(false);
        renderState->SetCullFace(CullFace::None);
    }

    if (size.x() <= 0.0f) {
        float width = static_cast<float>(texture->GetWidth());
        size.x() = width > 0.0f ? width : 1.0f;
    }
    if (size.y() <= 0.0f) {
        float height = static_cast<float>(texture->GetHeight());
        size.y() = height > 0.0f ? height : 1.0f;
    }

    Matrix4 model = Matrix4::Identity();
    if (transform) {
        model = transform->GetWorldMatrix();
    }
    Vector3 scaleVec(size.x(), size.y(), 1.0f);
    model *= MathUtils::Scale(scaleVec);

    auto clamp01 = [](float value) { return std::clamp(value, 0.0f, 1.0f); };

    float texWidth = static_cast<float>(texture->GetWidth());
    float texHeight = static_cast<float>(texture->GetHeight());

    float uMinRaw = sourceRect.x;
    float uMaxRaw = sourceRect.x + sourceRect.width;
    float vMinRaw = sourceRect.y;
    float vMaxRaw = sourceRect.y + sourceRect.height;

    bool usePixelUV = (uMinRaw > 1.0f || uMaxRaw > 1.0f || vMinRaw > 1.0f || vMaxRaw > 1.0f);
    if (usePixelUV) {
        if (texWidth > 0.0f) {
            uMinRaw /= texWidth;
            uMaxRaw /= texWidth;
        }
        if (texHeight > 0.0f) {
            vMinRaw /= texHeight;
            vMaxRaw /= texHeight;
        }
    }

    float uMin = clamp01(uMinRaw);
    float uMax = clamp01(uMaxRaw);
    float vMin = clamp01(vMinRaw);
    float vMax = clamp01(vMaxRaw);

    float uvWidth = std::max(uMax - uMin, 0.0f);
    float uvHeight = std::max(vMax - vMin, 0.0f);
    if (uvWidth <= 0.0f) {
        uMin = 0.0f;
        uvWidth = 1.0f;
    }
    if (uvHeight <= 0.0f) {
        vMin = 0.0f;
        uvHeight = 1.0f;
    }
    Vector4 uvRect(uMin, vMin, uvWidth, uvHeight);

    static bool loggedPixelUV = false;
    if (usePixelUV && !loggedPixelUV) {
        Logger::GetInstance().Info("[SpriteRenderable] Detected pixel-based UV, auto-normalized by texture size");
        loggedPixelUV = true;
    }

    shader->Use();
    auto* uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        Logger::GetInstance().Warning("[SpriteRenderable] UniformManager is null");
        shader->Unuse();
        return;
    }

    if (uniformMgr->HasUniform("uModel")) {
        uniformMgr->SetMatrix4("uModel", model);
    }
    if (uniformMgr->HasUniform("uView")) {
        uniformMgr->SetMatrix4("uView", viewMatrix);
    }
    if (uniformMgr->HasUniform("uProjection")) {
        uniformMgr->SetMatrix4("uProjection", projectionMatrix);
    }
    if (uniformMgr->HasUniform("uTintColor")) {
        uniformMgr->SetColor("uTintColor", tintColor);
    }
    if (uniformMgr->HasUniform("uUVRect")) {
        uniformMgr->SetVector4("uUVRect", uvRect);
    }
    if (uniformMgr->HasUniform("uUseTexture")) {
        uniformMgr->SetBool("uUseTexture", true);
    }
    if (uniformMgr->HasUniform("uUseInstancing")) {
        uniformMgr->SetBool("uUseInstancing", false);
    }
    if (uniformMgr->HasUniform("uTexture")) {
        uniformMgr->SetInt("uTexture", 0);
    } else if (uniformMgr->HasUniform("uTexture0")) {
        uniformMgr->SetInt("uTexture0", 0);
    }

    texture->Bind(0);
    quadMesh->Draw();

    shader->Unuse();
}

void SpriteRenderable::SubmitToRenderer(Renderer* renderer) {
    // 通过渲染队列机制，由SpriteRenderSystem统一提交
    // 此方法保留用于直接提交的场景（非ECS模式）
    if (renderer) {
        renderer->SubmitRenderable(this);
    }
}

void SpriteRenderable::SetTexture(const Ref<Texture>& texture) {
    std::unique_lock lock(m_mutex);
    m_texture = texture;
    m_materialSortDirty = true;
    m_hasMaterialSortKey = false;
}

Ref<Texture> SpriteRenderable::GetTexture() const {
    std::shared_lock lock(m_mutex);
    return m_texture;
}

void SpriteRenderable::SetSourceRect(const Rect& rect) {
    std::unique_lock lock(m_mutex);
    m_sourceRect = rect;
}

Rect SpriteRenderable::GetSourceRect() const {
    std::shared_lock lock(m_mutex);
    return m_sourceRect;
}

void SpriteRenderable::SetSize(const Vector2& size) {
    std::unique_lock lock(m_mutex);
    m_size = size;
}

Vector2 SpriteRenderable::GetSize() const {
    std::shared_lock lock(m_mutex);
    return m_size;
}

void SpriteRenderable::SetTintColor(const Color& color) {
    std::unique_lock lock(m_mutex);
    m_tintColor = color;
}

Color SpriteRenderable::GetTintColor() const {
    std::shared_lock lock(m_mutex);
    return m_tintColor;
}

AABB SpriteRenderable::GetBoundingBox() const {
    std::shared_lock lock(m_mutex);
    
    Vector2 size = m_size;
    if (m_texture) {
        if (size.x() <= 0.0f) {
            float width = static_cast<float>(m_texture->GetWidth());
            size.x() = width > 0.0f ? width : 1.0f;
        }
        if (size.y() <= 0.0f) {
            float height = static_cast<float>(m_texture->GetHeight());
            size.y() = height > 0.0f ? height : 1.0f;
        }
    }
    
    Vector3 halfSize(size.x() * 0.5f, size.y() * 0.5f, 0.0f);
    Vector3 center = Vector3::Zero();
    
    if (m_transform) {
        center = m_transform->GetPosition();
    }
    
    return AABB(center - halfSize, center + halfSize);
}

void SpriteRenderable::SetViewProjectionOverride(const Matrix4& view, const Matrix4& projection) {
    std::unique_lock lock(m_mutex);
    m_viewMatrixOverride = view;
    m_projectionMatrixOverride = projection;
    m_useViewProjectionOverride = true;
}

void SpriteRenderable::ClearViewProjectionOverride() {
    std::unique_lock lock(m_mutex);
    m_useViewProjectionOverride = false;
}

void SpriteRenderable::SetViewProjection(const Matrix4& view, const Matrix4& projection) {
    auto& shared = GetSpriteSharedResources();
    std::lock_guard<std::mutex> lock(shared.mutex);
    shared.viewMatrix = view;
    shared.projectionMatrix = projection;
    shared.matricesInitialized = true;
}

bool SpriteRenderable::AcquireSharedResources(Ref<Mesh>& outMesh, Ref<Shader>& outShader) {
    auto& shared = GetSpriteSharedResources();
    std::lock_guard<std::mutex> lock(shared.mutex);
    if (!EnsureSpriteResources(shared)) {
        return false;
    }
    outMesh = shared.quadMesh;
    outShader = shared.shader;
    return true;
}

// ============================================================
// TextRenderable 实现
// ============================================================

TextRenderable::TextRenderable()
    : Renderable(RenderableType::Text) {
    m_layerID = 800;  // UI_LAYER
    m_transparentHint = true;
}

TextRenderable::TextRenderable(TextRenderable&& other) noexcept
    : Renderable(std::move(other)),
      m_text(std::move(other.m_text)),
      m_viewMatrixOverride(other.m_viewMatrixOverride),
      m_projectionMatrixOverride(other.m_projectionMatrixOverride),
      m_useViewProjectionOverride(other.m_useViewProjectionOverride),
      m_cachedSize(other.m_cachedSize) {
    other.m_useViewProjectionOverride = false;
    other.m_cachedSize = Vector2::Zero();
}

TextRenderable& TextRenderable::operator=(TextRenderable&& other) noexcept {
    if (this != &other) {
        Renderable::operator=(std::move(other));
        m_text = std::move(other.m_text);
        m_viewMatrixOverride = other.m_viewMatrixOverride;
        m_projectionMatrixOverride = other.m_projectionMatrixOverride;
        m_useViewProjectionOverride = other.m_useViewProjectionOverride;
        m_cachedSize = other.m_cachedSize;

        other.m_useViewProjectionOverride = false;
        other.m_cachedSize = Vector2::Zero();
    }
    return *this;
}

bool TextRenderable::GatherBatchData(TextRenderBatchData& outData) {
    outData = {};

    Ref<Text> text;
    Ref<Transform> transform;
    bool useOverride = false;
    Matrix4 overrideView = Matrix4::Identity();
    Matrix4 overrideProjection = Matrix4::Identity();

    {
        std::shared_lock lock(m_mutex);
        if (!m_visible || !m_text) {
            return false;
        }
        text = m_text;
        transform = m_transform;
        useOverride = m_useViewProjectionOverride;
        if (useOverride) {
            overrideView = m_viewMatrixOverride;
            overrideProjection = m_projectionMatrixOverride;
        }
    }

    if (!text || !text->EnsureUpdated()) {
        return false;
    }

    Ref<Texture> texture = text->GetTexture();
    Vector2 size = text->GetSize();
    Color color = text->GetColor();

    if (!texture) {
        std::unique_lock lock(m_mutex);
        m_cachedSize = size;
        return false;
    }

    Ref<Mesh> mesh;
    Ref<Shader> shader;
    if (!AcquireSharedResources(mesh, shader) || !mesh || !shader || !shader->IsValid()) {
        return false;
    }

    Matrix4 viewMatrix = Matrix4::Identity();
    Matrix4 projectionMatrix = Matrix4::Identity();
    bool matricesInitialized = false;

    if (useOverride) {
        viewMatrix = overrideView;
        projectionMatrix = overrideProjection;
    } else {
        auto& shared = GetTextSharedResources();
        std::lock_guard<std::mutex> lock(shared.mutex);
        if (!EnsureTextResources(shared)) {
            return false;
        }
        GetTextMatrices(shared, viewMatrix, projectionMatrix, matricesInitialized);
        if (!matricesInitialized) {
            viewMatrix = Matrix4::Identity();
            projectionMatrix = Matrix4::Identity();
        }
    }

    if (size.x() <= 0.0f) {
        size.x() = 1.0f;
    }
    if (size.y() <= 0.0f) {
        size.y() = 1.0f;
    }

    Matrix4 modelMatrix = Matrix4::Identity();
    if (transform) {
        modelMatrix = transform->GetWorldMatrix();
    }
    modelMatrix *= MathUtils::Scale(Vector3(size.x(), size.y(), 1.0f));

    {
        std::unique_lock lock(m_mutex);
        m_cachedSize = size;
        m_transparentHint = true;
    }

    outData.texture = texture;
    outData.mesh = mesh;
    outData.shader = shader;
    outData.modelMatrix = modelMatrix;
    outData.viewMatrix = viewMatrix;
    outData.projectionMatrix = projectionMatrix;
    outData.color = color;
    outData.screenSpace = !useOverride;
    outData.viewHash = HashMatrix(viewMatrix);
    outData.projectionHash = HashMatrix(projectionMatrix);

    MaterialSortKey key{};
    key.materialID = static_cast<uint32_t>(std::hash<const void*>{}(texture.get()));
    key.shaderID = static_cast<uint32_t>(std::hash<const void*>{}(shader.get()));
    key.blendMode = BlendMode::Alpha;
    key.cullFace = CullFace::None;
    key.depthTest = false;
    key.depthWrite = false;
    key.pipelineFlags = outData.screenSpace ? MaterialPipelineFlags_ScreenSpace : MaterialPipelineFlags_None;
    key.overrideHash = HashColor(color);

    {
        std::unique_lock lock(m_mutex);
        m_materialSortKey = key;
        m_materialSortDirty = false;
        m_hasMaterialSortKey = true;
    }

    return true;
}

void TextRenderable::Render(RenderState* renderState) {
    TextRenderBatchData data;
    if (!GatherBatchData(data)) {
        return;
    }

    if (renderState) {
        renderState->SetBlendMode(BlendMode::Alpha);
        renderState->SetDepthTest(false);
        renderState->SetDepthWrite(false);
        renderState->SetCullFace(CullFace::None);
    }

    auto shader = data.shader;
    auto mesh = data.mesh;
    auto texture = data.texture;
    if (!shader || !mesh || !texture || !shader->IsValid()) {
        return;
    }

    shader->Use();
    auto* uniformMgr = shader->GetUniformManager();
    if (!uniformMgr) {
        shader->Unuse();
        return;
    }

    if (uniformMgr->HasUniform("uModel")) {
        uniformMgr->SetMatrix4("uModel", data.modelMatrix);
    }
    if (uniformMgr->HasUniform("uView")) {
        uniformMgr->SetMatrix4("uView", data.viewMatrix);
    }
    if (uniformMgr->HasUniform("uProjection")) {
        uniformMgr->SetMatrix4("uProjection", data.projectionMatrix);
    }
    if (uniformMgr->HasUniform("uTextColor")) {
        uniformMgr->SetColor("uTextColor", data.color);
    }
    if (uniformMgr->HasUniform("uTexture")) {
        uniformMgr->SetInt("uTexture", 0);
    }

    texture->Bind(0);
    mesh->Draw();
    shader->Unuse();
}

void TextRenderable::SubmitToRenderer(Renderer* renderer) {
    if (renderer) {
        renderer->SubmitRenderable(this);
    }
}

void TextRenderable::SetText(const Ref<Text>& text) {
    std::unique_lock lock(m_mutex);
    m_text = text;
    m_materialSortDirty = true;
    m_hasMaterialSortKey = false;
}

Ref<Text> TextRenderable::GetText() const {
    std::shared_lock lock(m_mutex);
    return m_text;
}

void TextRenderable::SetViewProjectionOverride(const Matrix4& view, const Matrix4& projection) {
    std::unique_lock lock(m_mutex);
    m_viewMatrixOverride = view;
    m_projectionMatrixOverride = projection;
    m_useViewProjectionOverride = true;
}

void TextRenderable::ClearViewProjectionOverride() {
    std::unique_lock lock(m_mutex);
    m_useViewProjectionOverride = false;
}

void TextRenderable::SetViewProjection(const Matrix4& view, const Matrix4& projection) {
    auto& shared = GetTextSharedResources();
    std::lock_guard<std::mutex> lock(shared.mutex);
    shared.viewMatrix = view;
    shared.projectionMatrix = projection;
    shared.matricesInitialized = true;
}

bool TextRenderable::AcquireSharedResources(Ref<Mesh>& outMesh, Ref<Shader>& outShader) {
    auto& shared = GetTextSharedResources();
    std::lock_guard<std::mutex> lock(shared.mutex);
    if (!EnsureTextResources(shared)) {
        return false;
    }
    outMesh = shared.quadMesh;
    outShader = shared.shader;
    return true;
}

void TextRenderable::GetSharedMatrices(Matrix4& outView, Matrix4& outProjection, bool& outInitialized) {
    auto& shared = GetTextSharedResources();
    std::lock_guard<std::mutex> lock(shared.mutex);
    outView = shared.viewMatrix;
    outProjection = shared.projectionMatrix;
    outInitialized = shared.matricesInitialized;
}

AABB TextRenderable::GetBoundingBox() const {
    Ref<Text> text;
    Ref<Transform> transform;
    Vector2 cachedSize;
    {
        std::shared_lock lock(m_mutex);
        cachedSize = m_cachedSize;
        transform = m_transform;
        text = m_text;
    }

    Vector2 size = cachedSize;
    if ((size.x() <= 0.0f || size.y() <= 0.0f) && text) {
        if (text->EnsureUpdated()) {
            size = text->GetSize();
        }
    }

    Vector3 center = Vector3::Zero();
    if (transform) {
        center = transform->GetPosition();
    }

    Vector3 halfSize(size.x() * 0.5f, size.y() * 0.5f, 0.0f);
    return AABB(center - halfSize, center + halfSize);
}

} // namespace Render