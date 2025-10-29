# 开发指南

## 目录
[返回文档首页](README.md)

## 快速开始

### 环境配置

#### 依赖项安装

**Windows (使用 vcpkg)**
```bash
vcpkg install opengl sdl3 --triplet x64-windows
```

**Linux (Ubuntu/Debian)**
```bash
sudo apt-get install libgl-dev libsdl3-dev
```

**macOS**
```bash
brew install opengl sdl3
```

#### 编译配置

创建 `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.15)
project(RenderEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(OpenGL REQUIRED)
find_package(SDL3 REQUIRED)

# 包含目录
include_directories(
    ${OPENGL_INCLUDE_DIRS}
    ${SDL3_INCLUDE_DIRS}
)

# 添加渲染引擎源文件
add_subdirectory(src)

# 链接库
target_link_libraries(RenderEngine
    ${OPENGL_LIBRARIES}
    ${SDL3_LIBRARIES}
)
```

### 基础使用示例

```cpp
#include "render/renderer.h"
#include <iostream>

int main() {
    // 创建渲染器
    Renderer* renderer = Renderer::Create();
    
    if (!renderer->Initialize()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return -1;
    }
    
    // 设置窗口
    renderer->SetWindowTitle("My Application");
    renderer->SetWindowSize(1920, 1080);
    renderer->SetVSync(true);
    
    // 创建场景
    SetupScene(renderer);
    
    // 主循环
    bool running = true;
    while (running) {
        // 处理输入事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        
        float deltaTime = renderer->GetDeltaTime();
        
        // 更新逻辑
        Update(deltaTime);
        
        // 渲染
        renderer->BeginFrame();
        renderer->Clear();
        renderer->Render();
        renderer->EndFrame();
        renderer->Present();
    }
    
    // 清理
    renderer->Shutdown();
    Renderer::Destroy(renderer);
    
    return 0;
}

void SetupScene(Renderer* renderer) {
    // 创建渲染层级
    RenderLayer* skyLayer = renderer->CreateLayer(100);
    RenderLayer* worldLayer = renderer->CreateLayer(300);
    RenderLayer* uiLayer = renderer->CreateLayer(800);
    
    // 创建天空盒
    CreateSkybox(renderer, skyLayer);
    
    // 创建世界几何
    CreateWorld(renderer, worldLayer);
    
    // 创建UI
    CreateUI(renderer, uiLayer);
}
```

## 项目结构

推荐的项目目录结构：

```
myproject/
├── CMakeLists.txt
├── src/
│   ├── core/
│   │   ├── renderer.cpp
│   │   ├── render_layer.cpp
│   │   └── resource_manager.cpp
│   ├── renderable/
│   │   ├── mesh.cpp
│   │   ├── sprite.cpp
│   │   └── text.cpp
│   ├── rendering/
│   │   ├── camera.cpp
│   │   ├── lighting.cpp
│   │   └── post_processing.cpp
│   ├── utils/
│   │   ├── math.cpp
│   │   └── file_utils.cpp
│   └── integration/
│       ├── ecs_integration.cpp
│       └── component_systems.cpp
├── shaders/
│   ├── base.vert
│   ├── base.frag
│   ├── deferred.vert
│   └── deferred.frag
├── resources/
│   ├── models/
│   ├── textures/
│   └── fonts/
├── docs/
│   ├── README.md
│   ├── ARCHITECTURE.md
│   ├── API_REFERENCE.md
│   ├── RENDERING_LAYERS.md
│   ├── ECS_INTEGRATION.md
│   └── DEVELOPMENT_GUIDE.md
└── tests/
    └── test_renderer.cpp
```

## 核心功能实现

### 着色器管理系统

#### 着色器加载

```cpp
class Shader {
public:
    struct ShaderSource {
        std::string vertex;
        std::string fragment;
        std::string geometry;
    };
    
    static Shader* LoadFromFiles(const std::string& vertexPath,
                                const std::string& fragmentPath);
    
    static Shader* LoadFromSource(const ShaderSource& source);
    
    void Use();
    void SetUniform(const std::string& name, int value);
    void SetUniform(const std::string& name, float value);
    void SetUniform(const std::string& name, const Vector3& value);
    void SetUniform(const std::string& name, const Matrix4& value);
    
private:
    uint32_t m_programID;
    std::unordered_map<std::string, int> m_uniformLocationCache;
    
    uint32_t CompileShader(const std::string& source, uint32_t type);
    uint32_t LinkProgram(uint32_t vertexShader, uint32_t fragmentShader);
};
```

#### 着色器示例

**base.vert**
```glsl
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPosition, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

**base.frag**
```glsl
#version 450 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D diffuseTexture;
uniform vec3 viewPos;

