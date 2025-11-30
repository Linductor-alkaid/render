# Model ECS 集成优化方案

[返回文档首页](README.md)

---

## 📋 执行摘要

### 当前状态分析

经过代码审查，发现：

1. **✅ ModelComponent 和 ModelRenderSystem 已存在**
   - `ModelComponent` 定义在 `include/render/ecs/components.h`
   - `ModelRenderSystem` 定义在 `include/render/ecs/systems.h` 并已实现
   - 已支持：LOD、视锥体裁剪、异步加载、透明排序、对象池复用

2. **⚠️ 但未充分利用所有优化**
   - **批处理系统未集成**：ModelRenderSystem 逐个提交 ModelRenderable，未使用 Renderer 的批处理系统
   - **LOD 实例化渲染未集成**：MeshRenderSystem 已支持 LOD 实例化，但 ModelRenderSystem 未支持
   - **批处理优化缺失**：未利用 CPU 合批和 GPU 实例化优化

3. **✅ 已有优化功能**
   - 异步资源加载（AsyncResourceLoader）
   - LOD 系统（LODComponent + LODSelector）
   - 视锥体裁剪（ShouldCull）
   - 材质排序（MaterialSortKey）
   - 透明物体排序（深度排序）
   - 对象池复用（m_renderables）

### 结论

**需要将 Model 融合进 ECS**，但主要是**优化现有 ModelRenderSystem**，使其充分利用项目中已有的各种优化，包括：
- 批处理系统（RenderBatching）
- LOD 实例化渲染（LOD Instancing）
- 批操作优化

---

## 🎯 设计目标

1. **0 破坏性**：保持现有 API 和接口不变，向后兼容
2. **充分利用优化**：集成批处理、LOD 实例化、批操作等所有优化
3. **性能提升**：减少 Draw Call，提升渲染性能
4. **代码复用**：复用 MeshRenderSystem 的优化经验

---

## 📐 设计方案

### 阶段 1：批处理系统集成

#### 1.1 分析当前实现

**当前 ModelRenderSystem 流程**：
```cpp
void ModelRenderSystem::SubmitRenderables() {
    // 1. 查询实体
    auto entities = m_world->Query<TransformComponent, ModelComponent>();
    
    // 2. 逐个创建 ModelRenderable
    for (const auto& entity : entities) {
        ModelRenderable renderable;
        // ... 设置属性
        m_renderables.push_back(std::move(renderable));
    }
    
    // 3. 逐个提交到 Renderer
    for (size_t idx : opaqueIndices) {
        m_renderer->SubmitRenderable(&m_renderables[idx]);
    }
}
```

**问题**：
- 每个 ModelRenderable 单独提交，无法利用批处理
- 即使多个模型使用相同材质/网格，也无法合并

#### 1.2 集成批处理系统

**方案**：让 ModelRenderable 支持批处理，类似 MeshRenderable

**实现步骤**：

1. **扩展 ModelRenderable 支持批处理**
   ```cpp
   // 在 ModelRenderable 中添加批处理支持
   class ModelRenderable : public Renderable {
   public:
       // 检查是否可以批处理
       bool CanBatch() const;
       
       // 创建批处理项
       BatchableItem CreateBatchableItem() const;
   };
   ```

2. **ModelRenderSystem 利用 Renderer 批处理**
   ```cpp
   void ModelRenderSystem::SubmitRenderables() {
       // 1. 查询实体
       auto entities = m_world->Query<TransformComponent, ModelComponent>();
       
       // 2. 创建 ModelRenderable（对象池复用）
       for (const auto& entity : entities) {
           ModelRenderable renderable;
           // ... 设置属性
           m_renderables.push_back(std::move(renderable));
       }
       
       // 3. 提交到 Renderer（Renderer 会自动批处理）
       for (auto& renderable : m_renderables) {
           renderable.SubmitToRenderer(m_renderer);
           // Renderer::SubmitRenderable() 会自动检测批处理能力
       }
   }
   ```

