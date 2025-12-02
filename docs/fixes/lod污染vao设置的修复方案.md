## 问题根源

```cpp
// 当前实现：直接修改Mesh的基础VAO
GLuint baseVAO = mesh->GetVertexArrayID();
instanceVBOs.instancedVAO = baseVAO;  // ❌ 使用同一个VAO
glBindVertexArray(baseVAO);
// 设置实例化属性（location 6-11）...
```

当非实例化渲染使用这个VAO时，location 6-11的实例化属性仍然启用，导致渲染错误。

## 解决方案

需要为每个Mesh创建**独立的实例化VAO**，而不是复用基础VAO。这里是修复后的代码：
```
// ==================== 修复VAO污染的关键更改 ====================

// 1. 在InstanceVBOs结构中，instancedVAO现在是独立创建的VAO，不再复用baseVAO
GLuint LODInstancedRenderer::GetOrCreateInstancedVAO(
    Ref<Mesh> mesh,
    InstanceVBOs& instanceVBOs
) {
    GL_THREAD_CHECK();
    
    // 获取基础VAO（用于复制顶点数据设置）
    GLuint baseVAO = mesh->GetVertexArrayID();
    if (baseVAO == 0) {
        LOG_WARNING("LODInstancedRenderer: Base mesh VAO is invalid");
        return 0;
    }
    
    // ✅ 如果instancedVAO为0，创建新的独立VAO
    if (instanceVBOs.instancedVAO == 0) {
        glGenVertexArrays(1, &instanceVBOs.instancedVAO);
        instanceVBOs.attributesSetup = false;
    }
    
    // 如果属性已经设置，直接返回
    if (instanceVBOs.attributesSetup) {
        return instanceVBOs.instancedVAO;
    }
    
    // ✅ 绑定新创建的实例化VAO
    glBindVertexArray(instanceVBOs.instancedVAO);
    
    // ✅ 步骤1：复制基础Mesh的顶点属性设置（location 0-5）
    // 获取Mesh的VBO和EBO
    GLuint meshVBO = mesh->GetVertexBufferID();
    GLuint meshEBO = mesh->GetIndexBufferID();
    
    if (meshVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
        
        // 设置标准顶点属性（根据Mesh的布局）
        // 假设标准布局：Position(0), Normal(1), TexCoord(2), Tangent(3), Bitangent(4)
        
        // Location 0: Position (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 
                              mesh->GetVertexStride(), 
                              (void*)mesh->GetPositionOffset());
        
        // Location 1: Normal (vec3)
        if (mesh->HasNormals()) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 
                                  mesh->GetVertexStride(), 
                                  (void*)mesh->GetNormalOffset());
        }
        
        // Location 2: TexCoord (vec2)
        if (mesh->HasTexCoords()) {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 
                                  mesh->GetVertexStride(), 
                                  (void*)mesh->GetTexCoordOffset());
        }
        
        // Location 3: Tangent (vec3)
        if (mesh->HasTangents()) {
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 
                                  mesh->GetVertexStride(), 
                                  (void*)mesh->GetTangentOffset());
        }
        
        // Location 4: Bitangent (vec3)
        if (mesh->HasBitangents()) {
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 
                                  mesh->GetVertexStride(), 
                                  (void*)mesh->GetBitangentOffset());
        }
    }
    
    // 绑定索引缓冲区
    if (meshEBO != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
    }
    
    // ✅ 步骤2：设置实例化属性（location 6-11）
    
    // 实例化矩阵（location 6-9）
    if (instanceVBOs.matrixVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.matrixVBO);
        for (int i = 0; i < 4; ++i) {
            GLuint location = 6 + i;
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(float) * 16,
                                  (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(location, 1);  // ✅ 关键：设置为实例化属性
        }
    }
    
    // 实例化颜色（location 10）
    if (instanceVBOs.colorVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.colorVBO);
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
        glVertexAttribDivisor(10, 1);
    }
    
    // 自定义参数（location 11）
    if (instanceVBOs.paramsVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBOs.paramsVBO);
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), 0);
        glVertexAttribDivisor(11, 1);
    }
    
    // 解绑
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    instanceVBOs.attributesSetup = true;
    
    return instanceVBOs.instancedVAO;
}

// 2. 更新ClearInstanceVBOs，删除独立创建的VAO
void LODInstancedRenderer::ClearInstanceVBOs() {
    GL_THREAD_CHECK();
    
    for (auto& [mesh, vbos] : m_instanceVBOs) {
        // 根据是否使用持久映射选择清理方式
        if (vbos.usePersistentMapping) {
            DestroyPersistentMappedVBOs(vbos);
        } else {
            // 传统清理方式
            if (vbos.matrixVBO != 0) {
                glDeleteBuffers(1, &vbos.matrixVBO);
                vbos.matrixVBO = 0;
            }
            if (vbos.colorVBO != 0) {
                glDeleteBuffers(1, &vbos.colorVBO);
                vbos.colorVBO = 0;
            }
            if (vbos.paramsVBO != 0) {
                glDeleteBuffers(1, &vbos.paramsVBO);
                vbos.paramsVBO = 0;
            }
        }
        
        // ✅ 删除独立创建的实例化VAO
        if (vbos.instancedVAO != 0) {
            glDeleteVertexArrays(1, &vbos.instancedVAO);
            vbos.instancedVAO = 0;
        }
        
        vbos.capacity = 0;
        vbos.colorCapacity = 0;
        vbos.paramsCapacity = 0;
        vbos.attributesSetup = false;
    }
    
    m_instanceVBOs.clear();
}

// 3. 更新RenderGroup，确保清理状态
void LODInstancedRenderer::RenderGroup(
    LODInstancedGroup* group,
    Renderer* renderer,
    RenderState* renderState
) {
    if (!group || !group->mesh || !group->material || group->instances.empty()) {
        return;
    }
    
    if (!renderer) {
        return;
    }
    
    GL_THREAD_CHECK();
    
    // 使用MaterialStateCache避免重复绑定材质
    auto& stateCache = MaterialStateCache::Get();
    if (stateCache.ShouldBind(group->material.get(), renderState)) {
        group->material->Bind(renderState);
        stateCache.OnBind(group->material.get(), renderState);
    }
    
    // 设置实例化标志
    if (auto shader = group->material->GetShader()) {
        if (auto uniformMgr = shader->GetUniformManager()) {
            uniformMgr->SetBool("uHasInstanceData", true);
            uniformMgr->SetMatrix4("uModel", Matrix4::Identity());
        }
    }
    
    // 仅在需要时上传数据
    if (group->NeedsUpload()) {
        ScopedTimer uploadTimer(m_stats.uploadTimeMs);
        UploadInstanceData(group->instances, group->mesh);
        group->MarkUploaded();
    }
    
    size_t instanceCount = group->instances.size();
    auto& instanceVBOs = GetOrCreateInstanceVBOs(group->mesh, instanceCount);
    
    // 获取或创建实例化VAO
    GLuint vao = GetOrCreateInstancedVAO(group->mesh, instanceVBOs);
    
    if (vao == 0) {
        LOG_WARNING("LODInstancedRenderer: Failed to create instanced VAO");
        return;
    }
    
    // 绑定VAO
    if (renderState) {
        renderState->BindVertexArray(vao);
    } else {
        glBindVertexArray(vao);
    }
    
    // 实例化绘制
    group->mesh->DrawInstanced(static_cast<uint32_t>(instanceCount));
    
    // 解绑VAO
    if (renderState) {
        renderState->BindVertexArray(0);
    } else {
        glBindVertexArray(0);
    }
    
    // ✅ 清理OpenGL状态（可选，但更安全）
    // 注意：由于我们使用了独立VAO，这些清理实际上不是必须的
    // 但为了保险起见，仍然保留
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    // 如果需要，可以禁用实例化属性（但通常不需要，因为VAO已经解绑）
    // for (int i = 6; i <= 11; ++i) {
    //     glDisableVertexAttribArray(i);
    // }
}
```

