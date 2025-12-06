# JSON序列化工具使用指南

[返回文档首页](../README.md)

---

## 1. 概述

本项目提供了统一的JSON序列化/反序列化工具，用于实现数据的持久化存储和配置管理。该工具基于[nlohmann/json](https://github.com/nlohmann/json)库实现，支持UI主题、场景数据、配置文件等多种数据的序列化。

### 1.1 设计原则

1. **代码复用**：提供通用的序列化基础设施，避免重复实现
2. **类型安全**：使用C++模板和类型推导，确保编译时类型检查
3. **易于扩展**：通过ADL（Argument-Dependent Lookup）机制，轻松为自定义类型添加序列化支持
4. **错误处理**：统一的错误处理和日志记录
5. **格式友好**：生成格式化的JSON文件，便于人工阅读和编辑

### 1.2 核心组件

- **JsonSerializer**：核心工具类，提供文件读写和字符串解析功能
- **to_json/from_json**：序列化函数对，为各种类型提供序列化支持
- **基础类型支持**：Color、Vector2、Vector3、Vector4、Quaternion、Rect等
- **主题序列化**：UITheme及其子结构的完整序列化支持

---

## 2. 基础用法

### 2.1 包含头文件

```cpp
#include "render/json_serializer.h"
```

### 2.2 保存JSON到文件

```cpp
#include "render/json_serializer.h"
#include <nlohmann/json.hpp>

// 创建JSON对象
nlohmann::json j = {
    {"name", "MyData"},
    {"value", 42},
    {"enabled", true}
};

// 保存到文件（缩进4空格，便于阅读）
if (JsonSerializer::SaveToFile(j, "data.json")) {
    std::cout << "保存成功！" << std::endl;
}
```

### 2.3 从文件加载JSON

```cpp
#include "render/json_serializer.h"
#include <nlohmann/json.hpp>

nlohmann::json j;
if (JsonSerializer::LoadFromFile("data.json", j)) {
    std::string name = j["name"].get<std::string>();
    int value = j["value"].get<int>();
    bool enabled = j["enabled"].get<bool>();
    std::cout << "加载成功：" << name << ", " << value << std::endl;
}
```

### 2.4 字符串解析

```cpp
std::string jsonStr = R"({"x": 10, "y": 20})";
nlohmann::json j;
if (JsonSerializer::ParseFromString(jsonStr, j)) {
    int x = j["x"].get<int>();
    int y = j["y"].get<int>();
}
```

---

## 3. 基础类型序列化

项目已为常用的基础类型提供了序列化支持，可以直接使用。

### 3.1 Color（颜色）

支持数组格式和对象格式：

```cpp
Color color(1.0f, 0.5f, 0.0f, 1.0f);

// 序列化为JSON（数组格式）
nlohmann::json j = color;
// 结果：[1.0, 0.5, 0.0, 1.0]

// 从JSON反序列化
Color loadedColor = j.get<Color>();

// 也支持对象格式
nlohmann::json j2 = {
    {"r", 1.0},
    {"g", 0.5},
    {"b", 0.0},
    {"a", 1.0}
};
Color color2 = j2.get<Color>();
```

### 3.2 Vector2/Vector3/Vector4

```cpp
Vector3 position(1.0f, 2.0f, 3.0f);

// 序列化
nlohmann::json j = position;
// 结果：[1.0, 2.0, 3.0]

// 反序列化
Vector3 loadedPos = j.get<Vector3>();

// 也支持对象格式
nlohmann::json j2 = {{"x", 1.0}, {"y", 2.0}, {"z", 3.0}};
Vector3 pos2 = j2.get<Vector3>();
```

### 3.3 Quaternion（四元数）

```cpp
Quaternion rotation(0.0f, 0.707f, 0.0f, 0.707f);

// 序列化
nlohmann::json j = rotation;
// 结果：[0.0, 0.707, 0.0, 0.707] (x, y, z, w)

// 反序列化
Quaternion loadedRot = j.get<Quaternion>();
```

### 3.4 Rect（矩形）

```cpp
Rect rect(10.0f, 20.0f, 100.0f, 50.0f);

// 序列化
nlohmann::json j = rect;
// 结果：{"x": 10.0, "y": 20.0, "width": 100.0, "height": 50.0}

// 反序列化
Rect loadedRect = j.get<Rect>();
```

---

## 4. UI主题序列化

### 4.1 保存主题到JSON

```cpp
#include "render/ui/ui_theme.h"
#include "render/ui/ui_theme_serialization.h"

// 创建主题
UITheme theme = UITheme::CreateDefault();

// 保存到文件
if (UITheme::SaveToJSON(theme, "themes/my_theme.json")) {
    std::cout << "主题保存成功！" << std::endl;
}
```

### 4.2 从JSON加载主题

```cpp
#include "render/ui/ui_theme.h"

UITheme theme;
if (UITheme::LoadFromJSON("themes/my_theme.json", theme)) {
    std::cout << "主题加载成功！" << std::endl;
    // 使用加载的主题
    UIThemeManager::GetInstance().RegisterBuiltinTheme("custom", theme);
}
```

### 4.3 主题JSON格式示例

```json
{
    "version": "1.0",
    "colors": {
        "button": {
            "normal": {
                "outline": [0.2, 0.2, 0.2, 1.0],
                "inner": [0.92, 0.92, 0.92, 1.0],
                "text": [0.2, 0.2, 0.2, 1.0]
            },
            "hover": {
                "outline": [0.2, 0.2, 0.2, 1.0],
                "inner": [0.96, 0.96, 0.96, 1.0],
                "text": [0.2, 0.2, 0.2, 1.0]
            }
        }
    },
    "fonts": {
        "widget": {
            "family": "NotoSansSC-Regular",
            "size": 14.0,
            "bold": false,
            "italic": false
        }
    },
    "sizes": {
        "widgetUnit": 20.0,
        "buttonHeight": 40.0,
        "spacing": 8.0
    }
}
```

详细的主题JSON结构请参考 `themes/default.json` 和 `themes/dark.json` 示例文件。

---

## 5. 为自定义类型添加序列化支持

### 5.1 简单结构体序列化

使用ADL（Argument-Dependent Lookup）机制，在与类型相同的命名空间中定义`to_json`和`from_json`函数：

```cpp
namespace MyApp {

struct PlayerData {
    std::string name;
    int level;
    float health;
    Render::Vector3 position;
};

// 序列化函数（struct -> json）
void to_json(nlohmann::json& j, const PlayerData& player) {
    j = {
        {"name", player.name},
        {"level", player.level},
        {"health", player.health},
        {"position", player.position}  // Vector3自动序列化
    };
}

// 反序列化函数（json -> struct）
void from_json(const nlohmann::json& j, PlayerData& player) {
    j.at("name").get_to(player.name);
    j.at("level").get_to(player.level);
    j.at("health").get_to(player.health);
    j.at("position").get_to(player.position);
}

} // namespace MyApp
```

### 5.2 使用示例

```cpp
using namespace MyApp;

// 创建数据
PlayerData player;
player.name = "Hero";
player.level = 10;
player.health = 100.0f;
player.position = Render::Vector3(5.0f, 0.0f, 3.0f);

// 保存
nlohmann::json j = player;  // 自动调用to_json
Render::JsonSerializer::SaveToFile(j, "player.json");

// 加载
nlohmann::json j2;
if (Render::JsonSerializer::LoadFromFile("player.json", j2)) {
    PlayerData loadedPlayer = j2.get<PlayerData>();  // 自动调用from_json
}
```

### 5.3 使用nlohmann提供的宏简化代码

对于简单的结构体，可以使用nlohmann/json提供的宏：

```cpp
struct Config {
    int width;
    int height;
    bool fullscreen;
    
    // 自动生成to_json和from_json
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, width, height, fullscreen)
};
```

---

## 6. 场景序列化示例

### 6.1 保存场景数据

```cpp
#include "render/json_serializer.h"

void SaveScene(const Scene& scene, const std::string& path) {
    nlohmann::json j;
    j["name"] = scene.name;
    j["entities"] = nlohmann::json::array();
    
    for (const auto& entity : scene.entities) {
        nlohmann::json entityJson = {
            {"name", entity.name},
            {"transform", {
                {"position", entity.transform.position},
                {"rotation", entity.transform.rotation},
                {"scale", entity.transform.scale}
            }},
            {"components", nlohmann::json::array()}
        };
        
        // 序列化各种组件
        if (entity.HasComponent<MeshComponent>()) {
            const auto& mesh = entity.GetComponent<MeshComponent>();
            entityJson["components"].push_back({
                {"type", "MeshComponent"},
                {"meshPath", mesh.meshPath},
                {"materialPath", mesh.materialPath}
            });
        }
        
        j["entities"].push_back(entityJson);
    }
    
    JsonSerializer::SaveToFile(j, path);
}
```

### 6.2 加载场景数据

```cpp
void LoadScene(Scene& scene, const std::string& path) {
    nlohmann::json j;
    if (!JsonSerializer::LoadFromFile(path, j)) {
        return;
    }
    
    scene.name = j.value("name", "Untitled");
    scene.entities.clear();
    
    for (const auto& entityJson : j["entities"]) {
        Entity entity;
        entity.name = entityJson.value("name", "Entity");
        
        // 加载变换组件
        if (entityJson.contains("transform")) {
            const auto& t = entityJson["transform"];
            entity.transform.position = t["position"].get<Vector3>();
            entity.transform.rotation = t["rotation"].get<Quaternion>();
            entity.transform.scale = t["scale"].get<Vector3>();
        }
        
        // 加载其他组件
        for (const auto& compJson : entityJson["components"]) {
            std::string type = compJson["type"];
            if (type == "MeshComponent") {
                MeshComponent mesh;
                mesh.meshPath = compJson["meshPath"];
                mesh.materialPath = compJson["materialPath"];
                entity.AddComponent(mesh);
            }
            // ... 处理其他组件类型
        }
        
        scene.entities.push_back(entity);
    }
}
```

---

## 7. 最佳实践

### 7.1 错误处理

始终检查返回值并处理错误：

```cpp
nlohmann::json j;
if (!JsonSerializer::LoadFromFile("config.json", j)) {
    // 使用默认配置
    j = GetDefaultConfig();
}

try {
    MyData data = j.get<MyData>();
} catch (const std::exception& e) {
    Logger::GetInstance().Error("Failed to parse data: " + std::string(e.what()));
    // 使用默认数据
    data = GetDefaultData();
}
```

### 7.2 版本兼容性

在JSON中包含版本信息，便于未来升级：

```cpp
void to_json(nlohmann::json& j, const MyData& data) {
    j = {
        {"version", "1.0"},
        {"data", data.value}
    };
}

void from_json(const nlohmann::json& j, MyData& data) {
    std::string version = j.value("version", "1.0");
    if (version == "1.0") {
        data.value = j["data"];
    } else if (version == "2.0") {
        // 处理新版本格式
    }
}
```

### 7.3 可选字段

使用`value()`方法处理可选字段，提供默认值：

```cpp
void from_json(const nlohmann::json& j, MyData& data) {
    data.requiredField = j.at("required");  // 必需字段，不存在会抛异常
    data.optionalField = j.value("optional", 42);  // 可选字段，不存在使用默认值
}
```

### 7.4 性能考虑

- 对于大量数据，考虑使用流式解析
- 避免频繁的序列化/反序列化操作
- 缓存序列化结果，仅在数据变化时重新序列化

---

## 8. 常见问题

### 8.1 编译错误：找不到to_json/from_json

确保：
1. 在与类型相同的命名空间中定义函数
2. 在使用前包含定义这些函数的头文件
3. 函数签名正确（参数类型和const修饰符）

### 8.2 运行时错误：解析失败

常见原因：
- JSON格式错误（缺少逗号、括号不匹配等）
- 类型不匹配（如将字符串解析为数字）
- 必需字段缺失

使用try-catch捕获异常并查看错误信息。

### 8.3 如何调试序列化问题

```cpp
// 打印JSON内容
nlohmann::json j = myData;
std::cout << j.dump(4) << std::endl;  // 格式化输出

// 检查字段是否存在
if (j.contains("fieldName")) {
    // 字段存在
}

// 检查类型
if (j["field"].is_number()) {
    // 是数字类型
}
```

---

## 9. 参考资源

- [nlohmann/json 官方文档](https://json.nlohmann.me/)
- [nlohmann/json GitHub](https://github.com/nlohmann/json)
- [UI主题系统文档](application/UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md)
- [场景序列化示例](ECS_INTEGRATION.md#组件序列化)

---

## 10. 总结

通过使用统一的JSON序列化工具：

1. **提高代码复用**：避免重复实现文件读写和解析逻辑
2. **简化开发**：通过简单的to_json/from_json函数对即可实现序列化
3. **类型安全**：利用C++类型系统，编译时检查类型错误
4. **易于维护**：集中管理序列化逻辑，便于升级和修改

该工具已成功应用于UI主题系统，未来可扩展到场景、材质、配置等更多模块。

---

[返回文档首页](README.md) | [UI主题系统](application/UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md) | [场景序列化](ECS_INTEGRATION.md)