3. **Renderer 自动批处理**
   - Renderer 已经支持批处理（通过 `SetBatchingMode()`）
   - ModelRenderable 只需要实现 `CanBatch()` 和 `CreateBatchableItem()`
   - Renderer 会自动将可批处理的 ModelRenderable 分组并批量渲染

**优势**：
- ✅ 0 破坏性：现有代码无需修改
- ✅ 自动批处理：Renderer 自动处理批处理逻辑
- ✅ 性能提升：减少 Draw Call

---

### 阶段 2：LOD 实例化渲染集成

#### 2.1 分析 MeshRenderSystem 的 LOD 实例化实现

**MeshRenderSystem 已实现**：
```cpp
void MeshRenderSystem::Update(float deltaTime) {
    // 1. 检查 LOD 实例化是否启用
    bool lodInstancingEnabled = IsLODInstancingEnabled();
    bool lodInstancingAvailable = m_renderer->IsLODInstancingAvailable();
    
    if (lodInstancingEnabled && lodInstancingAvailable) {
        // 2. 使用 LOD 实例化渲染
        // ... LOD 实例化逻辑
    } else {
        // 3. 回退到普通渲染
        SubmitRenderables();
    }
}
```

#### 2.2 为 ModelRenderSystem 添加 LOD 实例化支持

**实现步骤**：

1. **添加 LOD 实例化方法**
   ```cpp
   class ModelRenderSystem : public System {
   public:
       // 启用/禁用 LOD 实例化
       void SetLODInstancingEnabled(bool enabled);
       bool IsLODInstancingEnabled() const;
       
       // 检查 LOD 实例化是否可用
       bool IsLODInstancingAvailable() const;
   };
   ```

2. **实现 LOD 实例化渲染逻辑**
   ```cpp
   void ModelRenderSystem::SubmitRenderables() {
       bool lodInstancingEnabled = IsLODInstancingEnabled();
       bool lodInstancingAvailable = m_renderer && 
                                      m_renderer->IsLODInstancingAvailable();
       
       if (lodInstancingEnabled && lodInstancingAvailable) {
           // 使用 LOD 实例化渲染
           SubmitRenderablesWithLODInstancing();
       } else {
           // 使用普通批处理渲染
           SubmitRenderablesWithBatching();
       }
   }
   ```

3. **LOD 实例化渲染实现**
   ```cpp
   void ModelRenderSystem::SubmitRenderablesWithLODInstancing() {
       // 1. 查询所有有 ModelComponent 和 LODComponent 的实体
       auto entities = m_world->Query<TransformComponent, ModelComponent, LODComponent>();
       
       // 2. 按 LOD 级别分组
       std::map<LODLevel, std::vector<EntityID>> entitiesByLOD;
       for (const auto& entity : entities) {
           auto& lodComp = m_world->GetComponent<LODComponent>(entity);
           entitiesByLOD[lodComp.currentLOD].push_back(entity);
       }
       
       // 3. 对每个 LOD 级别进行实例化渲染
       for (const auto& [lodLevel, entityList] : entitiesByLOD) {
           if (lodLevel == LODLevel::Culled) continue;
           
           // 按模型和材质分组
           std::map<ModelPtr, std::map<MaterialPtr, std::vector<EntityID>>> groups;
           for (EntityID entity : entityList) {
               auto& modelComp = m_world->GetComponent<ModelComponent>(entity);
               // ... 分组逻辑
           }
           
           // 4. 实例化渲染每个组
           for (const auto& [model, materialGroups] : groups) {
               for (const auto& [material, instances] : materialGroups) {
                   // 使用 GPU 实例化渲染
                   RenderInstanced(model, material, instances);
               }
           }
       }
   }
   ```

**优势**：
- ✅ 复用 MeshRenderSystem 的经验
- ✅ 大幅减少 Draw Call（100个实例可能只需2-10个 Draw Call）
- ✅ 自动按 LOD 级别分组

---

### 阶段 3：批操作优化

#### 3.1 批量 LOD 计算

**当前实现**：ModelRenderSystem 中 LOD 计算是逐个进行的

