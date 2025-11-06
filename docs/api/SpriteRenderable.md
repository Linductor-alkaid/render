# SpriteRenderable API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md) | [返回 Renderable](Renderable.md)

---

## 📋 概述

SpriteRenderable 是用于渲染 2D 精灵的可渲染对象，继承自 `Renderable` 基类。它支持纹理、UV 映射、大小调整、着色等功能，适用于 UI、2D 游戏等场景。

**命名空间**：`Render`

**头文件**：`<render/renderable.h>`

---

## 🏗️ 类定义

```cpp
class SpriteRenderable : public Renderable {
public:
    SpriteRenderable();
    ~SpriteRenderable() override = default;
    
    // 禁止拷贝
    SpriteRenderable(const SpriteRenderable&) = delete;
    SpriteRenderable& operator=(const SpriteRenderable&) = delete;
    
    // 支持移动
    SpriteRenderable(SpriteRenderable&& other) noexcept;
    SpriteRenderable& operator=(SpriteRenderable&& other) noexcept;
    
    // 渲染
    void Render() override;
    void SubmitToRenderer(Renderer* renderer) override;
    
    // 纹理
    void SetTexture(const Ref<Texture>& texture);
    Ref<Texture> GetTexture() const;
    
    // 显示属性
    void SetSourceRect(const Rect& rect);
    Rect GetSourceRect() const;
    
    void SetSize(const Vector2& size);
    Vector2 GetSize() const;
    
    void SetTintColor(const Color& color);
    Color GetTintColor() const;
    
    // 包围盒
    AABB GetBoundingBox() const override;
    
private:
    Ref<Texture> m_texture;
    Rect m_sourceRect{0, 0, 1, 1};  // UV 坐标
    Vector2 m_size{1.0f, 1.0f};     // 显示大小
    Color m_tintColor{1, 1, 1, 1};  // 着色颜色
};
```

---

## 🔧 成员函数详解

### 构造函数

#### `SpriteRenderable()`

构造函数，创建 2D 精灵渲染对象。

```cpp
SpriteRenderable();
```

**说明**：
- 自动设置类型为 `RenderableType::Sprite`
- 默认可见
- 默认层级为 `800`（UI_LAYER）
- 默认源矩形为 `{0, 0, 1, 1}`（整个纹理）
- 默认大小为 `{1.0f, 1.0f}`
- 默认着色为白色（不改变纹理颜色）

**示例**：
```cpp
SpriteRenderable sprite;
```

---

### 渲染

#### `Render()`

渲染精灵。

```cpp
void Render() override;
```

**说明**：
- 检查可见性和纹理是否有效
- 实现 2D 精灵渲染（当前版本为占位实现，将在后续阶段完善）

**示例**：
```cpp
sprite.Render();
```

#### `SubmitToRenderer()`

提交到渲染器。

```cpp
void SubmitToRenderer(Renderer* renderer) override;
```

**参数**：
- `renderer` - 渲染器指针

**说明**：
- 将自己提交到渲染队列
- 通常由 `SpriteRenderSystem` 调用

**示例**：
```cpp
sprite.SubmitToRenderer(renderer);
```

---

### 纹理

#### `SetTexture()` / `GetTexture()`

设置/获取纹理对象。

```cpp
void SetTexture(const Ref<Texture>& texture);
Ref<Texture> GetTexture() const;
```

**参数**：
- `texture` - 纹理对象（`std::shared_ptr<Texture>`）

**示例**：
```cpp
// 加载纹理
auto texture = TextureLoader::LoadFromFile("textures/player.png");
sprite.SetTexture(texture);

// 获取纹理
auto texture = sprite.GetTexture();
if (texture) {
    int width = texture->GetWidth();
    int height = texture->GetHeight();
}
```

---

### 显示属性

#### `SetSourceRect()` / `GetSourceRect()`

设置/获取源矩形（UV 坐标）。

```cpp
void SetSourceRect(const Rect& rect);
Rect GetSourceRect() const;
```

**参数**：
- `rect` - 源矩形，格式为 `{x, y, width, height}`，范围 [0, 1]

**说明**：
- 用于纹理图集（Texture Atlas）
- 只渲染纹理的一部分
- UV 坐标从左上角 (0, 0) 到右下角 (1, 1)

