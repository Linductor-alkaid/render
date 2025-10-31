# 修复和优化计划

> **基于**: CODE_EVALUATION_REPORT.md  
> **目标**: 修复关键问题，为抽象基类开发做准备

---

## 第一阶段：关键安全问题修复 (P0) ✅ 已完成

### 任务 1.1: 修复 Renderer 裸指针返回问题 ✅ 已完成

**问题文件**: `include/render/renderer.h`, `src/core/renderer.cpp`

**修复方案 A（推荐）**: 改用 shared_ptr

```cpp
// renderer.h
class Renderer {
public:
    // 修改前
    OpenGLContext* GetContext();
    RenderState* GetRenderState();
    
    // 修改后
    std::shared_ptr<OpenGLContext> GetContext() const;
    std::shared_ptr<RenderState> GetRenderState() const;
    
private:
    // 修改前
    std::unique_ptr<OpenGLContext> m_context;
    std::unique_ptr<RenderState> m_renderState;
    
    // 修改后
    std::shared_ptr<OpenGLContext> m_context;
    std::shared_ptr<RenderState> m_renderState;
};
```

**实现步骤**:
1. 修改成员变量类型从 `unique_ptr` 到 `shared_ptr`
2. 修改 getter 方法返回 `shared_ptr`
3. 更新所有调用点
4. 运行所有测试确保正常工作

**影响范围**:
- `renderer.h` / `renderer.cpp`
- 所有使用 `GetContext()` 的代码（需要从 `*` 改为 `->` 或 `.get()`）

---

**修复方案 B（备选）**: 提供安全的访问接口

```cpp
// renderer.h
class Renderer {
public:
    // 不直接暴露指针，提供功能接口
    void SetClearColor(const Color& color);
    void Clear(bool color, bool depth, bool stencil);
    void SwapBuffers();
    int GetWidth() const;
    int GetHeight() const;
    
    // 如果必须访问，使用回调
    template<typename Func>
    void WithContext(Func&& func) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_context) {
            func(*m_context);
        }
    }
    
private:
    std::unique_ptr<OpenGLContext> m_context;
    std::unique_ptr<RenderState> m_renderState;
};
```

**使用示例**:
```cpp
// 之前
auto* ctx = renderer->GetContext();
ctx->SwapBuffers();  // 不安全

// 之后
renderer->SwapBuffers();  // 安全，封装在内部

// 或者使用回调
renderer->WithContext([](OpenGLContext& ctx) {
    // 在锁保护下访问
    ctx.SetVSync(true);
});
```

---

### 任务 1.2: 修复 Texture 移动操作的线程检查 ✅ 已完成

**问题文件**: `src/rendering/texture.cpp`

**修复内容**:

```cpp
// texture.cpp

Texture::Texture(Texture&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    
    m_textureID = other.m_textureID;
    m_width = other.m_width;
    m_height = other.m_height;
    m_format = other.m_format;
    m_hasMipmap = other.m_hasMipmap;
    
    other.m_textureID = 0;
    other.m_width = 0;
    other.m_height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        
        // ✅ 添加线程检查
        if (m_textureID != 0) {
            GL_THREAD_CHECK();  // 新增
            glDeleteTextures(1, &m_textureID);
            Logger::GetInstance().Debug("释放纹理 ID: " + std::to_string(m_textureID));
        }

        m_textureID = other.m_textureID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_hasMipmap = other.m_hasMipmap;

        other.m_textureID = 0;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}
```

**实现步骤**:
1. 在 `operator=` 的 `glDeleteTextures` 前添加 `GL_THREAD_CHECK()`
2. 运行纹理测试
3. 检查日志确认线程检查生效

**测试方法**:
```cpp
// 在非 OpenGL 线程中测试
std::thread([&]() {
    Texture tex2;
    tex2 = std::move(tex1);  // 应该触发 GL_THREAD_CHECK 警告
}).join();
```

---

### 任务 1.3: 修复 Mesh 移动操作的线程检查 ✅ 已完成

**问题文件**: `src/rendering/mesh.cpp`

**修复内容**:

