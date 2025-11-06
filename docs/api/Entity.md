# Entity API 参考

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

---

## 📋 概述

Entity（实体）是 ECS 架构的核心概念之一，它是一个轻量级的 ID，用于关联组件。实体本身不包含任何数据或逻辑，仅作为组件的容器。

**命名空间**：`Render::ECS`

**头文件**：
- `<render/ecs/entity.h>` - EntityID 和 EntityDescriptor
- `<render/ecs/entity_manager.h>` - EntityManager

---

## 🏷️ EntityID

实体 ID 类型，使用 64 位存储（32 位索引 + 32 位版本号）。

### 结构定义

```cpp
struct EntityID {
    uint32_t index;      // 实体索引
    uint32_t version;    // 版本号（用于检测悬空引用）
    
    bool IsValid() const;
    
    bool operator==(const EntityID& other) const;
    bool operator!=(const EntityID& other) const;
    bool operator<(const EntityID& other) const;
    
    struct Hash {
        size_t operator()(const EntityID& id) const;
    };
    
    static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    static EntityID Invalid();
};
```

### 成员变量

| 名称 | 类型 | 说明 |
|------|------|------|
| `index` | `uint32_t` | 实体在数组中的索引 |
| `version` | `uint32_t` | 版本号，用于检测悬空引用 |

### 成员函数

#### `IsValid()`

检查实体 ID 是否有效。

```cpp
bool IsValid() const;
```

**返回值**：如果索引不等于 `INVALID_INDEX`，返回 `true`。

**示例**：
```cpp
EntityID entity = world->CreateEntity();
if (entity.IsValid()) {
    // 实体有效，可以使用
}
```

#### 比较运算符

```cpp
bool operator==(const EntityID& other) const;
bool operator!=(const EntityID& other) const;
bool operator<(const EntityID& other) const;
```

**示例**：
```cpp
EntityID a = world->CreateEntity();
EntityID b = world->CreateEntity();

if (a != b) {
    // 两个不同的实体
}
```

#### `Hash`

用于在 `std::unordered_map` 中使用 EntityID 作为键。

```cpp
struct Hash {
    size_t operator()(const EntityID& id) const;
};
```

**示例**：
```cpp
std::unordered_map<EntityID, MyData, EntityID::Hash> entityData;
entityData[entity] = myData;
```

#### `Invalid()`

创建无效的实体 ID。

```cpp
static EntityID Invalid();
```

**返回值**：无效的 EntityID（index = INVALID_INDEX）。

**示例**：
```cpp
EntityID entity = EntityID::Invalid();
assert(!entity.IsValid());
```

---

## 📝 EntityDescriptor

实体描述符，用于创建实体时设置初始属性。

### 结构定义

```cpp
struct EntityDescriptor {
    std::string name;                    // 实体名称
    bool active = true;                  // 是否激活
    std::vector<std::string> tags;       // 标签列表
};
```

### 成员变量

| 名称 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `name` | `std::string` | `""` | 实体名称（用于调试） |
| `active` | `bool` | `true` | 是否激活 |
| `tags` | `std::vector<std::string>` | `{}` | 标签列表 |

### 示例

```cpp
// 创建相机实体
EntityDescriptor cameraDesc;
cameraDesc.name = "MainCamera";
cameraDesc.active = true;
cameraDesc.tags = {"camera", "main"};

EntityID camera = world->CreateEntity(cameraDesc);
```

---

## 🎛️ EntityManager

实体管理器，负责实体的创建、销毁和查询。

**🆕 性能优化（v1.1）**：内部使用 `IsValidNoLock()` 避免递归锁，提升性能约 5-10%。

### 类定义

```cpp
class EntityManager {
public:
    EntityManager();
    ~EntityManager();
    
    // 实体创建/销毁
    EntityID CreateEntity(const EntityDescriptor& desc = {});
    void DestroyEntity(EntityID entity);
    bool IsValid(EntityID entity) const;
    
    // 实体信息
    void SetName(EntityID entity, const std::string& name);
    std::string GetName(EntityID entity) const;
    void SetActive(EntityID entity, bool active);
    bool IsActive(EntityID entity) const;
    
    // 标签系统
    void AddTag(EntityID entity, const std::string& tag);
    void RemoveTag(EntityID entity, const std::string& tag);
    bool HasTag(EntityID entity, const std::string& tag) const;
    std::vector<std::string> GetTags(EntityID entity) const;
    
    // 查询
    std::vector<EntityID> GetAllEntities() const;
    std::vector<EntityID> GetEntitiesWithTag(const std::string& tag) const;
    std::vector<EntityID> GetActiveEntities() const;
    
    // 统计
    size_t GetEntityCount() const;
    size_t GetActiveEntityCount() const;
    void Clear();
    
private:
    // 内部优化方法（避免递归锁）
    bool IsValidNoLock(EntityID entity) const;
};
```

### 特性说明

#### 线程安全

- ✅ 所有公共方法都使用 `std::shared_mutex` 保护
- ✅ 支持多读单写（multiple readers, single writer）
- ✅ 内部优化避免递归锁，提升性能

#### 版本号机制

- ✅ 有效防止悬空引用（stale references）
- ✅ 实体删除后版本号自动递增
- ✅ 索引复用时版本号不同

#### 内存优化

- ✅ 使用空闲索引队列复用已删除实体的索引
- ✅ 减少内存碎片化
- ✅ 提升实体创建性能

---

## 🔧 成员函数详解

### 实体创建/销毁

#### `CreateEntity()`

创建新实体。

```cpp
EntityID CreateEntity(const EntityDescriptor& desc = {});
```

**参数**：
- `desc` - 实体描述符（可选）

**返回值**：新创建的实体 ID。