**示例**：
```cpp
// 整个纹理
sprite.SetSourceRect({0, 0, 1, 1});

// 纹理的左上角四分之一
sprite.SetSourceRect({0, 0, 0.5f, 0.5f});

// 纹理图集中的某一帧（假设 4x4 网格）
int frameX = 2;  // 第 3 列
int frameY = 1;  // 第 2 行
sprite.SetSourceRect({
    frameX * 0.25f, 
    frameY * 0.25f, 
    0.25f, 
    0.25f
});
```

#### `SetSize()` / `GetSize()`

设置/获取显示大小。

```cpp
void SetSize(const Vector2& size);
Vector2 GetSize() const;
```

**参数**：
- `size` - 显示大小（世界单位或屏幕像素）

**说明**：
- 控制精灵在屏幕上的显示大小
- 不影响源纹理

**示例**：
```cpp
// 设置大小为 64x64
sprite.SetSize(Vector2(64, 64));

// 根据纹理尺寸设置
auto texture = sprite.GetTexture();
if (texture) {
    sprite.SetSize(Vector2(texture->GetWidth(), texture->GetHeight()));
}

// 获取大小
Vector2 size = sprite.GetSize();
```

#### `SetTintColor()` / `GetTintColor()`

设置/获取着色颜色。

```cpp
void SetTintColor(const Color& color);
Color GetTintColor() const;
```

**参数**：
- `color` - 着色颜色（RGBA，范围 [0, 1]）

**说明**：
- 与纹理颜色相乘
- 白色 `(1, 1, 1, 1)` 不改变纹理颜色
- 可用于实现淡入淡出、受伤闪烁等效果

**示例**：
```cpp
// 默认（不改变颜色）
sprite.SetTintColor(Color(1, 1, 1, 1));

// 红色着色
sprite.SetTintColor(Color(1, 0, 0, 1));

// 半透明
sprite.SetTintColor(Color(1, 1, 1, 0.5f));

// 淡入效果
for (float alpha = 0.0f; alpha <= 1.0f; alpha += 0.01f) {
    sprite.SetTintColor(Color(1, 1, 1, alpha));
    // 渲染...
}
```

---

### 包围盒

#### `GetBoundingBox()`

获取包围盒。

```cpp
AABB GetBoundingBox() const override;
```

**返回值**：轴对齐包围盒（AABB）。

**说明**：
- 计算 2D 精灵的包围盒（Z=0）
- 基于显示大小和位置
- 用于拾取检测、碰撞检测等

**实现**：
```cpp
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
```

**示例**：
```cpp
AABB bounds = sprite.GetBoundingBox();

// 拾取检测
bool IsPointInside(const Vector2& point, const SpriteRenderable& sprite) {
    AABB bounds = sprite.GetBoundingBox();
    return point.x() >= bounds.min.x() && point.x() <= bounds.max.x() &&
           point.y() >= bounds.min.y() && point.y() <= bounds.max.y();
}
```

---

## 🎯 完整使用示例

### 基本使用

```cpp
#include <render/renderable.h>
#include <render/texture_loader.h>

// 创建 SpriteRenderable
SpriteRenderable sprite;

// 设置变换
auto transform = std::make_shared<Transform>();
transform->SetPosition(Vector3(100, 100, 0));  // 屏幕坐标
sprite.SetTransform(transform);

// 加载纹理
auto texture = TextureLoader::LoadFromFile("textures/player.png");
sprite.SetTexture(texture);

// 设置显示属性
sprite.SetSize(Vector2(64, 64));
sprite.SetSourceRect({0, 0, 1, 1});  // 整个纹理
sprite.SetTintColor(Color(1, 1, 1, 1));  // 不着色

// 设置渲染属性
sprite.SetVisible(true);
sprite.SetLayerID(800);  // UI_LAYER

// 渲染
sprite.Render();
```

---

### 与 ECS 集成

在 ECS 系统中，`SpriteRenderable` 由 `SpriteRenderSystem` 自动创建和管理：

```cpp
// 创建实体
EntityID entity = world->CreateEntity({.name = "PlayerSprite"});

// 添加 Transform 组件
TransformComponent transform;
transform.SetPosition(Vector3(100, 100, 0));
world->AddComponent(entity, transform);

// 添加 SpriteRenderComponent
SpriteRenderComponent sprite;
sprite.textureName = "textures/player.png";  // 异步加载
sprite.size = Vector2(64, 64);
sprite.sourceRect = {0, 0, 1, 1};
sprite.tintColor = Color(1, 1, 1, 1);
sprite.visible = true;
sprite.layerID = 800;
world->AddComponent(entity, sprite);

// SpriteRenderSystem 会在每帧：
// 1. 查询所有具有 TransformComponent 和 SpriteRenderComponent 的实体
// 2. 创建 SpriteRenderable 对象
// 3. 设置 texture、size、tintColor 等
// 4. 提交到渲染队列
```

