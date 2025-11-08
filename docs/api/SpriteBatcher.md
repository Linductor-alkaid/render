# SpriteBatcher API 参考

[返回 API 目录](README.md)

---

## 📋 概述

`SpriteBatcher` 聚合同构精灵实例，生成 GPU 实例化批次，配合 `SpriteBatchRenderable` 与渲染器的批处理管线，显著减少 Draw Call。  

- **命名空间**：`Render`  
- **头文件**：`<render/sprite/sprite_batcher.h>`  
- **线程安全**：否；由 `SpriteRenderSystem` 在渲染线程内驱动。

---

## 🏗️ 关键类型

```cpp
class SpriteBatcher {
public:
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
                   uint32_t sortOrder,
                   BlendMode blendMode = BlendMode::Alpha);

    void BuildBatches();
    size_t GetBatchCount() const;
    uint32_t GetBatchLayer(size_t index) const;
    uint32_t GetBatchSortOrder(size_t index) const;
    void DrawBatch(size_t index, RenderState* renderState);

    struct SpriteBatchInfo {
        Ref<Texture> texture;
        BlendMode blendMode;
        bool screenSpace;
        uint32_t viewHash;
        uint32_t projectionHash;
        Matrix4 viewMatrix;
        Matrix4 projectionMatrix;
        uint32_t instanceCount;
        uint32_t layer;
        uint32_t sortOrder;
    };

    bool GetBatchInfo(size_t index, SpriteBatchInfo& outInfo) const;
};
```

`SpriteBatchRenderable` 是一个轻量代理对象，持有 `SpriteBatcher` 与批次索引，在渲染器管线中作为 `RenderableType::Sprite` 使用。

---

## 🔁 工作流程

1. `Clear()`：清空上一帧缓存。  
2. `AddSprite()`：由 `SpriteRenderSystem` 针对每个 ECS 精灵调用，记录纹理、矩阵、UV、颜色等信息。  
3. `BuildBatches()`：按照层级、排序键、纹理、视图/投影哈希分组，生成批次实例数组。  
4. `GetBatchInfo()`：渲染器在批处理阶段读取批次元数据，转换为 `BatchableItem`。  
5. `DrawBatch()`：真正绘制时绑定实例缓冲，执行 `DrawInstanced()`。

---

## 📦 GPU 实例化细节

- 使用共享的四边形网格与 sprite shader。  
- 实例缓冲 (`InstancePayload`) 包含：模型矩阵、UV 矩形、颜色。  
- 顶点着色器通过 `uUseInstancing` 与额外顶点属性（location 4~9）加载实例数据。  
- 每个批次对应一次 Draw Call；统计数据可通过 `SpriteRenderSystem::GetLastBatchCount()` 获取。

---

## 🧩 示例（ECS 侧）

`SpriteRenderSystem` 片段（简化）：

```cpp
m_batcher.Clear();

for (auto entity : m_world->Query<TransformComponent, SpriteRenderComponent>()) {
    // ... 计算模型矩阵、视图矩阵
    m_batcher.AddSprite(spriteComp.texture,
                        spriteComp.sourceRect,
                        effectiveSize,
                        spriteComp.tintColor,
                        modelMatrix,
                        viewMatrix,
                        projectionMatrix,
                        spriteComp.screenSpace,
                        spriteComp.layerID,
                        spriteComp.sortOrder,
                        BlendMode::Alpha);
}

m_batcher.BuildBatches();

for (size_t i = 0; i < m_batcher.GetBatchCount(); ++i) {
    auto& renderable = AcquireSpriteBatchRenderable(i);
    renderable.SetBatch(&m_batcher, i);
    renderable.SubmitToRenderer(m_renderer);
}
```

---

## ⚠️ 注意事项

- 批次按层级和排序键划分；不同 blendMode 或视图/投影组合无法合并。  
- `AddSprite()` 需要已加载的纹理指针；若尚未完成加载会跳过该实体。  
- `DrawBatch()` 依赖 `RenderState` 设置混合模式、深度与剔除；若自定义渲染流程，请在调用前配置好状态。  
- `GetBatchInfo()` 返回 `false` 表示批次无实例或纹理缺失，此时不要提交到渲染器。

---

## 📚 相关文档

- [SpriteRenderer](SpriteRenderer.md) — 即时模式渲染。  
- [RenderBatching](RenderBatching.md) — 渲染批处理框架。  
- [SpriteRenderable](SpriteRenderable.md) — 底层渲染对象。  
- [Renderer](Renderer.md) — 批处理模式配置 (`SetBatchingMode`)。

---

[上一页](SpriteAtlasImporter.md) | [下一页](RenderBatching.md)

