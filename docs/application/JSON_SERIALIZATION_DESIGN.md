[返回文档首页](../README.md)

---

# JSON序列化系统设计文档

**创建时间**: 2025-11-25  
**最后更新**: 2025-11-25  
**状态**: 已完成

---

## 1. 设计概述

### 1.1 背景

在开发过程中，多个模块（UI主题、场景数据、配置管理等）都需要实现数据的序列化和持久化存储。为避免重复开发，提高代码复用率，我们设计并实现了一个通用的JSON序列化工具系统。

### 1.2 设计目标

1. **代码复用**：提供统一的序列化基础设施，多个模块共享
2. **类型安全**：充分利用C++类型系统，编译时检查类型错误
3. **易于扩展**：通过简单的函数对即可为新类型添加序列化支持
4. **统一错误处理**：集中的错误处理和日志记录机制
5. **格式友好**：生成可读性好的JSON文件，便于人工编辑和调试

### 1.3 技术选型

- **JSON库**: [nlohmann/json](https://github.com/nlohmann/json) v3.11.3
  - 单头文件，易于集成
  - 现代C++接口，支持C++11/14/17
  - 完善的类型转换和错误处理
  - 广泛使用，社区活跃

---

## 2. 架构设计

### 2.1 系统架构图

```
┌─────────────────────────────────────────────────────────┐
│                    应用层模块                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
│  │  UI主题系统  │  │  场景管理器  │  │  配置系统   │      │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘      │
└─────────┼─────────────────┼─────────────────┼────────────┘
          │                 │                 │
          └─────────────────┼─────────────────┘
                           │
┌──────────────────────────┼──────────────────────────────┐
│           序列化层       │                               │
│  ┌───────────────────────▼───────────────────────┐      │
│  │  ui_theme_serialization.h                     │      │
│  │  (UITheme相关类型的to_json/from_json)         │      │
│  └───────────────────────────────────────────────┘      │
│  ┌───────────────────────────────────────────────┐      │
│  │  scene_serialization.h                        │      │
│  │  (场景相关类型的to_json/from_json)            │      │
│  └───────────────────────────────────────────────┘      │
│  ┌───────────────────────────────────────────────┐      │
│  │  ...更多序列化模块                             │      │
│  └───────────────────────────────────────────────┘      │
└──────────────────────────┬──────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────┐
│          基础设施层                                      │
│  ┌───────────────────────────────────────────────┐      │
│  │  json_serializer.h                            │      │
│  │  - JsonSerializer (文件读写、字符串解析)      │      │
│  │  - 基础类型to_json/from_json                  │      │
│  │    (Color, Vector2/3/4, Quaternion, Rect)    │      │
│  └───────────────────────────────────────────────┘      │
│  ┌───────────────────────────────────────────────┐      │
│  │  nlohmann/json.hpp                            │      │
│  │  (第三方JSON库)                               │      │
│  └───────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────┘
```

### 2.2 核心组件

#### 2.2.1 JsonSerializer（基础设施层）

**职责**:
- 提供文件读写功能
- 提供字符串解析功能
- 统一的错误处理和日志记录

**接口设计**:
```cpp
class JsonSerializer {
public:
    static bool LoadFromFile(const std::string& filepath, nlohmann::json& outJson);
    static bool SaveToFile(const nlohmann::json& json, const std::string& filepath, int indent = 4);
    static bool ParseFromString(const std::string& jsonStr, nlohmann::json& outJson);
    static std::string ToString(const nlohmann::json& json, int indent = 4);
};
```

#### 2.2.2 基础类型序列化（基础设施层）

为常用的基础类型提供序列化支持：

```cpp
// Color
void to_json(nlohmann::json& j, const Color& color);
void from_json(const nlohmann::json& j, Color& color);

// Vector2/3/4
void to_json(nlohmann::json& j, const Vector2& vec);
void from_json(const nlohmann::json& j, Vector2& vec);
// ... Vector3, Vector4

// Quaternion
void to_json(nlohmann::json& j, const Quaternion& quat);
void from_json(const nlohmann::json& j, Quaternion& quat);

// Rect
void to_json(nlohmann::json& j, const Rect& rect);
void from_json(const nlohmann::json& j, Rect& rect);
```

#### 2.2.3 模块特定序列化（序列化层）

每个模块在独立的头文件中定义其类型的序列化支持：

**ui_theme_serialization.h**:
```cpp
namespace Render::UI {
    void to_json(nlohmann::json& j, const UIThemeColorSet& colorSet);
    void from_json(const nlohmann::json& j, UIThemeColorSet& colorSet);
    
    void to_json(nlohmann::json& j, const UIThemeWidgetColors& widgetColors);
    void from_json(const nlohmann::json& j, UIThemeWidgetColors& widgetColors);
    
    void to_json(nlohmann::json& j, const UITheme& theme);
    void from_json(const nlohmann::json& j, UITheme& theme);
}
```

---

## 3. 技术实现

### 3.1 ADL（Argument-Dependent Lookup）机制

nlohmann/json库利用C++的ADL机制自动查找序列化函数：

```cpp
namespace MyNamespace {
    struct MyType { int value; };
    
    // 在相同命名空间中定义
    void to_json(nlohmann::json& j, const MyType& t) {
        j = {{"value", t.value}};
    }
    
    void from_json(const nlohmann::json& j, MyType& t) {
        j.at("value").get_to(t.value);
    }
}

// 使用时会自动找到对应的序列化函数
MyNamespace::MyType obj;
nlohmann::json j = obj;  // 自动调用to_json
auto obj2 = j.get<MyNamespace::MyType>();  // 自动调用from_json
```

### 3.2 模板元编程和类型推导

```cpp
// 泛型序列化接口
template<typename T>
T JsonSerializer::Load(const std::string& filepath) {
    nlohmann::json j;
    LoadFromFile(filepath, j);
    return j.get<T>();  // 自动推导类型并调用对应的from_json
}

template<typename T>
bool JsonSerializer::Save(const T& object, const std::string& filepath) {
    nlohmann::json j = object;  // 自动调用to_json
    return SaveToFile(j, filepath);
}
```

### 3.3 错误处理策略

**分层错误处理**:

1. **基础设施层**（JsonSerializer）：
   - 捕获文件IO异常
   - 捕获JSON解析异常
   - 记录详细的错误日志
   - 返回布尔值表示成功/失败

2. **序列化层**（to_json/from_json）：
   - 使用`at()`访问必需字段（不存在会抛异常）
   - 使用`value()`访问可选字段（提供默认值）
   - 抛出异常由上层捕获

3. **应用层**：
   - 检查返回值
   - 使用try-catch处理异常
   - 提供回退机制（如使用默认配置）

**示例**:
```cpp
// 应用层代码
UITheme theme;
if (!UITheme::LoadFromJSON("theme.json", theme)) {
    // 加载失败，使用默认主题
    theme = UITheme::CreateDefault();
    Logger::GetInstance().Warning("Failed to load theme, using default");
}
```

### 3.4 版本兼容性设计

在JSON中包含版本字段，支持未来升级：

```json
{
    "version": "1.0",
    "data": { ... }
}
```

```cpp
void from_json(const nlohmann::json& j, UITheme& theme) {
    std::string version = j.value("version", "1.0");
    
    if (version == "1.0") {
        // 解析v1.0格式
    } else if (version == "2.0") {
        // 解析v2.0格式，并转换为内部格式
    } else {
        throw std::runtime_error("Unsupported version: " + version);
    }
}
```

---

## 4. UI主题序列化实现

### 4.1 数据结构层次

```
UITheme
├── UIThemeWidgetColors button
│   ├── UIThemeColorSet normal
│   │   ├── Color outline
│   │   ├── Color inner
│   │   ├── Color innerSelected
│   │   ├── Color item
│   │   ├── Color text
│   │   ├── Color textSelected
│   │   └── Color shadow
│   ├── UIThemeColorSet hover
│   ├── UIThemeColorSet pressed
│   ├── UIThemeColorSet disabled
│   └── UIThemeColorSet active
├── UIThemeWidgetColors textField
├── UIThemeWidgetColors panel
├── UIThemeWidgetColors menu
├── UIThemeFontStyle widget
│   ├── string family
│   ├── float size
│   ├── bool bold
│   └── bool italic
├── UIThemeFontStyle widgetLabel
├── UIThemeFontStyle menuFont
├── UIThemeSizes sizes
│   ├── float widgetUnit
│   ├── float panelSpace
│   ├── float buttonHeight
│   ├── float textFieldHeight
│   ├── float spacing
│   └── float padding
├── Color backgroundColor
└── Color borderColor
```

### 4.2 序列化实现

**自底向上的序列化链**:

1. Color → JSON数组 `[r, g, b, a]`
2. UIThemeColorSet → JSON对象（包含7个颜色）
3. UIThemeWidgetColors → JSON对象（包含5个状态）
4. UITheme → 完整的JSON文档

**示例JSON输出** (简化版):
```json
{
    "version": "1.0",
    "colors": {
        "button": {
            "normal": {
                "inner": [0.92, 0.92, 0.92, 1.0],
                "text": [0.2, 0.2, 0.2, 1.0]
            }
        }
    },
    "fonts": {
        "widget": {
            "family": "NotoSansSC-Regular",
            "size": 14.0
        }
    },
    "sizes": {
        "buttonHeight": 40.0
    }
}
```

---

## 5. 扩展到其他模块

### 5.1 场景序列化

**场景数据结构**:
```cpp
struct TransformComponent {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
};

struct MeshComponent {
    std::string meshPath;
    std::string materialPath;
    int layerID;
};

struct Entity {
    std::string name;
    TransformComponent transform;
    std::vector<Component*> components;
};
```

**序列化实现**:
```cpp
// scene_serialization.h
namespace Render {
    void to_json(nlohmann::json& j, const TransformComponent& t) {
        j = {
            {"position", t.position},
            {"rotation", t.rotation},
            {"scale", t.scale}
        };
    }
    
    void from_json(const nlohmann::json& j, TransformComponent& t) {
        j.at("position").get_to(t.position);
        j.at("rotation").get_to(t.rotation);
        j.at("scale").get_to(t.scale);
    }
    
    void to_json(nlohmann::json& j, const Entity& e) {
        j = {
            {"name", e.name},
            {"transform", e.transform},
            {"components", nlohmann::json::array()}
        };
        // 序列化各种组件...
    }
}
```

### 5.2 配置系统

```cpp
struct GraphicsConfig {
    int windowWidth;
    int windowHeight;
    bool fullscreen;
    bool vsync;
    int msaaSamples;
};

// 使用nlohmann提供的宏简化代码
NLOHMANN_DEFINE_TYPE_INTRUSIVE(GraphicsConfig, 
    windowWidth, windowHeight, fullscreen, vsync, msaaSamples)

// 使用
GraphicsConfig config;
nlohmann::json j;
JsonSerializer::LoadFromFile("config.json", j);
config = j.get<GraphicsConfig>();
```

---

## 6. 性能考虑

### 6.1 序列化性能

- **开销**: JSON文本解析比二进制格式慢
- **优化**: 
  - 缓存解析结果，避免重复加载
  - 对于大文件考虑流式解析
  - 关键路径使用二进制格式，配置文件使用JSON

### 6.2 内存使用

- nlohmann/json使用std::map存储对象，内存开销较大
- 对于大量数据，考虑使用SAX解析器（事件驱动）

### 6.3 文件大小

- 格式化输出（indent=4）会增加文件大小约30%
- 发布版本可以使用紧凑格式（indent=0）
- 考虑压缩存储（如gzip）

---

## 7. 测试策略

### 7.1 单元测试

```cpp
TEST(JsonSerializerTest, ColorSerialization) {
    Color original(1.0f, 0.5f, 0.0f, 1.0f);
    
    // 序列化
    nlohmann::json j = original;
    
    // 反序列化
    Color loaded = j.get<Color>();
    
    // 验证
    EXPECT_FLOAT_EQ(original.r, loaded.r);
    EXPECT_FLOAT_EQ(original.g, loaded.g);
    EXPECT_FLOAT_EQ(original.b, loaded.b);
    EXPECT_FLOAT_EQ(original.a, loaded.a);
}

TEST(UIThemeTest, SaveAndLoad) {
    UITheme original = UITheme::CreateDefault();
    
    // 保存
    ASSERT_TRUE(UITheme::SaveToJSON(original, "test_theme.json"));
    
    // 加载
    UITheme loaded;
    ASSERT_TRUE(UITheme::LoadFromJSON("test_theme.json", loaded));
    
    // 验证关键字段
    EXPECT_FLOAT_EQ(original.sizes.buttonHeight, loaded.sizes.buttonHeight);
    EXPECT_EQ(original.widget.family, loaded.widget.family);
}
```

### 7.2 集成测试

- 测试完整的保存→加载→使用流程
- 测试版本兼容性
- 测试错误处理（文件不存在、格式错误等）

---

## 8. 未来扩展

### 8.1 计划中的功能

1. **二进制序列化**：为关键数据提供更快的二进制格式
2. **增量序列化**：只保存修改过的字段
3. **压缩支持**：自动压缩/解压缩大文件
4. **验证器**：JSON Schema验证
5. **迁移工具**：自动迁移旧版本数据

### 8.2 可能的优化

1. **内存池**：减少频繁的内存分配
2. **延迟加载**：按需加载大型数据
3. **并行解析**：多线程解析大文件
4. **缓存机制**：缓存常用配置

---

## 9. 参考资料

- [nlohmann/json 官方文档](https://json.nlohmann.me/)
- [JSON规范 RFC 8259](https://www.rfc-editor.org/rfc/rfc8259.html)
- [C++ ADL机制](https://en.cppreference.com/w/cpp/language/adl)
- [UI主题系统设计](UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md)

---

## 10. 总结

通过设计和实现通用的JSON序列化系统，我们实现了：

1. **高代码复用率**：基础设施被多个模块共享
2. **类型安全**：编译时类型检查，减少运行时错误
3. **易于扩展**：新模块只需添加to_json/from_json函数对
4. **统一体验**：所有模块使用一致的序列化API

该系统已成功应用于UI主题系统，为场景管理、配置系统等其他模块提供了良好的参考和基础。

---

[返回文档首页](../README.md) | [使用指南](../JSON_SERIALIZATION_GUIDE.md) | [UI主题系统](UI_SYSTEM_BLENDER_REFERENCE_UPDATE_PLAN.md)