---

### 精灵动画

使用源矩形实现帧动画：

```cpp
class SpriteAnimator {
public:
    SpriteAnimator(SpriteRenderable* sprite, int frameCount, float fps)
        : m_sprite(sprite), m_frameCount(frameCount), m_fps(fps) {}
    
    void Update(float deltaTime) {
        m_time += deltaTime;
        
        float frameDuration = 1.0f / m_fps;
        if (m_time >= frameDuration) {
            m_time -= frameDuration;
            m_currentFrame = (m_currentFrame + 1) % m_frameCount;
            
            // 更新源矩形（假设帧水平排列）
            float frameWidth = 1.0f / m_frameCount;
            m_sprite->SetSourceRect({
                m_currentFrame * frameWidth,
                0,
                frameWidth,
                1.0f
            });
        }
    }
    
private:
    SpriteRenderable* m_sprite;
    int m_frameCount;
    float m_fps;
    int m_currentFrame = 0;
    float m_time = 0.0f;
};

// 使用
SpriteRenderable sprite;
sprite.SetTexture(spriteSheetTexture);  // 包含多帧的纹理
SpriteAnimator animator(&sprite, 8, 12.0f);  // 8帧，12 FPS

// 主循环
while (running) {
    animator.Update(deltaTime);
    sprite.Render();
}
```

---

### UI 按钮

```cpp
class Button {
public:
    Button(const Vector2& position, const Vector2& size, Ref<Texture> texture)
        : m_sprite(std::make_unique<SpriteRenderable>()) {
        
        auto transform = std::make_shared<Transform>();
        transform->SetPosition(Vector3(position.x(), position.y(), 0));
        m_sprite->SetTransform(transform);
        
        m_sprite->SetTexture(texture);
        m_sprite->SetSize(size);
        m_sprite->SetLayerID(800);  // UI_LAYER
        
        m_normalColor = Color(1, 1, 1, 1);
        m_hoverColor = Color(1.2f, 1.2f, 1.2f, 1);
        m_pressedColor = Color(0.8f, 0.8f, 0.8f, 1);
    }
    
    void Update(const Vector2& mousePos, bool mouseDown) {
        AABB bounds = m_sprite->GetBoundingBox();
        bool inside = mousePos.x() >= bounds.min.x() && mousePos.x() <= bounds.max.x() &&
                      mousePos.y() >= bounds.min.y() && mousePos.y() <= bounds.max.y();
        
        if (inside) {
            if (mouseDown) {
                m_sprite->SetTintColor(m_pressedColor);
                m_pressed = true;
            } else {
                m_sprite->SetTintColor(m_hoverColor);
                if (m_pressed) {
                    // 按钮被点击
                    OnClick();
                }
                m_pressed = false;
            }
        } else {
            m_sprite->SetTintColor(m_normalColor);
            m_pressed = false;
        }
    }
    
    void Render() {
        m_sprite->Render();
    }
    
    virtual void OnClick() {
        // 子类重写
    }
    
private:
    std::unique_ptr<SpriteRenderable> m_sprite;
    Color m_normalColor;
    Color m_hoverColor;
    Color m_pressedColor;
    bool m_pressed = false;
};
```

---

### 粒子系统（2D）