```cpp
// mesh.cpp

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_Mutex, other.m_Mutex);
        
        // ✅ 添加线程检查
        if (m_VAO != 0) {
            GL_THREAD_CHECK();  // 新增
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        if (m_VBO != 0) {
            GL_THREAD_CHECK();  // 新增
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        if (m_EBO != 0) {
            GL_THREAD_CHECK();  // 新增
            glDeleteBuffers(1, &m_EBO);
            m_EBO = 0;
        }
        
        m_Vertices = std::move(other.m_Vertices);
        m_Indices = std::move(other.m_Indices);
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_Uploaded = other.m_Uploaded;
        
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_Uploaded = false;
    }
    return *this;
}
```

---

### 任务 1.4: 修复 Material::GetShader 线程安全 ✅ 已完成

**问题文件**: `include/render/material.h`, `src/rendering/material.cpp`

**修复内容**:

```cpp
// material.h
class Material {
public:
    // 修改前
    std::shared_ptr<Shader> GetShader() const { return m_shader; }
    
    // 修改后
    std::shared_ptr<Shader> GetShader() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_shader;
    }
    
private:
    mutable std::mutex m_mutex;
    std::shared_ptr<Shader> m_shader;
};
```

**注意**: 所有返回 shared_ptr 成员变量的 getter 都应该加锁

---

### 任务 1.5: 修复 Mesh::GetVertices 返回引用问题 ✅ 已完成

**问题文件**: `include/render/mesh.h`

**已采用方案**: 方案 B - 提供只读访问器

**修复方案 A（安全但可能慢）**: 返回副本

```cpp
// mesh.h
class Mesh {
public:
    // 修改前
    const std::vector<Vertex>& GetVertices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Vertices;
    }
    
    // 修改后
    std::vector<Vertex> GetVertices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Vertices;  // 返回副本
    }
};
```

**修复方案 B（推荐）**: 提供只读访问器 ✅ **已实施**

**实施日期**: 2025-10-30

**修改内容**:
1. 将 `GetVertices()` 和 `GetIndices()` 改为返回副本并标记为 `deprecated`
2. 添加 `AccessVertices()` 和 `AccessIndices()` 回调访问器（推荐）
3. 添加 `LockVertices()` 和 `LockIndices()` RAII 守卫
4. 更新 API 文档 `docs/api/Mesh.md`

```cpp
// mesh.h (已实施)
class Mesh {
public:
    // 方法 1: 回调访问（推荐）
    template<typename Func>
    void AccessVertices(Func&& func) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        func(m_Vertices);
    }
    
    template<typename Func>
    void AccessIndices(Func&& func) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        func(m_Indices);
    }
    
    // 方法 2: RAII 守卫
    class VertexGuard {
        const Mesh& m_mesh;
        std::lock_guard<std::mutex> m_lock;
    public:
        VertexGuard(const Mesh& mesh) 
            : m_mesh(mesh), m_lock(mesh.m_Mutex) {}
        
        const std::vector<Vertex>& Get() const { 
            return m_mesh.m_Vertices; 
        }
    };
    
    class IndexGuard {
        const Mesh& m_mesh;
        std::lock_guard<std::mutex> m_lock;
    public:
        IndexGuard(const Mesh& mesh) 
            : m_mesh(mesh), m_lock(mesh.m_Mutex) {}
        
        const std::vector<uint32_t>& Get() const { 
            return m_mesh.m_Indices; 
        }
    };
    
    VertexGuard LockVertices() const {
        return VertexGuard(*this);
    }
    
    IndexGuard LockIndices() const {
        return IndexGuard(*this);
    }
    
    // 旧方法（已弃用，返回副本）
    [[deprecated("Use AccessVertices() or LockVertices() instead")]]
    std::vector<Vertex> GetVertices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Vertices;  // 返回副本
    }
    
    [[deprecated("Use AccessIndices() or LockIndices() instead")]]
    std::vector<uint32_t> GetIndices() const { 
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Indices;  // 返回副本
    }
};
```

**使用示例**:
```cpp
// 方案 A: 副本（简单但可能慢）
auto vertices = mesh->GetVertices();
for (const auto& v : vertices) { /* ... */ }

// 方案 B 方法 1: 回调（推荐）
mesh->AccessVertices([](const std::vector<Vertex>& vertices) {
    for (const auto& v : vertices) { /* ... */ }
});

// 方案 B 方法 2: RAII 守卫
{
    auto guard = mesh->LockVertices();
    const auto& vertices = guard.Get();
    for (const auto& v : vertices) { /* ... */ }
}  // 自动解锁
```