**优化方案**：使用 LODSelector::BatchCalculateLOD

```cpp
void ModelRenderSystem::Update(float deltaTime) {
    // 1. 批量计算 LOD（在提交前）
    if (m_world) {
        auto lodEntities = m_world->Query<LODComponent, TransformComponent, ModelComponent>();
        if (!lodEntities.empty()) {
            Camera* camera = m_cameraSystem ? m_cameraSystem->GetMainCameraObject() : nullptr;
            if (camera) {
                Vector3 cameraPos = camera->GetPosition();
                static uint64_t frameId = 0;
                frameId++;
                
                // 批量计算 LOD
                LODSelector::BatchCalculateLODWithBounds(
                    std::vector<EntityID>(lodEntities.begin(), lodEntities.end()),
                    m_world,
                    cameraPos,
                    frameId,
                    [this](EntityID entity) -> AABB {
                        // 获取模型包围盒
                        if (m_world->HasComponent<ModelComponent>(entity)) {
                            auto& modelComp = m_world->GetComponent<ModelComponent>(entity);
                            if (modelComp.model) {
                                return modelComp.model->GetBounds();
                            }
                        }
                        return AABB();
                    }
                );
            }
        }
    }
    
    // 2. 提交渲染
    SubmitRenderables();
}
```

#### 3.2 批量视锥体裁剪

**当前实现**：逐个进行视锥体裁剪

**优化方案**：使用 LODFrustumCullingSystem::BatchCullAndSelectLOD

```cpp
void ModelRenderSystem::SubmitRenderablesWithLODInstancing() {
    Camera* camera = m_cameraSystem ? m_cameraSystem->GetMainCameraObject() : nullptr;
    if (!camera) {
        SubmitRenderablesWithBatching();
        return;
    }
    
    // 批量视锥体裁剪和 LOD 选择
    auto entities = m_world->Query<TransformComponent, ModelComponent>();
    std::vector<EntityID> entityList(entities.begin(), entities.end());
    
    static uint64_t frameId = 0;
    frameId++;
    
    auto visibleEntitiesByLOD = LODFrustumCullingSystem::BatchCullAndSelectLODWithBounds(
        entityList,
        m_world,
        camera,
        frameId,
        [this](EntityID entity) -> AABB {
            // 获取模型包围盒
            if (m_world->HasComponent<ModelComponent>(entity)) {
                auto& modelComp = m_world->GetComponent<ModelComponent>(entity);
                if (modelComp.model) {
                    auto& transformComp = m_world->GetComponent<TransformComponent>(entity);
                    AABB localBounds = modelComp.model->GetBounds();
                    // 转换到世界空间
                    Matrix4 worldMatrix = transformComp.GetWorldMatrix();
                    return TransformAABB(localBounds, worldMatrix);
                }
            }
            return AABB();
        }
    );
    
    // 按 LOD 级别处理可见实体
    for (const auto& [lodLevel, visibleEntities] : visibleEntitiesByLOD) {
        // ... 实例化渲染逻辑
    }
}
```

**优势**：
- ✅ 批量处理，提升缓存命中率
- ✅ 减少 CPU 开销
- ✅ 先进行视锥体裁剪，只对可见实体计算 LOD

---

## 🔧 实现细节

### 1. ModelRenderable 批处理支持

**需要实现的方法**：

