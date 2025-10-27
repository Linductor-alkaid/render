# Shader API 参考

[返回 API 首页](README.md)

---

## 概述

`Shader` 类管理 OpenGL 着色器程序的编译、链接和使用，支持顶点、片段和几何着色器。

**头文件**: `render/shader.h`  
**命名空间**: `Render`

---

## 类定义

```cpp
class Shader {
public:
    Shader();
    ~Shader();
    
    bool LoadFromFile(const std::string& vertexPath,
                      const std::string& fragmentPath,
                      const std::string& geometryPath = "");
    
    bool LoadFromSource(const std::string& vertexSource,
                        const std::string& fragmentSource,
                        const std::string& geometrySource = "");
    
    void Use() const;
    void Unuse() const;
    bool Reload();
    
    // ... 更多方法见下文
};
```

---

## 枚举类型

### ShaderType

着色器类型枚举。

```cpp
enum class ShaderType {
    Vertex,     // 顶点着色器
    Fragment,   // 片段着色器
    Geometry,   // 几何着色器
    Compute     // 计算着色器（框架支持）
};
```

---

## 构造和析构

### Shader

构造函数。

```cpp
Shader();
```

**说明**: 创建空的着色器对象。

---

### ~Shader

析构函数。

```cpp
~Shader();
```

**说明**: 自动清理 OpenGL 着色器程序。

---

## 加载方法

### LoadFromFile

从文件加载着色器。

```cpp
bool LoadFromFile(const std::string& vertexPath,
                  const std::string& fragmentPath,
                  const std::string& geometryPath = "");
```

**参数**:
- `vertexPath` - 顶点着色器文件路径
- `fragmentPath` - 片段着色器文件路径
- `geometryPath` - 几何着色器文件路径（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
Shader shader;
if (!shader.LoadFromFile("shaders/basic.vert", "shaders/basic.frag")) {
    LOG_ERROR("Failed to load shader");
    return false;
}
shader.SetName("BasicShader");
```

**几何着色器示例**:
```cpp
Shader geoShader;
if (!geoShader.LoadFromFile(
    "shaders/particle.vert", 
    "shaders/particle.frag",
    "shaders/particle.geom")) {  // 几何着色器
    LOG_ERROR("Failed to load geometry shader");
}
```

---

### LoadFromSource

从源码字符串加载着色器。

```cpp
bool LoadFromSource(const std::string& vertexSource,
                    const std::string& fragmentSource,
                    const std::string& geometrySource = "");
```

**参数**:
- `vertexSource` - 顶点着色器源码
- `fragmentSource` - 片段着色器源码
- `geometrySource` - 几何着色器源码（可选）

**返回值**: 成功返回 `true`，失败返回 `false`

**示例**:
```cpp
std::string vertexSource = R"(
    #version 450 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

std::string fragmentSource = R"(
    #version 450 core
    out vec4 FragColor;
    uniform vec4 color;
    
    void main() {
        FragColor = color;
    }
)";

Shader shader;
if (!shader.LoadFromSource(vertexSource, fragmentSource)) {
    LOG_ERROR("Failed to load shader from source");
}
```

---

## 使用方法

### Use

激活此着色器程序。

```cpp
void Use() const;
```

**说明**: 调用 `glUseProgram()`，使此着色器生效。

**示例**:
```cpp
shader.Use();

// 设置 uniform
shader.GetUniformManager()->SetMatrix4("projection", projMatrix);
shader.GetUniformManager()->SetVector3("lightPos", lightPos);

// 渲染...
glDrawArrays(GL_TRIANGLES, 0, 36);

