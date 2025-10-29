# UniformManager API 参考

[返回 API 首页](README.md)

---

## 概述

`UniformManager` 统一管理着色器的 uniform 变量，提供类型安全的接口，并缓存 uniform 位置以提高性能。

**头文件**: `render/uniform_manager.h`  
**命名空间**: `Render`

**说明**: 每个 `Shader` 对象内部包含一个 `UniformManager` 实例。

---

## 类定义

```cpp
class UniformManager {
public:
    UniformManager(uint32_t programID);
    ~UniformManager();
    
    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetBool(const std::string& name, bool value);
    
    void SetVector2(const std::string& name, const Vector2& value);
    void SetVector3(const std::string& name, const Vector3& value);
    void SetVector4(const std::string& name, const Vector4& value);
    
    void SetMatrix3(const std::string& name, const Matrix3& value);
    void SetMatrix4(const std::string& name, const Matrix4& value);
    
    void SetColor(const std::string& name, const Color& value);
    
    // ... 更多方法见下文
};
```

---

## 标量类型

### SetInt

设置整型 uniform。

```cpp
void SetInt(const std::string& name, int value);
```

**示例**:
```cpp
uniformMgr->SetInt("textureSlot", 0);
uniformMgr->SetInt("numLights", 4);
```

---

### SetFloat

设置浮点型 uniform。

```cpp
void SetFloat(const std::string& name, float value);
```

**示例**:
```cpp
uniformMgr->SetFloat("time", currentTime);
uniformMgr->SetFloat("roughness", 0.5f);
uniformMgr->SetFloat("metallic", 0.0f);
```

---

### SetBool

设置布尔型 uniform。

```cpp
void SetBool(const std::string& name, bool value);
```

**说明**: 内部转换为 int（0 或 1）。

**示例**:
```cpp
uniformMgr->SetBool("useTexture", true);
uniformMgr->SetBool("enableLighting", false);
```

---

## 向量类型

### SetVector2

设置二维向量 uniform。

```cpp
void SetVector2(const std::string& name, const Vector2& value);
```

**示例**:
```cpp
uniformMgr->SetVector2("resolution", Vector2(1280.0f, 720.0f));
uniformMgr->SetVector2("mousePos", Vector2(mouseX, mouseY));
```

---

### SetVector3

设置三维向量 uniform。

```cpp
void SetVector3(const std::string& name, const Vector3& value);
```

**示例**:
```cpp
uniformMgr->SetVector3("lightPos", Vector3(1.0f, 2.0f, 3.0f));
uniformMgr->SetVector3("cameraPos", cameraPosition);
uniformMgr->SetVector3("objectScale", Vector3(2.0f, 2.0f, 2.0f));
```

---

### SetVector4

设置四维向量 uniform。

```cpp
void SetVector4(const std::string& name, const Vector4& value);
```

**示例**:
```cpp
uniformMgr->SetVector4("clipPlane", Vector4(0.0f, 1.0f, 0.0f, 0.0f));
```

---

## 颜色

### SetColor

设置颜色 uniform。

```cpp
void SetColor(const std::string& name, const Color& value);
```

**说明**: `Color` 是 RGBA 四元组（每个分量 0.0~1.0）。

**示例**:
```cpp
uniformMgr->SetColor("objectColor", Color(1.0f, 0.5f, 0.2f, 1.0f));
uniformMgr->SetColor("tint", Color(1.0f, 1.0f, 1.0f, 0.5f));  // 半透明白色
```

---

## 矩阵类型

### SetMatrix3

设置 3x3 矩阵 uniform。

```cpp
void SetMatrix3(const std::string& name, const Matrix3& value);
```

**示例**:
```cpp
// 法线矩阵
Matrix3 normalMatrix = modelMatrix.block<3,3>(0,0).inverse().transpose();
uniformMgr->SetMatrix3("normalMatrix", normalMatrix);
```

---

### SetMatrix4

设置 4x4 矩阵 uniform。

```cpp
void SetMatrix4(const std::string& name, const Matrix4& value);
```

**示例**:
```cpp
uniformMgr->SetMatrix4("model", modelMatrix);
uniformMgr->SetMatrix4("view", viewMatrix);
uniformMgr->SetMatrix4("projection", projectionMatrix);

// MVP 矩阵
Matrix4 mvp = projectionMatrix * viewMatrix * modelMatrix;
uniformMgr->SetMatrix4("mvp", mvp);
```