// 光照结构
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
};

uniform Light lights[4];
uniform int lightCount;

void main() {
    vec3 albedo = texture(diffuseTexture, TexCoord).rgb;
    vec3 normal = normalize(Normal);
    
    // 环境光
    vec3 ambient = albedo * 0.1;
    
    // 漫反射和镜面反射
    vec3 lighting = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec3 lightDir = normalize(lights[i].position - FragPos);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * lights[i].color * lights[i].intensity;
        
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = spec * lights[i].color * lights[i].intensity * 0.5;
        
        lighting += (diffuse + specular) * albedo;
    }
    
    FragColor = vec4(ambient + lighting, 1.0);
}
```

### 资源管理系统

#### 资源加载器基类

```cpp
template<typename T>
class ResourceLoader {
public:
    virtual ~ResourceLoader() = default;
    
    virtual T* Load(const std::string& path) = 0;
    virtual void Unload(T* resource) = 0;
    virtual bool IsLoaded(const std::string& path) const;
    
protected:
    std::unordered_map<std::string, std::unique_ptr<T>> m_cache;
};

class TextureLoader : public ResourceLoader<Texture> {
public:
    Texture* Load(const std::string& path) override {
        if (m_cache.find(path) != m_cache.end()) {
            return m_cache[path].get();
        }
        
        // 使用SDL_image加载
        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return nullptr;
        }
        
        uint32_t textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        GLenum format = (surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format,
                     surface->w, surface->h, 0, format,
                     GL_UNSIGNED_BYTE, surface->pixels);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        auto texture = std::make_unique<Texture>();
        texture->id = textureID;
        texture->width = surface->w;
        texture->height = surface->h;
        texture->format = format;
        
        SDL_FreeSurface(surface);
        
        m_cache[path] = std::move(texture);
        return m_cache[path].get();
    }
    
    void Unload(Texture* texture) override {
        if (!texture) return;
        
        glDeleteTextures(1, &texture->id);
        
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (it->second.get() == texture) {
                m_cache.erase(it);
                break;
            }
        }
    }
};
```

### 批处理系统

#### 渲染批处理

```cpp
class RenderBatch {
public:
    struct Vertex {
        Vector3 position;
        Vector2 texCoord;
        Color color;
        Vector3 normal;
    };
    
    void AddMesh(MeshRenderable* mesh) {
        const Mesh* data = mesh->GetMesh();
        const Material* material = mesh->GetMaterial();
        
        if (GetBatchKey(material) != GetBatchKey(m_currentMaterial)) {
            Flush();
            m_currentMaterial = material;
        }
        
        Matrix4 transform = mesh->GetTransform();
        
        for (size_t i = 0; i < data->GetVertexCount(); ++i) {
            Vertex vertex;
            vertex.position = transform * data->GetVertex(i).position;
            vertex.texCoord = data->GetVertex(i).texCoord;
            vertex.color = material->GetColor();
            vertex.normal = transform.GetRotationMatrix() * data->GetVertex(i).normal;
            
            m_vertices.push_back(vertex);
            m_indices.push_back(m_currentIndex++);
        }
    }
    
    void Flush() {
        if (m_vertices.empty()) return;
        
        // 更新VBO
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                    m_vertices.size() * sizeof(Vertex),
                    m_vertices.data(), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    m_indices.size() * sizeof(uint32_t),
                    m_indices.data(), GL_STATIC_DRAW);
        
        // 渲染
        glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
        
        // 清空
        m_vertices.clear();
        m_indices.clear();
        m_currentIndex = 0;
    }
    
private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    const Material* m_currentMaterial = nullptr;
    uint32_t m_vbo, m_vao, m_ibo;
    uint32_t m_currentIndex = 0;
    
    uint64_t GetBatchKey(const Material* material) {
        return reinterpret_cast<uint64_t>(material);
    }
};
```

## 性能优化技巧

### 1. 减少绘制调用

```cpp
class BatchingManager {
public:
    void AddToBatch(MeshRenderable* mesh) {
        BatchKey key = GetBatchKey(mesh);
        
        if (m_batches.find(key) == m_batches.end()) {
            m_batches[key] = std::make_unique<RenderBatch>(key);
        }
        
        m_batches[key]->AddMesh(mesh);
    }
    
    void FlushAll() {
        for (auto& [key, batch] : m_batches) {
            batch->Flush();
        }
        m_batches.clear();
    }
    
private:
    struct BatchKey {
        uint32_t materialID;
        uint32_t shaderID;
        BlendMode blendMode;
        
        bool operator<(const BatchKey& other) const {
            if (materialID != other.materialID) 
                return materialID < other.materialID;
            if (shaderID != other.shaderID) 
                return shaderID < other.shaderID;
            return blendMode < other.blendMode;
        }
    };
    