shader.Unuse();
```

---

### Unuse

解除着色器使用。

```cpp
void Unuse() const;
```

**说明**: 调用 `glUseProgram(0)`。

---

### Reload

重新加载着色器（热重载）。

```cpp
bool Reload();
```

**返回值**: 成功返回 `true`，失败返回 `false`

**说明**: 
- 仅对通过 `LoadFromFile()` 加载的着色器有效
- 从保存的文件路径重新读取和编译

**示例**:
```cpp
// 按 R 键重载着色器
if (event.key.key == SDLK_R) {
    if (shader.Reload()) {
        LOG_INFO("Shader reloaded successfully");
    } else {
        LOG_ERROR("Failed to reload shader");
    }
}
```

---

## 查询方法

### IsValid

检查着色器是否有效。

```cpp
bool IsValid() const;
```

**返回值**: 着色器有效返回 `true`

**示例**:
```cpp
if (shader.IsValid()) {
    shader.Use();
    // 渲染...
}
```

---

### GetProgramID

获取 OpenGL 程序 ID。

```cpp
uint32_t GetProgramID() const;
```

**返回值**: OpenGL 着色器程序 ID

**示例**:
```cpp
uint32_t programID = shader.GetProgramID();
LOG_INFO("Shader program ID: " + std::to_string(programID));
```

---

### GetName

获取着色器名称。

```cpp
const std::string& GetName() const;
```

**返回值**: 着色器名称

---

### SetName

设置着色器名称。

```cpp
void SetName(const std::string& name);
```

**参数**:
- `name` - 着色器名称

**示例**:
```cpp
shader.SetName("TerrainShader");
LOG_INFO("Using shader: " + shader.GetName());
```

---

## Uniform 管理

### GetUniformManager

获取 Uniform 管理器。

```cpp
UniformManager* GetUniformManager();
const UniformManager* GetUniformManager() const;
```

**返回值**: UniformManager 指针

**示例**:
```cpp
shader.Use();

auto* uniformMgr = shader.GetUniformManager();

// 设置矩阵
uniformMgr->SetMatrix4("projection", projMatrix);
uniformMgr->SetMatrix4("view", viewMatrix);
uniformMgr->SetMatrix4("model", modelMatrix);

// 设置向量
uniformMgr->SetVector3("lightPos", Vector3(1.0f, 2.0f, 3.0f));
uniformMgr->SetColor("objectColor", Color(1.0f, 0.5f, 0.2f, 1.0f));

// 设置标量
uniformMgr->SetFloat("time", currentTime);
uniformMgr->SetInt("texture1", 0);
```

---

## 完整示例

### 基础着色器使用

```cpp
#include "render/shader.h"
#include "render/logger.h"

// 加载着色器
Shader shader;
if (!shader.LoadFromFile("shaders/basic.vert", "shaders/basic.frag")) {
    LOG_ERROR("Failed to load shader");
    return -1;
}
shader.SetName("BasicShader");

// 打印 uniform 信息
LOG_INFO("Shader uniforms:");
shader.GetUniformManager()->PrintUniformInfo();

// 主循环
while (running) {
    // 使用着色器
    shader.Use();
    
    // 设置 uniform
    shader.GetUniformManager()->SetMatrix4("projection", projMatrix);
    shader.GetUniformManager()->SetMatrix4("view", viewMatrix);
    shader.GetUniformManager()->SetMatrix4("model", modelMatrix);
    shader.GetUniformManager()->SetVector3("viewPos", cameraPos);
    
    // 渲染
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    shader.Unuse();
}
```

---

### 几何着色器示例

```cpp
// 加载带几何着色器的着色器程序
Shader geoShader;
if (!geoShader.LoadFromFile(
    "shaders/point_to_quad.vert",
    "shaders/point_to_quad.frag",
    "shaders/point_to_quad.geom")) {  // 几何着色器
    LOG_ERROR("Failed to load geometry shader");
    return -1;
}

geoShader.Use();

// 设置几何着色器的 uniform
geoShader.GetUniformManager()->SetFloat("quadSize", 0.1f);
geoShader.GetUniformManager()->SetMatrix4("projection", projMatrix);
geoShader.GetUniformManager()->SetMatrix4("view", viewMatrix);

