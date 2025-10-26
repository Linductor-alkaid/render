# 渲染层级管理

## 目录
[返回文档首页](README.md)

## 概述

渲染层级系统允许您按照优先级组织渲染内容，类似于游戏引擎中的图层系统。每个层级可以独立启用/禁用，并且可以设置不同的渲染属性。

## 核心概念

### 层级优先级
- 优先级数值越小，渲染越早
- 典型层级优先级范围：0-1000
- 建议按100的倍数设置主要层级，方便插入中间层级

### 标准层级定义

```cpp
enum StandardLayers {
    BACKGROUND_LAYER = 100,   // 背景层
    FAR_ENVIRONMENT = 200,    // 远景环境
    WORLD_GEOMETRY = 300,     // 世界几何
    OPAQUE_OBJECTS = 400,     // 不透明物体
    TRANSPARENT_OBJECTS = 500, // 透明物体
    EFFECTS_LAYER = 600,       // 特效层
    OVERLAY_LAYER = 700,       // 叠加层
    UI_LAYER = 800,           // UI层
    DEBUG_LAYER = 900,        // 调试层
    TOP_LAYER = 1000          // 顶层
};
```

## 层级操作

### 创建和管理层级

```cpp
// 创建自定义层级
RenderLayer* skyboxLayer = renderer->CreateLayer(BACKGROUND_LAYER);
RenderLayer* terrainLayer = renderer->CreateLayer(WORLD_GEOMETRY);
RenderLayer* characterLayer = renderer->CreateLayer(OPAQUE_OBJECTS);
RenderLayer* particleLayer = renderer->CreateLayer(EFFECTS_LAYER);
RenderLayer* hudLayer = renderer->CreateLayer(UI_LAYER);

// 设置层级属性
void SetupLayers() {
    // 天空盒层级 - 深度测试禁用
    skyboxLayer->EnableDepthTest(false);
    skyboxLayer->SetBlendMode(BlendMode::None);
    
    // 地形层级 - 正常渲染
    terrainLayer->EnableDepthTest(true);
    terrainLayer->EnableBackfaceCulling(true);
    
    // 角色层级 - 可包含透明物体
    characterLayer->SetSortMode(SortMode::StateThenDistance);
    characterLayer->EnableDepthTest(true);
    
    // 粒子层级 - 混合渲染
    particleLayer->SetBlendMode(BlendMode::Additive);
    particleLayer->SetSortMode(SortMode::Distance);
    particleLayer->EnableDepthTest(true);
    particleLayer->EnableDepthWrite(false);
    
    // UI层级 - 无深度测试
    hudLayer->EnableDepthTest(false);
    hudLayer->SetBlendMode(BlendMode::Alpha);
}
```

### 启用/禁用层级

```cpp
// 切换层级可见性
if (showUI) {
    hudLayer->SetEnabled(true);
} else {
    hudLayer->SetEnabled(false);
}

// 暂停特效渲染
particleLayer->SetEnabled(false);

// 恢复渲染
particleLayer->SetEnabled(true);
```

### 动态添加/移除物体

```cpp
// 将物体添加到层级
MeshRenderable* object = renderer->CreateMeshRenderable();
object->SetLayer(characterLayer);
object->SetVisible(true);

// 从层级移除
object->SetLayer(nullptr);
object->SetVisible(false);

// 重新分配层级
object->SetLayer(particleLayer);
```

## 层级排序策略

### 状态优先排序
减少着色器和纹理的状态切换，提高性能。

```cpp
characterLayer->SetSortMode(SortMode::StateThenDistance);
```

排序规则：
1. 材质ID排序（减少着色器切换）
2. 纹理ID排序（减少纹理绑定）
3. 距离排序（从近到远）

### 深度优先排序
用于透明物体的正确渲染。

```cpp
particleLayer->SetSortMode(SortMode::Distance);
```

排序规则：
从摄像机距离由远到近。

### 自定义排序

```cpp
// 自定义排序函数
bool CustomSort(Renderable* a, Renderable* b) {
    // 先按层级内部优先级
    if (a->GetLayerPriority() != b->GetLayerPriority()) {
        return a->GetLayerPriority() < b->GetLayerPriority();
    }
    
    // 再按材质
    if (a->GetMaterial()->GetID() != b->GetMaterial()->GetID()) {
        return a->GetMaterial()->GetID() < b->GetMaterial()->GetID();
    }
    
    // 最后按距离
    float distA = calculateDistance(a);
    float distB = calculateDistance(b);
    return distA > distB;
}

characterLayer->SetCustomSortFunction(CustomSort);
```

## 层级渲染属性

### 深度测试配置

```cpp
// 启用深度测试（默认）
layer->EnableDepthTest(true);

// 禁用深度测试
layer->EnableDepthTest(false);

// 设置深度比较函数
layer->SetDepthFunc(DepthFunc::LessEqual);

// 启用/禁用深度写入
layer->EnableDepthWrite(true);
```

### 混合模式配置

```cpp
// 不混合（默认）
layer->SetBlendMode(BlendMode::None);

// Alpha混合
layer->SetBlendMode(BlendMode::Alpha);

// 加法混合（用于火焰、光晕等）
layer->SetBlendMode(BlendMode::Additive);

// 乘法混合
layer->SetBlendMode(BlendMode::Multiply);

// 自定义混合函数
layer->SetBlendFunc(BlendFunc::SrcAlpha, BlendFunc::OneMinusSrcAlpha);
```

### 面剔除配置