## 关键修改说明

### 1. **独立VAO创建**
- 原实现：直接使用 `mesh->GetVertexArrayID()` 作为实例化VAO
- 新实现：用 `glGenVertexArrays` 创建**独立的实例化VAO**

### 2. **完整属性设置**
新VAO需要同时包含：
- **基础顶点属性**（location 0-5）：从Mesh复制
- **实例化属性**（location 6-11）：新增

### 3. **正确清理**
在 `ClearInstanceVBOs` 中，使用 `glDeleteVertexArrays` 删除独立创建的VAO

## 需要在Mesh类中添加的辅助方法

如果Mesh类没有提供这些方法，需要添加：

```cpp
// 在Mesh类中添加这些公共方法
class Mesh {
public:
    GLuint GetVertexBufferID() const { return m_vbo; }
    GLuint GetIndexBufferID() const { return m_ebo; }
    
    size_t GetVertexStride() const { return sizeof(Vertex); }
    size_t GetPositionOffset() const { return offsetof(Vertex, position); }
    size_t GetNormalOffset() const { return offsetof(Vertex, normal); }
    size_t GetTexCoordOffset() const { return offsetof(Vertex, texCoord); }
    size_t GetTangentOffset() const { return offsetof(Vertex, tangent); }
    size_t GetBitangentOffset() const { return offsetof(Vertex, bitangent); }
    
    bool HasNormals() const { return m_hasNormals; }
    bool HasTexCoords() const { return m_hasTexCoords; }
    bool HasTangents() const { return m_hasTangents; }
    bool HasBitangents() const { return m_hasBitangents; }
};
```

## 为什么这样修复有效

1. **隔离性**：每个Mesh的实例化VAO是独立的，不会影响Mesh的原始VAO
2. **完整性**：新VAO包含所有必要的属性配置（基础+实例化）
3. **兼容性**：非实例化渲染继续使用Mesh的原始VAO（location 6-11未启用）

这样修改后，非实例化渲染不会受到影响，因为它使用的是Mesh自己的VAO，没有启用实例化属性。