---

## 数组类型

### SetIntArray

设置整型数组 uniform。

```cpp
void SetIntArray(const std::string& name, const int* values, uint32_t count);
```

**示例**:
```cpp
int textures[4] = {0, 1, 2, 3};
uniformMgr->SetIntArray("textures", textures, 4);
```

---

### SetFloatArray

设置浮点数组 uniform。

```cpp
void SetFloatArray(const std::string& name, const float* values, uint32_t count);
```

**示例**:
```cpp
float weights[8] = {0.2f, 0.15f, 0.1f, 0.05f, ...};
uniformMgr->SetFloatArray("blurWeights", weights, 8);
```

---

### SetVector3Array

设置 Vector3 数组 uniform。

```cpp
void SetVector3Array(const std::string& name, const Vector3* values, uint32_t count);
```

**示例**:
```cpp
// 多个光源位置
Vector3 lightPositions[4] = {
    Vector3(1.0f, 2.0f, 0.0f),
    Vector3(-1.0f, 2.0f, 0.0f),
    Vector3(0.0f, 2.0f, 1.0f),
    Vector3(0.0f, 2.0f, -1.0f)
};
uniformMgr->SetVector3Array("lightPositions", lightPositions, 4);
```

---

### SetMatrix4Array

设置 Matrix4 数组 uniform。

```cpp
void SetMatrix4Array(const std::string& name, const Matrix4* values, uint32_t count);
```

**示例**:
```cpp
// 骨骼动画矩阵
Matrix4 boneMatrices[100];
// ... 计算骨骼变换
uniformMgr->SetMatrix4Array("boneMatrices", boneMatrices, 100);
```

---

## 查询方法

### HasUniform

检查 uniform 是否存在。

```cpp
bool HasUniform(const std::string& name) const;
```

**返回值**: 存在返回 `true`

**示例**:
```cpp
if (uniformMgr->HasUniform("normalMatrix")) {
    uniformMgr->SetMatrix3("normalMatrix", normalMatrix);
}
```

---

### GetUniformLocation

获取 uniform 位置。

```cpp
int GetUniformLocation(const std::string& name);
```

**返回值**: uniform 位置，未找到返回 -1

**说明**: 
- 位置会自动缓存
- 通常不需要手动调用此方法

**示例**:
```cpp
int loc = uniformMgr->GetUniformLocation("color");
if (loc != -1) {
    // uniform 存在
}
```

---

### GetAllUniformNames

获取所有 uniform 名称。

```cpp
std::vector<std::string> GetAllUniformNames() const;
```

**返回值**: uniform 名称列表

**示例**:
```cpp
auto uniforms = uniformMgr->GetAllUniformNames();
for (const auto& name : uniforms) {
    LOG_INFO("Uniform: " + name);
}
```

---

### PrintUniformInfo

打印所有 uniform 信息（调试用）。

```cpp
void PrintUniformInfo() const;
```

**输出示例**:
```
Active uniforms in shader:
  - model (location: 0)
  - view (location: 1)
  - projection (location: 2)
  - lightPos (location: 3)
  - lightColor (location: 4)
```

**示例**:
```cpp
shader->Use();
shader->GetUniformManager()->PrintUniformInfo();
```

---

### ClearCache

清除位置缓存。

```cpp
void ClearCache();
```

**说明**: 通常不需要调用，着色器重新编译时会自动清除。

---

## 完整示例

### 基础光照着色器

```cpp
// 着色器代码
// vertex shader
const char* vertexShader = R"(
#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// fragment shader
const char* fragmentShader = R"(
#version 450 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform vec3 viewPos;

void main() {
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
)";

// 使用
shader.Use();
auto* uniformMgr = shader.GetUniformManager();

// 设置矩阵
uniformMgr->SetMatrix4("model", modelMatrix);
uniformMgr->SetMatrix4("view", viewMatrix);
uniformMgr->SetMatrix4("projection", projectionMatrix);

// 设置光照参数
uniformMgr->SetVector3("lightPos", Vector3(1.2f, 1.0f, 2.0f));
uniformMgr->SetVector3("lightColor", Vector3(1.0f, 1.0f, 1.0f));
uniformMgr->SetVector3("objectColor", Vector3(1.0f, 0.5f, 0.31f));
uniformMgr->SetVector3("viewPos", cameraPos);

// 渲染
glDrawArrays(GL_TRIANGLES, 0, 36);
```