// 渲染点，几何着色器会将其扩展为四边形
glBindVertexArray(pointVAO);
glDrawArrays(GL_POINTS, 0, numPoints);
glBindVertexArray(0);

geoShader.Unuse();
```

---

### 运行时源码编译

```cpp
// 动态生成着色器
std::string GenerateFragmentShader(const std::vector<Light>& lights) {
    std::stringstream ss;
    ss << "#version 450 core\n";
    ss << "out vec4 FragColor;\n";
    ss << "uniform vec3 objectColor;\n";
    
    for (size_t i = 0; i < lights.size(); ++i) {
        ss << "uniform vec3 light" << i << "Pos;\n";
        ss << "uniform vec3 light" << i << "Color;\n";
    }
    
    ss << "void main() {\n";
    ss << "    vec3 result = vec3(0.0);\n";
    
    for (size_t i = 0; i < lights.size(); ++i) {
        ss << "    result += light" << i << "Color;\n";
    }
    
    ss << "    FragColor = vec4(result * objectColor, 1.0);\n";
    ss << "}\n";
    
    return ss.str();
}

// 使用生成的着色器
std::string vertSource = LoadVertexShaderTemplate();
std::string fragSource = GenerateFragmentShader(sceneLights);

Shader runtimeShader;
if (runtimeShader.LoadFromSource(vertSource, fragSource)) {
    LOG_INFO("Runtime shader compiled successfully");
}
```

---

## 着色器示例

### 基础顶点着色器

```glsl
#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

---

### 基础片段着色器

```glsl
#version 450 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // 环境光
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
    
    // 漫反射
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // 镜面反射
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;
    
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
```

---

### 几何着色器示例

```glsl
#version 450 core

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT {
    vec4 color;
} gs_in[];

out vec4 fragColor;

uniform float quadSize = 0.1;

void main()
{
    vec4 position = gl_in[0].gl_Position;
    vec4 color = gs_in[0].color;
    
    // 生成四个顶点构成四边形
    
    // 左下
    gl_Position = position + vec4(-quadSize, -quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 右下
    gl_Position = position + vec4(quadSize, -quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 左上
    gl_Position = position + vec4(-quadSize, quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    // 右上
    gl_Position = position + vec4(quadSize, quadSize, 0.0, 0.0);
    fragColor = color;
    EmitVertex();
    
    EndPrimitive();
}
```

---

## 错误处理

着色器编译或链接失败时，错误信息会自动记录到日志：

```cpp
Shader shader;
if (!shader.LoadFromFile("shaders/broken.vert", "shaders/broken.frag")) {
    // 查看日志文件获取详细错误信息
    LOG_ERROR("Shader compilation failed. Check logs for details.");
}
```

**常见错误**:
1. **文件未找到**: 检查文件路径是否正确
2. **语法错误**: 检查 GLSL 语法
3. **版本不匹配**: 确保使用 `#version 450 core`
4. **Uniform 未找到**: 检查 uniform 名称拼写

---

## 最佳实践

1. **命名规范**: 使用有意义的着色器名称
2. **错误检查**: 始终检查 `LoadFromFile()` 的返回值
3. **Use/Unuse**: 配对使用 `Use()` 和 `Unuse()`
4. **热重载**: 开发时使用 `Reload()` 快速迭代
5. **缓存系统**: 生产环境使用 [ShaderCache](ShaderCache.md) 管理着色器

---

## 相关文档

- [UniformManager API](UniformManager.md)
- [ShaderCache API](ShaderCache.md)
- [示例程序: 02_shader_test](../../examples/02_shader_test.cpp)
- [示例程序: 03_geometry_shader_test](../../examples/03_geometry_shader_test.cpp)

---

[上一篇: Renderer](Renderer.md) | [下一篇: ShaderCache](ShaderCache.md)

