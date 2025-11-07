#include "render/renderable.h"
#include "render/renderer.h"
#include "render/shader.h"
#include "render/logger.h"
#include "render/render_state.h"

namespace Render {

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
      m_renderPriority(other.m_renderPriority) {
    // m_mutex 不可移动，使用默认构造
}

Renderable& Renderable::operator=(Renderable&& other) noexcept {
    if (this != &other) {
        m_type = other.m_type;
        m_transform = std::move(other.m_transform);
        m_visible = other.m_visible;
        m_layerID = other.m_layerID;
        m_renderPriority = other.m_renderPriority;
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

void Renderable::SetRenderPriority(uint32_t priority) {
    std::unique_lock lock(m_mutex);
    m_renderPriority = priority;
}

uint32_t Renderable::GetRenderPriority() const {
    std::shared_lock lock(m_mutex);
    return m_renderPriority;
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
    
    // ✅ 绑定材质并应用渲染状态
    m_material->Bind(renderState);
    
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
}

Ref<Material> MeshRenderable::GetMaterial() const {
    std::shared_lock lock(m_mutex);
    return m_material;
}

void MeshRenderable::SetMaterialOverride(const MaterialOverride& override) {
    std::unique_lock lock(m_mutex);
    m_materialOverride = override;
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
}

void MeshRenderable::SetCastShadows(bool cast) {
    std::unique_lock lock(m_mutex);
    m_castShadows = cast;
}

bool MeshRenderable::GetCastShadows() const {
    std::shared_lock lock(m_mutex);
    return m_castShadows;
}

void MeshRenderable::SetReceiveShadows(bool receive) {
    std::unique_lock lock(m_mutex);
    m_receiveShadows = receive;
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
      m_tintColor(other.m_tintColor) {
}

SpriteRenderable& SpriteRenderable::operator=(SpriteRenderable&& other) noexcept {
    if (this != &other) {
        Renderable::operator=(std::move(other));
        m_texture = std::move(other.m_texture);
        m_sourceRect = other.m_sourceRect;
        m_size = other.m_size;
        m_tintColor = other.m_tintColor;
    }
    return *this;
}

void SpriteRenderable::Render(RenderState* renderState) {
    std::shared_lock lock(m_mutex);
    
    if (!m_visible || !m_texture) {
        return;
    }
    
    // ✅ 应用渲染状态（如果提供）
    // TODO: 实现 2D 精灵渲染
    // 这将在后续阶段实现
    (void)renderState;  // 标记参数已使用
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
    
    // 2D 精灵的包围盒（Z=0）
    Vector3 halfSize(m_size.x() * 0.5f, m_size.y() * 0.5f, 0.0f);
    Vector3 center = Vector3::Zero();
    
    if (m_transform) {
        center = m_transform->GetPosition();
    }
    
    return AABB(center - halfSize, center + halfSize);
}

} // namespace Render