---

### 多光源场景

```cpp
// 着色器中定义
/*
#define MAX_LIGHTS 8

uniform int numLights;
uniform vec3 lightPositions[MAX_LIGHTS];
uniform vec3 lightColors[MAX_LIGHTS];
uniform float lightIntensities[MAX_LIGHTS];
*/

// C++ 代码
std::vector<Vector3> lightPos = {
    Vector3(2.0f, 1.0f, 0.0f),
    Vector3(-2.0f, 1.0f, 0.0f),
    Vector3(0.0f, 1.0f, 2.0f),
};

std::vector<Vector3> lightCol = {
    Vector3(1.0f, 0.0f, 0.0f),  // 红
    Vector3(0.0f, 1.0f, 0.0f),  // 绿
    Vector3(0.0f, 0.0f, 1.0f),  // 蓝
};

std::vector<float> lightInt = {1.0f, 0.8f, 0.6f};

shader.Use();
auto* uniformMgr = shader.GetUniformManager();

uniformMgr->SetInt("numLights", lightPos.size());
uniformMgr->SetVector3Array("lightPositions", lightPos.data(), lightPos.size());
uniformMgr->SetVector3Array("lightColors", lightCol.data(), lightCol.size());
uniformMgr->SetFloatArray("lightIntensities", lightInt.data(), lightInt.size());
```

---

### 动画和时间

```cpp
// 着色器
/*
uniform float time;
uniform float animationSpeed;
uniform vec2 resolution;
*/

shader.Use();
auto* uniformMgr = shader.GetUniformManager();

// 时间
float time = SDL_GetTicks() / 1000.0f;
uniformMgr->SetFloat("time", time);
uniformMgr->SetFloat("animationSpeed", 2.0f);

// 分辨率
uniformMgr->SetVector2("resolution", 
    Vector2(renderer->GetWidth(), renderer->GetHeight()));

// 动画参数
float wave = std::sin(time * 2.0f);
uniformMgr->SetFloat("waveOffset", wave);
```

---

## 性能优化

### 位置缓存

`UniformManager` 自动缓存 uniform 位置，避免重复查询：

```cpp
// 第一次调用 - 查询并缓存位置
uniformMgr->SetFloat("time", 1.0f);  // glGetUniformLocation("time")

// 后续调用 - 直接使用缓存
uniformMgr->SetFloat("time", 2.0f);  // 使用缓存的位置
uniformMgr->SetFloat("time", 3.0f);  // 使用缓存的位置
```

---

### 批量设置

对于频繁更新的 uniform，建议批量设置：

```cpp
// 每帧更新的 uniform
void UpdatePerFrameUniforms(UniformManager* uniformMgr) {
    uniformMgr->SetMatrix4("view", camera.GetViewMatrix());
    uniformMgr->SetMatrix4("projection", camera.GetProjectionMatrix());
    uniformMgr->SetVector3("viewPos", camera.GetPosition());
    uniformMgr->SetFloat("time", currentTime);
}

// 每个对象更新的 uniform
void UpdatePerObjectUniforms(UniformManager* uniformMgr, const GameObject& obj) {
    uniformMgr->SetMatrix4("model", obj.GetTransform());
    uniformMgr->SetColor("objectColor", obj.GetColor());
}
```

---

## 错误处理

```cpp
// Uniform 不存在时会记录警告
uniformMgr->SetFloat("nonExistentUniform", 1.0f);
// 日志: Warning: Uniform 'nonExistentUniform' not found in shader

// 安全检查
if (uniformMgr->HasUniform("optionalUniform")) {
    uniformMgr->SetFloat("optionalUniform", value);
}
```

---

## 最佳实践

1. **使用前激活着色器**: 必须先调用 `shader.Use()`
2. **检查 uniform 存在性**: 对可选 uniform 使用 `HasUniform()`
3. **批量设置**: 将相关 uniform 分组设置
4. **调试**: 使用 `PrintUniformInfo()` 查看可用 uniform
5. **命名一致**: C++ 代码中的名称要与着色器中的一致

---

## 相关文档

- [Shader API](Shader.md)
- [Types API](Types.md)
- [示例程序: 02_shader_test](../../examples/02_shader_test.cpp)

---

[上一篇: ShaderCache](ShaderCache.md) | [下一篇: RenderState](RenderState.md)