```cpp
class ModelRenderable : public Renderable {
public:
    // 检查是否可以批处理
    bool CanBatch() const override {
        // 条件：
        // 1. 模型已加载
        // 2. 所有部件都有有效的网格和材质
        // 3. 没有材质覆盖
        // 4. 不是透明物体（或透明物体可以单独批处理）
        
        if (!m_model) return false;
        
        bool canBatch = true;
        m_model->AccessParts([&](const std::vector<ModelPart>& parts) {
            for (const auto& part : parts) {
                if (!part.mesh || !part.material) {
                    canBatch = false;
                    return;
                }
            }
        });
        
        return canBatch;
    }
    
    // 创建批处理项（为每个 ModelPart 创建）
    std::vector<BatchableItem> CreateBatchableItems() const {
        std::vector<BatchableItem> items;
        
        if (!m_model) return items;
        
        m_model->AccessParts([&](const std::vector<ModelPart>& parts) {
            for (const auto& part : parts) {
                if (!part.mesh || !part.material) continue;
                
                BatchableItem item;
                item.mesh = part.mesh;
                item.material = part.material;
                item.modelMatrix = m_transform ? m_transform->GetWorldMatrix() : Matrix4::Identity();
                item.layerID = m_layerID;
                item.renderPriority = m_renderPriority;
                // ... 其他属性
                
                items.push_back(item);
            }
        });
        
        return items;
    }
};
```

### 2. Renderer 批处理集成

**Renderer 已经支持批处理**，只需要确保 ModelRenderable 正确实现批处理接口：

```cpp
// Renderer::SubmitRenderable() 会自动检测
void Renderer::SubmitRenderable(Renderable* renderable) {
    if (renderable->CanBatch() && m_batchingMode != BatchingMode::Disabled) {
        // 创建批处理项
        auto items = renderable->CreateBatchableItems();
        for (const auto& item : items) {
            m_batchManager->AddItem(item);
        }
    } else {
        // 直接渲染
        m_renderQueue.push_back(renderable);
    }
}
```

### 3. LOD 实例化渲染实现

**参考 MeshRenderSystem 的实现**：

```cpp
void ModelRenderSystem::SubmitRenderablesWithLODInstancing() {
    // 1. 批量视锥体裁剪和 LOD 选择
    // ... (见阶段3.2)
    
    // 2. 按 LOD 级别、模型、材质分组
    struct GroupKey {
        ModelPtr model;
        MaterialPtr material;
        LODLevel lodLevel;
        
        bool operator<(const GroupKey& other) const {
            if (model != other.model) return model < other.model;
            if (material != other.material) return material < other.material;
            return lodLevel < other.lodLevel;
        }
    };
    
    std::map<GroupKey, std::vector<EntityID>> groups;
    
    for (const auto& [lodLevel, visibleEntities] : visibleEntitiesByLOD) {
        if (lodLevel == LODLevel::Culled) continue;
        
        for (EntityID entity : visibleEntities) {
            auto& modelComp = m_world->GetComponent<ModelComponent>(entity);
            auto& lodComp = m_world->GetComponent<LODComponent>(entity);
            
            // 获取 LOD 模型
            Ref<Model> lodModel = lodComp.config.GetLODModel(lodLevel, modelComp.model);
            if (!lodModel) continue;
            
            // 按 ModelPart 分组
            lodModel->AccessParts([&](const std::vector<ModelPart>& parts) {
                for (const auto& part : parts) {
                    if (!part.mesh || !part.material) continue;
                    
                    GroupKey key{lodModel, part.material, lodLevel};
                    groups[key].push_back(entity);
                }
            });
        }
    }
    
    // 3. 实例化渲染每个组
    for (const auto& [key, instances] : groups) {
        if (instances.size() < 2) {
            // 单个实例，使用普通渲染
            // ...
            continue;
        }
        
        // 收集实例数据
        std::vector<Matrix4> instanceMatrices;
        for (EntityID entity : instances) {
            auto& transformComp = m_world->GetComponent<TransformComponent>(entity);
            instanceMatrices.push_back(transformComp.GetWorldMatrix());
        }
        
        // 使用 GPU 实例化渲染
        RenderInstanced(key.model, key.material, key.lodLevel, instanceMatrices);
    }
}
```

---

## 📊 性能预期

### 批处理系统集成

**当前**：
- 100个模型，每个模型6个部件 = 600个 Draw Call

**优化后**：
- 100个模型，相同材质合并 = 10-50个 Draw Call
- **性能提升：10-60倍 Draw Call 减少**

### LOD 实例化渲染集成

**当前**：
- 1000个相同模型实例 = 1000个 Draw Call