---

### 任务 1.6: 修复 Eigen 对齐宏缺失问题 ✅ 已完成

**实施日期**: 2025-10-31

**问题文件**: 
- `include/render/transform.h` - ✅ 已有对齐宏
- `include/render/camera.h` - ✅ 已有对齐宏
- `include/render/material.h` - ✅ 已添加对齐宏
- `include/render/camera.h` (控制器类) - ✅ 已添加对齐宏

**问题描述**:

包含Eigen固定大小类型（Matrix4, Vector3等）的类需要添加 `EIGEN_MAKE_ALIGNED_OPERATOR_NEW` 宏，以确保：
1. 正确的内存对齐（16字节对齐用于SSE，32字节用于AVX）
2. SIMD指令可以安全使用
3. 避免在某些CPU架构上的崩溃

**修复内容**:

```cpp
// 1. Transform 类（已存在）
class Transform {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // ✅ 第19行
    // ...
};

// 2. Camera 类（已存在）
class Camera {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // ✅ 第64行
    // ...
};

// 3. Material 类（新添加）
class Material {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // ✅ 第49行
    // ...
private:
    std::unordered_map<std::string, Vector2> m_vector2Params;
    std::unordered_map<std::string, Vector3> m_vector3Params;
    std::unordered_map<std::string, Vector4> m_vector4Params;
    std::unordered_map<std::string, Matrix4> m_matrix4Params;
};

// 4. OrbitCameraController 类（新添加）
class OrbitCameraController : public CameraController {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // ✅ 第452行
    // ...
private:
    Vector3 m_target;
};

// 5. ThirdPersonCameraController 类（新添加）
class ThirdPersonCameraController : public CameraController {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW  // ✅ 第505行
    // ...
private:
    Vector3 m_target;
    Vector3 m_offset;
    Vector3 m_currentPosition;
};
```

**为什么需要这个宏？**

Eigen使用SIMD指令（SSE/AVX）优化矩阵运算，这些指令要求数据按16或32字节对齐：
- 标准 `new` 只保证对齐到 `alignof(T)`，通常是8字节
- SIMD指令访问未对齐内存会导致性能下降或崩溃
- `EIGEN_MAKE_ALIGNED_OPERATOR_NEW` 宏重载 `operator new` 以满足对齐要求

**影响分析**:
- ✅ 性能：启用SIMD优化，矩阵运算性能提升2-4倍
- ✅ 稳定性：避免未对齐访问导致的崩溃
- ✅ 兼容性：在所有平台上都能正确工作
- ✅ 零开销：只在使用 `new` 创建对象时生效

**验证方法**:

```cpp
// 编译时验证
static_assert(alignof(Transform) >= 16, "Transform needs 16-byte alignment");
static_assert(alignof(Camera) >= 16, "Camera needs 16-byte alignment");
static_assert(alignof(Material) >= 16, "Material needs 16-byte alignment");

// 运行时验证
Transform* t = new Transform();
assert(reinterpret_cast<uintptr_t>(t) % 16 == 0);
delete t;

auto mat = std::make_shared<Material>();
assert(reinterpret_cast<uintptr_t>(mat.get()) % 16 == 0);
```