**示例**：
```cpp
// 简单创建
EntityID entity = entityManager.CreateEntity();

// 使用描述符
EntityID player = entityManager.CreateEntity({
    .name = "Player",
    .active = true,
    .tags = {"player", "controllable"}
});
```

#### `DestroyEntity()`

销毁实体。

```cpp
void DestroyEntity(EntityID entity);
```

**参数**：
- `entity` - 要销毁的实体 ID

**说明**：
- 销毁实体后，其索引会被加入空闲队列，可以被复用
- 版本号会递增，以检测悬空引用

**示例**：
```cpp
EntityID entity = entityManager.CreateEntity();
// ... 使用实体 ...
entityManager.DestroyEntity(entity);
```

#### `IsValid()`

检查实体是否有效。

```cpp
bool IsValid(EntityID entity) const;
```

**参数**：
- `entity` - 实体 ID

**返回值**：如果实体有效且版本号匹配，返回 `true`。

**示例**：
```cpp
EntityID entity = entityManager.CreateEntity();
if (entityManager.IsValid(entity)) {
    // 实体有效
}
```

---

### 实体信息

#### `SetName()` / `GetName()`

设置/获取实体名称。

```cpp
void SetName(EntityID entity, const std::string& name);
std::string GetName(EntityID entity) const;
```

**示例**：
```cpp
entityManager.SetName(entity, "Player");
std::string name = entityManager.GetName(entity);
```

#### `SetActive()` / `IsActive()`

设置/获取实体激活状态。

```cpp
void SetActive(EntityID entity, bool active);
bool IsActive(EntityID entity) const;
```

**说明**：
- 非激活的实体不会被系统处理
- 适用于临时禁用实体

**示例**：
```cpp
// 禁用实体
entityManager.SetActive(entity, false);

// 检查是否激活
if (entityManager.IsActive(entity)) {
    // 实体处于激活状态
}
```

---

### 标签系统

#### `AddTag()` / `RemoveTag()`

添加/移除标签。

```cpp
void AddTag(EntityID entity, const std::string& tag);
void RemoveTag(EntityID entity, const std::string& tag);
```

**示例**：
```cpp
entityManager.AddTag(entity, "enemy");
entityManager.AddTag(entity, "flying");
entityManager.RemoveTag(entity, "flying");
```

#### `HasTag()`

检查实体是否有指定标签。

```cpp
bool HasTag(EntityID entity, const std::string& tag) const;
```

**示例**：
```cpp
if (entityManager.HasTag(entity, "player")) {
    // 这是玩家实体
}
```

#### `GetTags()`

获取实体的所有标签。

```cpp
std::vector<std::string> GetTags(EntityID entity) const;
```

**示例**：
```cpp
auto tags = entityManager.GetTags(entity);
for (const auto& tag : tags) {
    std::cout << tag << std::endl;
}
```

---

### 查询

#### `GetAllEntities()`

获取所有实体（包括非激活实体）。

```cpp
std::vector<EntityID> GetAllEntities() const;
```

**示例**：
```cpp
auto allEntities = entityManager.GetAllEntities();
std::cout << "Total entities: " << allEntities.size() << std::endl;
```

#### `GetEntitiesWithTag()`

获取具有指定标签的实体。

```cpp
std::vector<EntityID> GetEntitiesWithTag(const std::string& tag) const;
```

**示例**：
```cpp
auto enemies = entityManager.GetEntitiesWithTag("enemy");
for (auto enemy : enemies) {
    // 处理每个敌人
}
```

#### `GetActiveEntities()`

获取所有激活的实体。

```cpp
std::vector<EntityID> GetActiveEntities() const;
```

**示例**：
```cpp
auto activeEntities = entityManager.GetActiveEntities();
```

---

### 统计

#### `GetEntityCount()` / `GetActiveEntityCount()`

获取实体数量。

```cpp
size_t GetEntityCount() const;
size_t GetActiveEntityCount() const;
```

**示例**：
```cpp
std::cout << "Total: " << entityManager.GetEntityCount() << std::endl;
std::cout << "Active: " << entityManager.GetActiveEntityCount() << std::endl;
```

#### `Clear()`

清除所有实体。

```cpp
void Clear();
```

**示例**：
```cpp
entityManager.Clear();
```

---

## 💡 设计要点

### 1. 版本号机制

实体使用版本号机制防止悬空引用：

```cpp
EntityID entity = entityManager.CreateEntity();  // version = 0
entityManager.DestroyEntity(entity);             // version 递增为 1
// 此时旧的 entity (version = 0) 无效
bool valid = entityManager.IsValid(entity);      // false
```

### 2. 索引复用

删除实体后，索引会被加入空闲队列，可以被复用：

```cpp
EntityID e1 = entityManager.CreateEntity();  // index = 0
EntityID e2 = entityManager.CreateEntity();  // index = 1
entityManager.DestroyEntity(e1);             // index 0 加入空闲队列
EntityID e3 = entityManager.CreateEntity();  // 复用 index 0，version = 1
```

### 3. 标签索引

标签系统使用内部索引，支持 O(1) 查询：

```cpp
// 内部维护：标签 -> 实体列表的映射
std::unordered_map<std::string, std::unordered_set<EntityID>> m_tagIndex;
```

---

## 🔒 线程安全

`EntityManager` 使用 `std::shared_mutex` 保护所有操作，支持多读单写：

```cpp
// 读操作（共享锁）
std::shared_lock lock(m_mutex);
return m_entities[entity.index].active;

// 写操作（独占锁）
std::unique_lock lock(m_mutex);
m_entities[entity.index].active = active;
```

---

## 📖 相关文档

- [ECS 概览](ECS.md)
- [Component API](Component.md)
- [World API](World.md)

---

[返回 API 目录](README.md) | [返回 ECS 概览](ECS.md)