```cpp
class Particle2D {
public:
    SpriteRenderable sprite;
    Vector2 velocity;
    float lifetime = 0.0f;
    float maxLifetime = 1.0f;
};

class ParticleSystem2D {
public:
    void Emit(const Vector3& position, Ref<Texture> texture) {
        Particle2D particle;
        
        auto transform = std::make_shared<Transform>();
        transform->SetPosition(position);
        particle.sprite.SetTransform(transform);
        
        particle.sprite.SetTexture(texture);
        particle.sprite.SetSize(Vector2(8, 8));
        
        // 随机速度
        float angle = Random(0.0f, 360.0f) * 3.14159f / 180.0f;
        float speed = Random(50.0f, 100.0f);
        particle.velocity = Vector2(std::cos(angle) * speed, std::sin(angle) * speed);
        
        particle.maxLifetime = Random(0.5f, 1.5f);
        
        m_particles.push_back(particle);
    }
    
    void Update(float deltaTime) {
        for (auto it = m_particles.begin(); it != m_particles.end(); ) {
            auto& particle = *it;
            particle.lifetime += deltaTime;
            
            if (particle.lifetime >= particle.maxLifetime) {
                it = m_particles.erase(it);
                continue;
            }
            
            // 更新位置
            auto transform = particle.sprite.GetTransform();
            Vector3 pos = transform->GetPosition();
            pos.x() += particle.velocity.x() * deltaTime;
            pos.y() += particle.velocity.y() * deltaTime;
            transform->SetPosition(pos);
            
            // 淡出
            float alpha = 1.0f - (particle.lifetime / particle.maxLifetime);
            particle.sprite.SetTintColor(Color(1, 1, 1, alpha));
            
            ++it;
        }
    }
    
    void Render() {
        for (auto& particle : m_particles) {
            particle.sprite.Render();
        }
    }
    
private:
    std::vector<Particle2D> m_particles;
    
    float Random(float min, float max) {
        return min + (max - min) * (rand() / (float)RAND_MAX);
    }
};
```

---

## 💡 使用建议

### 1. 纹理图集

使用纹理图集提高性能：

```cpp
// ✅ 好：纹理图集（一次绘制调用）
Ref<Texture> atlas = TextureLoader::LoadFromFile("textures/ui_atlas.png");

SpriteRenderable button1;
button1.SetTexture(atlas);
button1.SetSourceRect({0, 0, 0.25f, 0.25f});  // 左上角

SpriteRenderable button2;
button2.SetTexture(atlas);
button2.SetSourceRect({0.25f, 0, 0.25f, 0.25f});  // 右边

// ❌ 差：每个精灵一个纹理（多次绘制调用）
button1.SetTexture(TextureLoader::LoadFromFile("button1.png"));
button2.SetTexture(TextureLoader::LoadFromFile("button2.png"));
```

### 2. 批次渲染

按纹理分组渲染：

```cpp
// ✅ 好：按纹理排序
std::sort(sprites.begin(), sprites.end(),
    [](const SpriteRenderable& a, const SpriteRenderable& b) {
        return a.GetTexture().get() < b.GetTexture().get();
    });

for (auto& sprite : sprites) {
    sprite.Render();
}

// ❌ 差：随机顺序（频繁切换纹理）
for (auto& sprite : sprites) {
    sprite.Render();
}
```

### 3. 层级管理

使用层级控制渲染顺序：

```cpp
// 背景层
backgroundSprite.SetLayerID(700);

// UI 层
uiSprite.SetLayerID(800);

// 覆盖层（如工具提示）
tooltipSprite.SetLayerID(900);
```

---

## 📊 性能优化

### 1. 对象池

```cpp
class SpritePool {
public:
    SpriteRenderable* Acquire() {
        if (m_freeList.empty()) {
            m_sprites.emplace_back();
            return &m_sprites.back();
        }
        
        SpriteRenderable* sprite = m_freeList.back();
        m_freeList.pop_back();
        return sprite;
    }
    
    void Release(SpriteRenderable* sprite) {
        m_freeList.push_back(sprite);
    }
    
private:
    std::vector<SpriteRenderable> m_sprites;
    std::vector<SpriteRenderable*> m_freeList;
};
```

### 2. 视口裁剪

```cpp
// 跳过屏幕外的精灵
bool IsOnScreen(const SpriteRenderable& sprite, const Rect& viewport) {
    AABB bounds = sprite.GetBoundingBox();
    
    return bounds.max.x() >= viewport.x &&
           bounds.min.x() <= viewport.x + viewport.width &&
           bounds.max.y() >= viewport.y &&
           bounds.min.y() <= viewport.y + viewport.height;
}
```

---

## 🔒 线程安全

`SpriteRenderable` 使用 `std::shared_mutex` 保护所有成员变量：

- 所有 getter 使用共享锁
- 所有 setter 使用独占锁
- `Render()` 使用共享锁

---

## 📖 相关文档

- [Renderable 基类](Renderable.md)
- [MeshRenderable](MeshRenderable.md)
- [Texture API](Texture.md)
- [Transform API](Transform.md)
- [ECS 概览](ECS.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md) | [返回 Renderable](Renderable.md)