```cpp
// 启用背面剔除（默认）
layer->EnableBackfaceCulling(true);

// 只剔除正面
layer->SetCullFace(CullFace::Front);

// 只剔除背面
layer->SetCullFace(CullFace::Back);

// 双向渲染（不剔除）
layer->EnableBackfaceCulling(false);
```

### 视口配置

```cpp
// 设置视口（可选）
layer->SetViewport(0, 0, 1920, 1080);

// 启用裁剪
layer->EnableScissorTest(true);
layer->SetScissorRect(100, 100, 800, 600);
```

## 层级可见性控制

### 遮罩系统

```cpp
// 设置层级遮罩
uint32_t mainCameraMask = (1 << characterLayer->GetID()) | 
                          (1 << terrainLayer->GetID());
uint32_t uiCameraMask = (1 << hudLayer->GetID());

// 设置相机遮罩
mainCamera->SetLayerMask(mainCameraMask);
uiCamera->SetLayerMask(uiCameraMask);
```

### 条件渲染

```cpp
// 基于条件渲染
class ConditionalLayer : public RenderLayer {
public:
    void Render(RenderContext* context) override {
        if (ShouldRender()) {
            RenderLayer::Render(context);
        }
    }
    
private:
    bool ShouldRender() const {
        // 检查各种条件
        return isVisible && distance < maxDistance;
    }
};
```

## 性能优化

### 层级剔除

```cpp
// 启用视锥剔除
layer->EnableFrustumCulling(true);

// 启用遮挡剔除（需要硬件支持）
layer->EnableOcclusionCulling(true);

// 设置距离剔除
layer->SetMaxRenderDistance(1000.0f);
layer->SetMinRenderDistance(0.0f);
```

### 批处理优化

```cpp
// 启用批处理
layer->EnableBatching(true);
layer->SetMaxBatchSize(1000);

// 设置批处理策略
layer->SetBatchStrategy(BatchStrategy::StateBased);
```

### LOD 支持

```cpp
// 为层级设置LOD距离
layer->SetLODLevels({
    {0.0f, 100.0f},   // 距离0-100使用LOD0
    {100.0f, 300.0f}, // 距离100-300使用LOD1
    {300.0f, 1000.0f} // 距离300-1000使用LOD2
});
```

## 实际应用示例

### 游戏场景分层

```cpp
void SetupGameScene() {
    // 天空盒
    RenderLayer* skybox = renderer->CreateLayer(BACKGROUND_LAYER);
    skybox->EnableDepthTest(false);
    
    // 地形
    RenderLayer* terrain = renderer->CreateLayer(WORLD_GEOMETRY);
    terrain->EnableBackfaceCulling(true);
    
    // 建筑物
    RenderLayer* buildings = renderer->CreateLayer(OPAQUE_OBJECTS);
    buildings->SetSortMode(SortMode::StateThenDistance);
    
    // 角色
    RenderLayer* characters = renderer->CreateLayer(OPAQUE_OBJECTS + 10);
    
    // 玻璃等透明物体
    RenderLayer* glass = renderer->CreateLayer(TRANSPARENT_OBJECTS);
    glass->SetBlendMode(BlendMode::Alpha);
    glass->SetSortMode(SortMode::Distance);
    
    // 烟雾、火焰等粒子
    RenderLayer* particles = renderer->CreateLayer(EFFECTS_LAYER);
    particles->SetBlendMode(BlendMode::Additive);
    particles->SetSortMode(SortMode::Distance);
    particles->EnableDepthWrite(false);
    
    // 过场动画装饰
    RenderLayer* overlay = renderer->CreateLayer(OVERLAY_LAYER);
    
    // UI
    RenderLayer* ui = renderer->CreateLayer(UI_LAYER);
    ui->EnableDepthTest(false);
    
    // 调试信息
    RenderLayer* debug = renderer->CreateLayer(DEBUG_LAYER);
    debug->EnableDepthTest(false);
}
```

### 菜单系统分层

```cpp
void SetupMenuLayers() {
    // 背景
    RenderLayer* bg = renderer->CreateLayer(100);
    bg->EnableDepthTest(false);
    
    // 菜单项
    RenderLayer* menuItems = renderer->CreateLayer(200);
    menuItems->EnableDepthTest(false);
    
    // 按钮高亮
    RenderLayer* highlights = renderer->CreateLayer(250);
    highlights->SetBlendMode(BlendMode::Alpha);
    
    // 弹窗
    RenderLayer* popups = renderer->CreateLayer(300);
    
    // 提示文本
    RenderLayer* tooltips = renderer->CreateLayer(400);
}
```

## 层级调试

### 可视化层级

```cpp
// 显示层级信息
void DebugRenderLayers() {
    for (auto* layer : renderer->GetAllLayers()) {
        if (layer->IsEnabled()) {
            RenderStats stats = layer->GetStats();
            std::cout << "Layer: " << layer->GetName() 
                      << " | Priority: " << layer->GetPriority()
                      << " | Objects: " << stats.objectCount
                      << " | Draw Calls: " << stats.drawCalls << std::endl;
        }
    }
}
```

### 性能分析

```cpp
// 分析层级性能
void ProfileLayers() {
    renderer->BeginProfiling();
    
    // 渲染一帧
    renderer->RenderFrame();
    
    renderer->EndProfiling();
    
    // 获取各层级耗时
    for (auto* layer : renderer->GetAllLayers()) {
        float time = layer->GetLastRenderTime();
        std::cout << "Layer " << layer->GetName() 
                  << " took " << time << "ms" << std::endl;
    }
}
```

---

[返回文档首页](README.md) | [上一篇: API 参考](API_REFERENCE.md) | [下一篇: ECS 集成](ECS_INTEGRATION.md)