**优化后**：
- 1000个相同模型实例，按 LOD 分组 = 10-50个 Draw Call
- **性能提升：20-100倍 Draw Call 减少**

### 批操作优化

**当前**：
- LOD 计算：逐个计算，1000个实体 ≈ 5ms

**优化后**：
- LOD 计算：批量计算，1000个实体 ≈ 1ms
- **性能提升：5倍 CPU 开销减少**

---

## 🚀 实施计划

### Phase 1: 批处理系统集成（优先级：高）

1. **扩展 ModelRenderable 支持批处理**
   - 实现 `CanBatch()` 方法
   - 实现 `CreateBatchableItems()` 方法
   - 测试批处理逻辑

2. **验证批处理效果**
   - 创建测试场景（多个相同模型）
   - 验证 Draw Call 减少
   - 验证渲染正确性

**预计时间**：2-3天

### Phase 2: LOD 实例化渲染集成（优先级：高）

1. **添加 LOD 实例化方法**
   - 在 ModelRenderSystem 中添加 `SetLODInstancingEnabled()` 等方法
   - 实现 `SubmitRenderablesWithLODInstancing()` 方法

2. **实现 LOD 实例化渲染逻辑**
   - 按 LOD 级别、模型、材质分组
   - 实现 GPU 实例化渲染
   - 处理回退机制

3. **测试和优化**
   - 创建测试场景（大量相同模型实例）
   - 验证 LOD 实例化效果
   - 性能测试

**预计时间**：3-5天

### Phase 3: 批操作优化（优先级：中）

1. **批量 LOD 计算**
   - 在 ModelRenderSystem::Update() 中添加批量 LOD 计算
   - 使用 LODSelector::BatchCalculateLODWithBounds

2. **批量视锥体裁剪**
   - 使用 LODFrustumCullingSystem::BatchCullAndSelectLODWithBounds
   - 优化包围盒计算

3. **测试和优化**
   - 性能测试
   - 验证正确性

**预计时间**：2-3天

---

## ✅ 验证标准

### 功能验证

1. **批处理系统**
   - [ ] 多个相同模型可以批处理
   - [ ] 不同材质不会错误合并
   - [ ] 透明物体正确排序
   - [ ] Draw Call 数量减少

2. **LOD 实例化渲染**
   - [ ] LOD 级别正确选择
   - [ ] 实例化渲染正确
   - [ ] 回退机制工作正常
   - [ ] Draw Call 数量大幅减少

3. **批操作优化**
   - [ ] LOD 计算性能提升
   - [ ] 视锥体裁剪正确
   - [ ] CPU 开销减少

### 性能验证

1. **基准测试**
   - 100个模型：Draw Call < 50
   - 1000个模型实例：Draw Call < 100
   - LOD 计算：1000个实体 < 2ms

2. **回归测试**
   - 现有功能不受影响
   - 渲染结果一致
   - 内存使用正常

---

## 🔄 向后兼容性

### API 兼容性

- ✅ `ModelComponent` 结构不变
- ✅ `ModelRenderSystem` 公共接口不变
- ✅ `ModelRenderable` 公共接口不变
- ✅ 现有代码无需修改

### 行为兼容性

- ✅ 默认行为不变（批处理默认禁用）
- ✅ 可以通过 `SetBatchingMode()` 启用批处理
- ✅ 可以通过 `SetLODInstancingEnabled()` 启用 LOD 实例化
- ✅ 如果优化不可用，自动回退到原始行为

---

## 📝 总结

### 是否需要将 Model 融合进 ECS？

**答案：Model 已经融合进 ECS**，但需要**优化现有实现**以充分利用所有优化。

### 主要工作

1. **批处理系统集成**：让 ModelRenderable 支持批处理
2. **LOD 实例化渲染集成**：复用 MeshRenderSystem 的经验
3. **批操作优化**：使用批量 LOD 计算和视锥体裁剪

### 预期收益

- **Draw Call 减少：10-100倍**
- **CPU 开销减少：5倍**
- **0 破坏性**：完全向后兼容

---

[返回文档首页](README.md)