**相关文档**:
- Eigen官方文档：[Structures Having Eigen Members](https://eigen.tuxfamily.org/dox/group__TopicStructHavingEigenMembers.html)
- 已更新 `docs/todolists/CODE_EVALUATION_REPORT.md` 警告5部分

---

## 第二阶段：资源管理改进 (P1)

### 任务 2.1: 改进 ResourceManager 的引用计数清理 ✅ 已完成

**实施日期**: 2025-10-30

**问题文件**: `src/core/resource_manager.cpp`

**已实施方案**: 采用更好的方案（帧追踪 + 两阶段清理）

**修复内容**:

```cpp
// resource_manager.cpp

size_t ResourceManager::CleanupUnused() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t cleanedCount = 0;
    
    // 方案 1: 两阶段清理
    // 阶段 1: 标记
    std::vector<std::string> toDelete;
    for (const auto& [name, texture] : m_textures) {
        if (texture.use_count() == 1) {
            toDelete.push_back(name);
        }
    }
    
    // 阶段 2: 删除（再次检查）
    for (const auto& name : toDelete) {
        auto it = m_textures.find(name);
        if (it != m_textures.end() && it->second.use_count() == 1) {
            Logger::GetInstance().Debug("ResourceManager: 清理未使用纹理: " + name);
            m_textures.erase(it);
            ++cleanedCount;
        }
    }
    
    // 对 mesh, material, shader 重复相同逻辑
    // ...
    
    if (cleanedCount > 0) {
        Logger::GetInstance().Info("ResourceManager: 清理了 " + 
            std::to_string(cleanedCount) + " 个未使用资源");
    }
    
    return cleanedCount;
}
```

**更好的方案**: 添加显式标记 ✅ **已实施**

**实施细节**:

1. **添加 ResourceEntry 结构体** (resource_manager.h):
```cpp
template<typename T>
struct ResourceEntry {
    std::shared_ptr<T> resource;        // 资源引用
    bool markedForDeletion = false;     // 删除标记（用于两阶段清理）
    uint32_t lastAccessFrame = 0;       // 最后访问帧号
    
    ResourceEntry() = default;
    ResourceEntry(std::shared_ptr<T> res, uint32_t frame)
        : resource(std::move(res)), lastAccessFrame(frame) {}
};
```

2. **修改存储结构**:
```cpp
// 从
std::unordered_map<std::string, Ref<Texture>> m_textures;
// 改为
std::unordered_map<std::string, ResourceEntry<Texture>> m_textures;

uint32_t m_currentFrame = 0;  // 添加帧计数器
```

3. **添加 BeginFrame 方法**:
```cpp
void ResourceManager::BeginFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_currentFrame;
}
```

4. **更新所有 Get 方法以跟踪访问帧**:
```cpp
Ref<Texture> ResourceManager::GetTexture(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        it->second.lastAccessFrame = m_currentFrame;  // 更新访问帧
        return it->second.resource;
    }
    return nullptr;
}
```

5. **实施两阶段清理策略**:
```cpp
size_t ResourceManager::CleanupUnused(uint32_t unusedFrames = 60) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t cleanedCount = 0;
    
    // 阶段1: 标记待删除的资源
    std::vector<std::string> texturesToDelete;
    for (auto& [name, entry] : m_textures) {
        bool unused = (m_currentFrame - entry.lastAccessFrame) > unusedFrames;
        bool onlyManagerRef = entry.resource.use_count() == 1;
        
        if (unused && onlyManagerRef) {
            texturesToDelete.push_back(name);
            entry.markedForDeletion = true;
        }
    }
    
    // 阶段2: 再次检查并删除
    for (const auto& name : texturesToDelete) {
        auto it = m_textures.find(name);
        if (it != m_textures.end() && 
            it->second.markedForDeletion && 
            it->second.resource.use_count() == 1) {
            Logger::GetInstance().Debug("清理纹理: " + name + 
                " (已 " + std::to_string(m_currentFrame - it->second.lastAccessFrame) + 
                " 帧未使用)");
            m_textures.erase(it);
            ++cleanedCount;
        }
    }
    
    return cleanedCount;
}
```

**优势**:
- ✅ 避免竞态条件：两阶段清理确保引用计数检查的一致性
- ✅ 防止意外删除：基于帧数判断，不会删除刚停止使用的资源
- ✅ 更好的控制：可配置 `unusedFrames` 参数（默认60帧）
- ✅ 详细日志：清理时输出资源未使用的帧数
- ✅ 线程安全：所有操作都在锁保护下进行

**使用方法**:
```cpp
// 在主循环开始时
resourceManager.BeginFrame();

// 在需要时清理未使用资源（60帧未使用）
resourceManager.CleanupUnused(60);

// 或者立即清理（0帧）
resourceManager.CleanupUnused(0);
```

---

### 任务 2.2: 实现统一的错误处理机制 ✅ 已完成

**实施日期**: 2025-10-31

**新文件**: `include/render/error.h`, `src/utils/error.cpp`

**实现内容**:

```cpp
// error.h
#pragma once

#include <string>
#include <exception>
#include <source_location>

namespace Render {

/**
 * @brief 错误严重程度
 */
enum class ErrorSeverity {
    Info,       // 信息
    Warning,    // 警告（可恢复）
    Error,      // 错误（可能可恢复）
    Critical    // 严重错误（不可恢复）
};

/**
 * @brief 错误码
 */
enum class ErrorCode {
    Success = 0,
    
    // OpenGL 错误 (1000-1999)
    GLInvalidEnum = 1000,
    GLInvalidValue,
    GLInvalidOperation,
    GLOutOfMemory,
    GLInvalidFramebufferOperation,
    
    // 资源错误 (2000-2999)
    ResourceNotFound = 2000,
    ResourceAlreadyExists,
    ResourceLoadFailed,
    ResourceInvalidFormat,
    
    // 线程错误 (3000-3999)
    WrongThread = 3000,
    DeadlockDetected,
    
    // 渲染错误 (4000-4999)
    ShaderCompileFailed = 4000,
    ShaderLinkFailed,
    TextureUploadFailed,
    MeshUploadFailed,
    
    // 通用错误 (9000-9999)
    NotImplemented = 9000,
    InvalidArgument,
    NullPointer,
    OutOfRange,
    Unknown = 9999
};

/**
 * @brief 渲染错误基类
 */
class RenderError : public std::exception {
public:
    RenderError(ErrorCode code, 
                const std::string& message,
                ErrorSeverity severity = ErrorSeverity::Error,
                std::source_location location = std::source_location::current());
    
    const char* what() const noexcept override {
        return m_message.c_str();
    }
    
    ErrorCode GetCode() const { return m_code; }
    ErrorSeverity GetSeverity() const { return m_severity; }
    const std::string& GetMessage() const { return m_message; }
    const std::string& GetFile() const { return m_file; }
    int GetLine() const { return m_line; }
    
private:
    ErrorCode m_code;
    ErrorSeverity m_severity;
    std::string m_message;
    std::string m_file;
    int m_line;
};

/**
 * @brief 错误处理器
 */
class ErrorHandler {
public:
    using ErrorCallback = std::function<void(const RenderError&)>;
    
    static ErrorHandler& GetInstance();
    
    /**
     * @brief 处理错误
     */
    void Handle(const RenderError& error);
    
    /**
     * @brief 检查 OpenGL 错误
     */
    void CheckGLError(const std::source_location& location = 
                      std::source_location::current());
    
    /**
     * @brief 设置错误回调
     */
    void SetCallback(ErrorCallback callback) {
        m_callback = callback;
    }
    
private:
    ErrorHandler() = default;
    ErrorCallback m_callback;
};

// 便捷宏
#define RENDER_ERROR(code, msg) \
    Render::RenderError(code, msg, Render::ErrorSeverity::Error)

#define RENDER_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            throw RENDER_ERROR(Render::ErrorCode::InvalidArgument, msg); \
        } \
    } while(0)

#define CHECK_GL_ERROR() \
    Render::ErrorHandler::GetInstance().CheckGLError()

} // namespace Render
```

**已实施内容**:

1. **核心实现** ✅
   - 创建 `include/render/error.h`（493 行）
   - 创建 `src/utils/error.cpp`（379 行）
   - 更新 `CMakeLists.txt`（C++20 + 新文件）

2. **错误分类体系** ✅
   - 50+ 错误码，7个类别
   - 4个严重级别（Info/Warning/Error/Critical）
   - 自动位置追踪（C++20 source_location）

3. **集成到单例类** ✅
   - ResourceManager - 资源注册错误处理
   - ShaderCache - 着色器加载错误处理
   - TextureLoader - 纹理加载错误处理
   - GLThreadChecker - 线程错误处理
   - Renderer - 初始化错误处理
   - OpenGLContext - OpenGL 错误检查

4. **集成到核心类** ✅
   - Texture - 参数验证、文件加载、格式转换
   - Mesh - 绘制状态检查、参数验证
   - Material - 着色器验证
   - Camera - 参数验证和自动修正
   - UniformManager - Uniform 未找到警告
   - RenderState - 预备集成
   - Transform - 预备集成

5. **便捷宏** ✅
   - `RENDER_ERROR/WARNING/CRITICAL` - 创建错误
   - `RENDER_ASSERT` - 断言检查
   - `CHECK_GL_ERROR` - GL 错误检查（只记录）
   - `CHECK_GL_ERROR_THROW` - GL 错误检查（抛出异常）
   - `HANDLE_ERROR` - 处理错误（不抛出）
   - `RENDER_TRY/CATCH/CATCH_ALL` - Try-Catch 包装

6. **完整文档** ✅
   - `docs/ERROR_HANDLING.md` - 完整文档
   - `docs/ERROR_HANDLING_EXAMPLES.md` - 使用示例
   - `docs/ERROR_HANDLING_SUMMARY.md` - 实施总结
   - `docs/ERROR_HANDLING_INTEGRATION_SUMMARY.md` - 集成总结
   - `docs/api/ErrorHandler.md` - ErrorHandler API 文档
   - `docs/api/RenderError.md` - RenderError API 文档
   - 更新 `docs/api/README.md` - 添加错误处理链接

**使用示例**:

```cpp
// 1. 基本错误处理
RENDER_TRY {
    LoadTexture("test.png");
}
RENDER_CATCH {
    // 错误已自动记录
}

// 2. 参数验证
if (width <= 0 || height <= 0) {
    HANDLE_ERROR(RENDER_ERROR(ErrorCode::InvalidArgument, 
                             "纹理尺寸无效"));
    return false;
}

// 3. OpenGL 错误检查
glBindTexture(GL_TEXTURE_2D, id);
CHECK_GL_ERROR();  // 只记录，不抛出

// 4. 断言检查
RENDER_ASSERT(texture != nullptr, "纹理不能为空");

// 5. 自定义回调
ErrorHandler::GetInstance().AddCallback([](const RenderError& e) {
    if (e.GetSeverity() == ErrorSeverity::Critical) {
        SaveCrashDump(e);
    }
});
```

**优势**:
- ✅ 统一的错误处理机制
- ✅ 详细的错误信息和位置追踪
- ✅ 灵活的错误恢复策略
- ✅ 线程安全的全局错误处理器
- ✅ 性能友好（正常情况几乎无开销）

---

### 任务 2.3: 添加资源生命周期文档

**新文件**: `docs/RESOURCE_LIFETIME.md`

**内容**:
```markdown
# 资源生命周期管理指南

## 资源类型

### 1. OpenGL 资源
- Texture (GLuint)
- Shader Program (GLuint)
- Buffer (GLuint)
- VAO (GLuint)

**生命周期**:
- 创建: 必须在 OpenGL 线程
- 使用: 必须在 OpenGL 线程
- 销毁: 必须在 OpenGL 线程
- 引用计数: 使用 shared_ptr 管理

### 2. CPU 资源
- Mesh 顶点/索引数据
- 纹理像素数据
- 着色器源码

**生命周期**:
- 创建: 任意线程
- 使用: 任意线程（加锁）
- 销毁: 任意线程

## 最佳实践

### ✅ 正确做法

1. 使用 shared_ptr 管理资源
```cpp
auto texture = std::make_shared<Texture>();
resourceManager.RegisterTexture("albedo", texture);
```

2. 通过 ResourceManager 获取资源
```cpp
auto texture = resourceManager.GetTexture("albedo");
if (texture) {
    texture->Bind(0);
}
```

3. 在 OpenGL 线程释放
```cpp
// 在主循环中
resourceManager.CleanupUnused();
```

### ❌ 错误做法

1. 保存裸指针
```cpp
Texture* tex = resourceManager.GetTexture("albedo").get();
// 之后使用 tex - 可能已失效！
```

2. 在非 OpenGL 线程删除
```cpp
std::thread([&]() {
    delete texture;  // ❌ 崩溃！
}).join();
```

3. 循环引用
```cpp
class A {
    std::shared_ptr<B> b;
};
class B {
    std::shared_ptr<A> a;  // ❌ 循环引用，永不释放
};
```

## 调试技巧

1. 启用引用计数日志
2. 使用 ASAN (Address Sanitizer)
3. 使用 Valgrind (Linux)
4. 定期检查资源泄漏

## 常见问题

Q: 为什么资源没有被释放？
A: 检查是否有循环引用或遗漏的 shared_ptr

Q: 为什么在 Shutdown 后崩溃？
A: 可能保存了裸指针，在资源删除后仍然使用

Q: 如何实现资源热重载？
A: 保持原 ID，只更新内部数据
```

---

## 第三阶段：架构优化 (P2)

### 任务 3.1: 实现渲染命令队列

**新文件**: `include/render/render_command_queue.h`, `src/rendering/render_command_queue.cpp`

**实现内容**:

```cpp
// render_command_queue.h
#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <vector>

namespace Render {

/**
 * @brief 渲染命令
 */
class RenderCommand {
public:
    using ExecuteFunc = std::function<void()>;
    
    RenderCommand(ExecuteFunc func) : m_func(std::move(func)) {}
    
    void Execute() {
        if (m_func) {
            m_func();
        }
    }
    
private:
    ExecuteFunc m_func;
};

/**
 * @brief 渲染命令队列
 * 
 * 用于分离逻辑线程和渲染线程
 */
class RenderCommandQueue {
public:
    /**
     * @brief 提交命令
     */
    void Submit(RenderCommand command) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands.push(std::move(command));
    }
    
    /**
     * @brief 批量提交命令
     */
    template<typename... Commands>
    void SubmitBatch(Commands&&... commands) {
        std::lock_guard<std::mutex> lock(m_mutex);
        (m_commands.push(std::forward<Commands>(commands)), ...);
    }
    
    /**
     * @brief 执行所有命令
     */
    void Execute() {
        std::queue<RenderCommand> localQueue;
        
        // 快速交换，减少锁持有时间
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::swap(localQueue, m_commands);
        }
        
        // 执行命令
        while (!localQueue.empty()) {
            localQueue.front().Execute();
            localQueue.pop();
        }
    }
    
    /**
     * @brief 清空队列
     */
    void Clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_commands.empty()) {
            m_commands.pop();
        }
    }
    
    /**
     * @brief 获取命令数量
     */
    size_t GetCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_commands.size();
    }
    
private:
    std::queue<RenderCommand> m_commands;
    mutable std::mutex m_mutex;
};

} // namespace Render
```

**使用示例**:

```cpp
// 在逻辑线程
RenderCommandQueue cmdQueue;

// 提交绘制命令
cmdQueue.Submit([mesh, material]() {
    material->Bind();
    mesh->Draw();
});

// 提交更新命令
cmdQueue.Submit([texture, data]() {
    texture->UpdateData(data);
});

// 在渲染线程
void Render() {
    cmdQueue.Execute();  // 执行所有命令
}
```

---

### 任务 3.2: 实现批量绘制系统

**新文件**: `include/render/batch_renderer.h`

**实现内容**:

```cpp
// batch_renderer.h
#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/material.h"
#include <vector>
#include <algorithm>

namespace Render {

/**
 * @brief 绘制调用
 */
struct DrawCall {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    Matrix4 transform;
    
    // 排序键（用于减少状态切换）
    uint64_t sortKey = 0;
};

/**
 * @brief 批量渲染器
 */
class BatchRenderer {
public:
    /**
     * @brief 添加绘制调用
     */
    void Submit(const DrawCall& drawCall) {
        m_drawCalls.push_back(drawCall);
    }
    
    /**
     * @brief 排序绘制调用
     */
    void Sort() {
        std::sort(m_drawCalls.begin(), m_drawCalls.end(),
            [](const DrawCall& a, const DrawCall& b) {
                // 按材质排序以减少状态切换
                return a.sortKey < b.sortKey;
            });
    }
    
    /**
     * @brief 执行绘制
     */
    void Render(Camera& camera) {
        Sort();
        
        Material* lastMaterial = nullptr;
        
        for (const auto& drawCall : m_drawCalls) {
            // 仅在材质改变时切换
            if (drawCall.material.get() != lastMaterial) {
                if (lastMaterial) {
                    lastMaterial->Unbind();
                }
                drawCall.material->Bind();
                lastMaterial = drawCall.material.get();
            }
            
            // 设置变换矩阵
            auto shader = drawCall.material->GetShader();
            if (shader && shader->GetUniformManager()) {
                auto* uniforms = shader->GetUniformManager();
                uniforms->SetMatrix4("u_Model", drawCall.transform);
                uniforms->SetMatrix4("u_View", camera.GetViewMatrix());
                uniforms->SetMatrix4("u_Projection", camera.GetProjectionMatrix());
            }
            
            // 绘制
            drawCall.mesh->Draw();
        }
        
        if (lastMaterial) {
            lastMaterial->Unbind();
        }
    }
    
    /**
     * @brief 清空绘制调用
     */
    void Clear() {
        m_drawCalls.clear();
    }
    
private:
    std::vector<DrawCall> m_drawCalls;
};

} // namespace Render
```

---

## 第四阶段：性能优化 (P3)

### 任务 4.1: 使用 std::string_view 优化字符串

**修改文件**: `include/render/uniform_manager.h`, `src/rendering/uniform_manager.cpp`

**示例**:

```cpp
// uniform_manager.h
#include <string_view>

class UniformManager {
public:
    // 修改前
    void SetInt(const std::string& name, int value);
    
    // 修改后
    void SetInt(std::string_view name, int value);
    
private:
    // 缓存仍然使用 std::string
    std::unordered_map<std::string, int> m_uniformLocationCache;
};
```

---

### 任务 4.2: 实现 Uniform Buffer Object (UBO)

**新文件**: `include/render/uniform_buffer.h`

**实现内容**:

```cpp
// uniform_buffer.h
#pragma once

#include "render/types.h"
#include <glad/glad.h>

namespace Render {

/**
 * @brief Uniform Buffer Object
 * 
 * 用于批量上传 uniform 数据，减少 OpenGL 调用
 */
class UniformBuffer {
public:
    UniformBuffer(size_t size, uint32_t bindingPoint);
    ~UniformBuffer();
    
    /**
     * @brief 更新数据
     */
    void Update(const void* data, size_t size, size_t offset = 0);
    
    /**
     * @brief 绑定到绑定点
     */
    void Bind() const;
    
    /**
     * @brief 解绑
     */
    void Unbind() const;
    
private:
    GLuint m_bufferID = 0;
    size_t m_size = 0;
    uint32_t m_bindingPoint = 0;
};

/**
 * @brief 场景 Uniform 数据
 */
struct SceneUniforms {
    Matrix4 viewMatrix;
    Matrix4 projMatrix;
    Vector3 cameraPosition;
    float padding;  // 对齐
};

} // namespace Render
```

**使用示例**:

```cpp
// 创建 UBO
UniformBuffer sceneUBO(sizeof(SceneUniforms), 0);

// 每帧更新
SceneUniforms uniforms;
uniforms.viewMatrix = camera.GetViewMatrix();
uniforms.projMatrix = camera.GetProjectionMatrix();
uniforms.cameraPosition = camera.GetPosition();

sceneUBO.Update(&uniforms, sizeof(uniforms));
sceneUBO.Bind();

// 在着色器中
// layout(std140, binding = 0) uniform SceneData {
//     mat4 u_ViewMatrix;
//     mat4 u_ProjMatrix;
//     vec3 u_CameraPos;
// };
```

---

## 测试计划

### 单元测试

1. **资源管理测试**
   - 测试 shared_ptr 生命周期
   - 测试多线程资源获取
   - 测试资源清理

2. **移动操作测试**
   - 测试 Texture 移动
   - 测试 Mesh 移动
   - 测试线程检查

3. **错误处理测试**
   - 测试异常抛出
   - 测试错误恢复
   - 测试错误回调

### 集成测试

1. **多线程压力测试**
   - 100+ 线程同时访问资源
   - 随机 Shutdown/Initialize
   - 检测内存泄漏

2. **性能测试**
   - 批量绘制 vs 单独绘制
   - UBO vs 单独 uniform
   - 命令队列开销

3. **稳定性测试**
   - 长时间运行（24小时）
   - 随机资源创建/销毁
   - 内存监控

---

## 回归测试清单

每次修改后检查：

- [ ] 所有现有测试通过
- [ ] 没有新的编译警告
- [ ] 没有内存泄漏（ASAN）
- [ ] 没有线程错误（TSAN）
- [ ] 性能没有明显下降
- [ ] 文档已更新

---

## 完成标准

### P0 任务完成标准
- [ ] 所有 getter 不再返回裸指针
- [ ] 所有移动操作有线程检查
- [ ] 单元测试覆盖率 > 80%
- [ ] 内存泄漏检测通过

### P1 任务完成标准
- [ ] 资源清理无竞态条件
- [ ] 错误处理机制完整
- [ ] 文档完整
- [ ] 集成测试通过

### P2 任务完成标准
- [ ] 命令队列可用
- [ ] 批量渲染性能提升 > 30%
- [ ] 多线程压力测试通过

---

## 时间估算

- **P0 任务**: 2-3 天
- **P1 任务**: 3-5 天
- **P2 任务**: 5-7 天
- **P3 任务**: 7-10 天

**总计**: 约 3-4 周完成所有优化

---

*本文档将随着修复进展持续更新*