    BatchKey GetBatchKey(MeshRenderable* mesh) {
        return {
            .materialID = mesh->GetMaterial()->GetID(),
            .shaderID = mesh->GetMaterial()->GetShader()->GetID(),
            .blendMode = mesh->GetMaterial()->GetBlendMode()
        };
    }
    
    std::map<BatchKey, std::unique_ptr<RenderBatch>> m_batches;
};
```

### 2. 视锥剔除

```cpp
class Frustum {
public:
    void UpdateFromCamera(const Camera* camera) {
        Matrix4 viewProj = camera->GetProjectionMatrix() * camera->GetViewMatrix();
        
        // 提取6个平面
        // 左、右、上、下、近、远
        ExtractPlanes(viewProj);
    }
    
    bool IsBoxInside(const AABB& aabb) const {
        for (const Plane& plane : m_planes) {
            if (!plane.IsBoxOnPositiveSide(aabb)) {
                return false;
            }
        }
        return true;
    }
    
private:
    struct Plane {
        Vector3 normal;
        float distance;
        
        bool IsBoxOnPositiveSide(const AABB& box) const {
            Vector3 positiveVertex = box.GetPositiveVertex(normal);
            return glm::dot(normal, positiveVertex) + distance > 0;
        }
    };
    
    std::array<Plane, 6> m_planes;
    
    void ExtractPlanes(const Matrix4& matrix);
};
```

### 3. 遮挡剔除

```cpp
class OcclusionCuller {
public:
    void RenderDepthPrepass(const std::vector<MeshRenderable*>& meshes) {
        // 使用简单着色器渲染深度
        m_depthShader->Use();
        
        for (auto* mesh : meshes) {
            mesh->RenderDepthOnly(m_depthShader);
        }
    }
    
    bool IsVisible(MeshRenderable* mesh) {
        AABB bounds = mesh->GetBounds();
        
        // 简化的可见性测试
        return QueryOcclusionQuery(bounds);
    }
    
private:
    Shader* m_depthShader;
};
```

## 调试工具

### 渲染调试器

```cpp
class RenderDebugger {
public:
    static void DrawWireframe(bool enable) {
        m_wireframeMode = enable;
        glPolygonMode(GL_FRONT_AND_BACK, enable ? GL_LINE : GL_FILL);
    }
    
    static void DrawAABB(const AABB& aabb, const Color& color) {
        if (m_debugShader) {
            m_debugShader->Use();
            m_debugShader->SetUniform("color", color);
            
            // 渲染线框盒子
            RenderWireframeBox(aabb);
        }
    }
    
    static void DrawLine(const Vector3& from, const Vector3& to, const Color& color) {
        std::vector<Vector3> points = {from, to};
        RenderLines(points, color);
    }
    
    static void ShowGizmos(bool show) {
        m_showGizmos = show;
    }
    
    static void OnGUI() {
        ImGui::Begin("Render Debug");
        
        ImGui::Checkbox("Wireframe", &m_wireframeMode);
        ImGui::Checkbox("Show Gizmos", &m_showGizmos);
        ImGui::Checkbox("Show AABBs", &m_showAABBs);
        ImGui::Checkbox("Show Lights", &m_showLights);
        
        ImGui::End();
    }
    
private:
    static bool m_wireframeMode;
    static bool m_showGizmos;
    static bool m_showAABBs;
    static bool m_showLights;
    static Shader* m_debugShader;
};
```

## 测试

### 单元测试示例

```cpp
#include "gtest/gtest.h"
#include "render/renderer.h"

class RendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        renderer = Renderer::Create();
        renderer->Initialize();
    }
    
    void TearDown() override {
        renderer->Shutdown();
        Renderer::Destroy(renderer);
    }
    
    Renderer* renderer;
};

TEST_F(RendererTest, LayerCreation) {
    RenderLayer* layer = renderer->CreateLayer(100);
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(layer->GetPriority(), 100);
}

TEST_F(RendererTest, ResourceLoading) {
    Texture* texture = renderer->LoadTexture("test.png");
    EXPECT_NE(texture, nullptr);
    EXPECT_NE(texture->GetID(), 0);
}

TEST_F(RendererTest, RenderingBasic) {
    Mesh* mesh = renderer->CreateMesh();
    // ... 设置网格数据
    
    renderer->BeginFrame();
    renderer->Render();
    renderer->EndFrame();
    
    RenderStats stats = renderer->GetStats();
    EXPECT_GT(stats.drawCalls, 0);
}
```

---

[返回文档首页](README.md) | [上一篇: ECS 集成](ECS_INTEGRATION.md)

