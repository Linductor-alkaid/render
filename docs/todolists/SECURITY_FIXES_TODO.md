# 渲染引擎安全性修复 TODO 清单

> **生成日期**: 2025-11-01  
> **检查范围**: Mesh、Texture、Material、Shader、ResourceManager 核心模块  
> **总体评分**: 8.5/10 (良好)

---

## 📊 修复优先级概览

| 优先级 | 问题数量 | 预计工时 | 影响范围 |
|--------|---------|---------|---------|
| 🔴 高优先级 | 2 | 2-3小时 | 稳定性和安全性 |
| 🟡 中优先级 | 4 | 3-4小时 | 鲁棒性和可维护性 |
| 🟢 低优先级 | 3 | 4-6小时 | 代码质量和性能 |

---

## 🔴 高优先级修复项 (必须立即修复)

### ❌ TODO-1: 修复 UniformManager 栈数组潜在溢出

**严重程度**: ⚠️ 高危  
**影响**: 可能导致栈溢出和程序崩溃  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/uniform_manager.cpp`  
**位置**: 第 156 行、第 177 行

```cpp
// 当前代码 (有风险)
GLchar name[256];  // 固定大小栈数组
glGetActiveUniform(m_programID, i, sizeof(name), &length, &size, &type, name);
```

**风险分析**:
- Uniform 名称理论上可以超过 255 字符
- OpenGL 不保证名称长度限制
- 栈数组溢出可能导致未定义行为

#### 修复方案

**方案 1: 动态查询最大长度 (推荐)**
```cpp
// 在 GetAllUniformNames() 开头添加
GLint maxLength = 0;
glGetProgramiv(m_programID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

std::vector<GLchar> name(maxLength);
for (GLint i = 0; i < numUniforms; ++i) {
    GLsizei length;
    GLint size;
    GLenum type;
    
    glGetActiveUniform(m_programID, i, maxLength, &length, &size, &type, name.data());
    uniformNames.push_back(std::string(name.data(), length));
}
```

**方案 2: 使用 std::string (备选)**
```cpp
std::string name;
name.resize(512);  // 设置足够大的初始大小
GLsizei actualLength;

glGetActiveUniform(m_programID, i, name.size(), &actualLength, &size, &type, name.data());
name.resize(actualLength);  // 调整到实际长度
uniformNames.push_back(name);
```

#### 修复位置
需要修改以下函数：
1. ✅ `UniformManager::GetAllUniformNames()` (第 146-166 行)
2. ✅ `UniformManager::PrintUniformInfo()` (第 168-204 行)

#### 测试验证
- [ ] 编译通过
- [ ] 加载具有长 uniform 名称的着色器
- [ ] 运行现有的 shader 单元测试
- [ ] Valgrind/ASan 内存检查

---

### ❌ TODO-2: 添加指针数组参数的空指针检查

**严重程度**: ⚠️ 高危  
**影响**: 可能导致程序崩溃  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/uniform_manager.cpp`  
**位置**: 第 89-118 行

```cpp
// 当前代码 (缺少检查)
void UniformManager::SetIntArray(const std::string& name, const int* values, uint32_t count) {
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1iv(location, count, values);  // values 可能为 nullptr
    }
}
```

#### 修复方案

```cpp
void UniformManager::SetIntArray(const std::string& name, const int* values, uint32_t count) {
    // 添加参数验证
    if (!values) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetIntArray: values pointer is null"));
        return;
    }
    
    if (count == 0) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "UniformManager::SetIntArray: count is zero"));
        return;
    }
    
    int location = GetOrFindUniformLocation(name);
    if (location != -1) {
        GL_THREAD_CHECK();
        glUniform1iv(location, count, values);
    }
}
```

#### 修复位置
需要修改以下函数：
1. ✅ `SetIntArray()` (第 89-95 行)
2. ✅ `SetFloatArray()` (第 97-103 行)
3. ✅ `SetVector3Array()` (第 105-111 行)
4. ✅ `SetMatrix4Array()` (第 113-119 行)

#### 测试验证
- [ ] 编译通过
- [ ] 测试传入 nullptr 不会崩溃
- [ ] 测试传入 count=0 不会崩溃
- [ ] 验证日志正确输出警告

---

## 🟡 中优先级修复项 (建议尽快修复)

### ⚠️ TODO-3: 增强 Mesh::UpdateVertices 边界检查

**严重程度**: 🟡 中等  
**影响**: 可能导致越界访问  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/mesh.cpp`  
**位置**: 第 108-132 行

```cpp
// 当前代码 (检查不完整)
if (offset + vertices.size() > m_Vertices.size()) {
    Logger::GetInstance().Error("Mesh::UpdateVertices - Offset + size exceeds vertex count");
    return;
}
```

**缺少的检查**:
1. 没有检查 `offset` 本身是否越界
2. 没有检查 `vertices` 是否为空

#### 修复方案

```cpp
void Mesh::UpdateVertices(const std::vector<Vertex>& vertices, size_t offset) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // 检查网格是否已上传
    if (!m_Uploaded) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::UpdateVertices: Mesh not uploaded yet"));
        return;
    }
    
    // 检查输入数据是否为空
    if (vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument, 
                                   "Mesh::UpdateVertices: Empty vertex data provided"));
        return;
    }
    
    // 检查 offset 是否越界
    if (offset >= m_Vertices.size()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::OutOfRange, 
                                 "Mesh::UpdateVertices: Offset " + std::to_string(offset) + 
                                 " exceeds vertex count " + std::to_string(m_Vertices.size())));
        return;
    }
    
    // 检查 offset + size 是否越界
    if (offset + vertices.size() > m_Vertices.size()) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::OutOfRange, 
                                 "Mesh::UpdateVertices: Offset " + std::to_string(offset) + 
                                 " + size " + std::to_string(vertices.size()) + 
                                 " exceeds vertex count " + std::to_string(m_Vertices.size())));
        return;
    }
    
    // 更新 CPU 端数据
    std::copy(vertices.begin(), vertices.end(), m_Vertices.begin() + offset);
    
    // 更新 GPU 端数据
    GL_THREAD_CHECK();
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 
                    offset * sizeof(Vertex), 
                    vertices.size() * sizeof(Vertex), 
                    vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
```

#### 测试验证
- [ ] 编译通过
- [ ] 测试 offset 越界情况
- [ ] 测试空 vertices 情况
- [ ] 测试正常更新情况

---

### ⚠️ TODO-4: 改进 RecalculateNormals 的错误处理

**严重程度**: 🟡 中等  
**影响**: 静默跳过错误可能导致渲染异常  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/mesh.cpp`  
**位置**: 第 319-326 行

```cpp
// 当前代码 (静默跳过)
if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) {
    continue;  // 没有日志记录
}
```

#### 修复方案

```cpp
for (size_t i = 0; i < m_Indices.size(); i += 3) {
    uint32_t i0 = m_Indices[i];
    uint32_t i1 = m_Indices[i + 1];
    uint32_t i2 = m_Indices[i + 2];
    
    // 添加越界检查和警告
    if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Mesh::RecalculateNormals: Invalid triangle indices at offset " + 
                                   std::to_string(i) + " [" + std::to_string(i0) + ", " + 
                                   std::to_string(i1) + ", " + std::to_string(i2) + "], " +
                                   "vertex count: " + std::to_string(m_Vertices.size())));
        continue;
    }
    
    // ... 正常处理
}
```

#### 可选增强
添加统计信息：
```cpp
size_t invalidTriangles = 0;
for (size_t i = 0; i < m_Indices.size(); i += 3) {
    // ... 检查代码
    if (invalid) {
        invalidTriangles++;
        continue;
    }
}

if (invalidTriangles > 0) {
    Logger::GetInstance().Warning("Mesh::RecalculateNormals: Skipped " + 
                                  std::to_string(invalidTriangles) + " invalid triangles");
}
```

#### 测试验证
- [ ] 编译通过
- [ ] 测试有效索引正常工作
- [ ] 测试无效索引触发警告
- [ ] 检查日志输出

---

### ⚠️ TODO-5: 完善 Mesh::CalculateBounds 的防御性编程

**严重程度**: 🟡 低-中等  
**影响**: 极端情况下可能越界  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/mesh.cpp`  
**位置**: 第 286-303 行

虽然已有空检查，但可以更加防御性：

#### 修复方案

```cpp
AABB Mesh::CalculateBounds() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // 返回空的包围盒
    if (m_Vertices.empty()) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidState, 
                                   "Mesh::CalculateBounds: Mesh has no vertices"));
        return AABB();
    }
    
    // 在同一个临界区内完成所有操作
    AABB bounds;
    bounds.min = m_Vertices[0].position;
    bounds.max = m_Vertices[0].position;
    
    for (size_t i = 1; i < m_Vertices.size(); ++i) {  // 从索引1开始
        const auto& pos = m_Vertices[i].position;
        bounds.min = bounds.min.cwiseMin(pos);
        bounds.max = bounds.max.cwiseMax(pos);
    }
    
    return bounds;
}
```

#### 测试验证
- [ ] 测试空网格返回默认 AABB
- [ ] 测试单顶点网格
- [ ] 测试多顶点网格

---

### ⚠️ TODO-6: 为 ResourceManager::ForEach 方法添加文档警告

**严重程度**: 🟡 中等  
**影响**: 可能导致死锁  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `include/render/resource_manager.h`  
**位置**: 第 273-291 行

`ForEach*` 方法在持锁状态下调用用户回调，可能导致：
1. 回调中调用 ResourceManager 其他方法造成死锁
2. 长时间持锁影响性能

#### 修复方案

```cpp
/**
 * @brief 遍历所有纹理
 * @param callback 回调函数 (name, texture)
 * 
 * @warning 回调函数的限制：
 *  1. ⚠️ 不要在回调中调用 ResourceManager 的任何方法（会导致死锁）
 *  2. ⚠️ 不要长时间持有纹理对象的内部锁
 *  3. ⚠️ 不要在回调中进行阻塞操作或长时间计算
 *  4. ✅ 如需修改资源，应在回调中记录，退出后再处理
 * 
 * @example 正确用法
 * @code
 * std::vector<std::string> toRemove;
 * manager.ForEachTexture([&](const std::string& name, Ref<Texture> tex) {
 *     if (ShouldRemove(tex)) {
 *         toRemove.push_back(name);  // 只记录，不删除
 *     }
 * });
 * 
 * // 在循环外删除
 * for (const auto& name : toRemove) {
 *     manager.RemoveTexture(name);
 * }
 * @endcode
 * 
 * @example 错误用法 ⚠️
 * @code
 * manager.ForEachTexture([&](const std::string& name, Ref<Texture> tex) {
 *     manager.RemoveTexture(name);  // ❌ 死锁！
 * });
 * @endcode
 */
void ForEachTexture(std::function<void(const std::string&, Ref<Texture>)> callback);
```

同样的文档更新应用到：
- ✅ `ForEachMesh()`
- ✅ `ForEachMaterial()`
- ✅ `ForEachShader()`

#### 可选实现改进

考虑提供安全版本（不持锁）：
```cpp
/**
 * @brief 安全地遍历所有纹理（无死锁风险）
 * 
 * 先复制所有纹理的引用，然后在锁外遍历。
 * 性能稍差但更安全。
 */
void ForEachTextureSafe(std::function<void(const std::string&, Ref<Texture>)> callback) {
    std::vector<std::pair<std::string, Ref<Texture>>> snapshot;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot.reserve(m_textures.size());
        for (const auto& [name, entry] : m_textures) {
            snapshot.emplace_back(name, entry.resource);
        }
    }
    
    // 在锁外遍历
    for (const auto& [name, texture] : snapshot) {
        callback(name, texture);
    }
}
```

---

## 🟢 低优先级改进项 (长期优化)

### 💡 TODO-7: 添加整数溢出保护

**严重程度**: 🟢 低  
**影响**: 极端情况下可能溢出  
**状态**: ⬜ 未修复

#### 问题描述
**文件**: `src/rendering/texture.cpp`  
**位置**: 第 443 行

```cpp
size_t baseMemory = static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * bytesPerPixel;
```

虽然有 8192 大小检查，但添加溢出保护更安全。

#### 修复方案

```cpp
size_t Texture::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_textureID == 0) {
        return 0;
    }
    
    // 计算每像素字节数
    size_t bytesPerPixel = 0;
    switch (m_format) {
        case TextureFormat::RGB:          bytesPerPixel = 3; break;
        case TextureFormat::RGBA:         bytesPerPixel = 4; break;
        case TextureFormat::RED:          bytesPerPixel = 1; break;
        case TextureFormat::RG:           bytesPerPixel = 2; break;
        case TextureFormat::Depth:        bytesPerPixel = 4; break;
        case TextureFormat::DepthStencil: bytesPerPixel = 4; break;
    }
    
    // 溢出检查
    if (m_width > 0 && m_height > SIZE_MAX / m_width) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Texture::GetMemoryUsage: Size calculation overflow"));
        return SIZE_MAX;
    }
    
    size_t pixelCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);
    
    if (pixelCount > SIZE_MAX / bytesPerPixel) {
        HANDLE_ERROR(RENDER_WARNING(ErrorCode::OutOfRange, 
                                   "Texture::GetMemoryUsage: Memory calculation overflow"));
        return SIZE_MAX;
    }
    
    size_t baseMemory = pixelCount * bytesPerPixel;
    
    // 如果有 mipmap，大约增加 1/3 的内存
    if (m_hasMipmap) {
        if (baseMemory > SIZE_MAX / 4 * 3) {
            return SIZE_MAX;
        }
        baseMemory = baseMemory * 4 / 3;
    }
    
    return baseMemory;
}
```

---

### 💡 TODO-8: 改进异常安全性

**严重程度**: 🟢 低  
**影响**: 提高鲁棒性  
**状态**: ⬜ 未修复

#### 建议
在关键路径添加异常捕获：

```cpp
bool Mesh::Upload() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    if (m_Vertices.empty()) {
        Logger::GetInstance().Warning("Mesh::Upload - No vertices to upload");
        return false;
    }
    
    try {
        // 清理旧资源
        if (m_Uploaded) {
            // ... 清理代码
        }
        
        // OpenGL 操作
        GL_THREAD_CHECK();
        glGenVertexArrays(1, &m_VAO);
        // ... 其他 OpenGL 操作
        
        m_Uploaded = true;
        return true;
        
    } catch (const std::exception& e) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::UnknownError, 
                                 "Mesh::Upload: Exception during upload - " + std::string(e.what())));
        // 清理部分创建的资源
        if (m_VAO != 0) {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        return false;
    } catch (...) {
        HANDLE_ERROR(RENDER_ERROR(ErrorCode::UnknownError, 
                                 "Mesh::Upload: Unknown exception during upload"));
        return false;
    }
}
```

#### 影响范围
建议添加到：
- ✅ `Mesh::Upload()`
- ✅ `Texture::LoadFromFile()`
- ✅ `Shader::LoadFromFile()`
- ✅ `Material::Bind()`

---

### 💡 TODO-9: 性能优化 - 减少不必要的拷贝

**严重程度**: 🟢 低  
**影响**: 性能提升  
**状态**: ⬜ 未修复

#### 优化点 1: Material::GetTextureNames()

**当前实现**:
```cpp
std::vector<std::string> Material::GetTextureNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_textures.size());
    for (const auto& pair : m_textures) {
        names.push_back(pair.first);  // 拷贝字符串
    }
    return names;
}
```

**优化建议**:
```cpp
// 方案1: 使用输出参数避免返回拷贝
void Material::GetTextureNames(std::vector<std::string>& outNames) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outNames.clear();
    outNames.reserve(m_textures.size());
    for (const auto& pair : m_textures) {
        outNames.push_back(pair.first);
    }
}

// 方案2: 使用回调避免拷贝
void Material::ForEachTextureName(std::function<void(const std::string&)> callback) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_textures) {
        callback(pair.first);
    }
}
```

#### 优化点 2: 使用 std::atomic 优化频繁访问的getter

```cpp
class Mesh {
private:
    std::atomic<size_t> m_vertexCount{0};
    std::atomic<size_t> m_indexCount{0};
    
public:
    size_t GetVertexCount() const {
        return m_vertexCount.load(std::memory_order_relaxed);
    }
    
    void SetVertices(const std::vector<Vertex>& vertices) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Vertices = vertices;
        m_vertexCount.store(vertices.size(), std::memory_order_release);
        m_Uploaded = false;
    }
};
```

---

## 📋 修复检查清单

### 开始修复前
- [ ] 备份当前代码到新分支 `git checkout -b security-fixes`
- [ ] 确保所有现有测试通过
- [ ] 记录当前性能基准

### 修复过程中
- [ ] 每修复一个问题提交一次 commit
- [ ] Commit 信息格式: `[Security] Fix TODO-X: 简短描述`
- [ ] 每个修复都添加单元测试

### 修复完成后
- [ ] 运行完整测试套件
- [ ] 使用 Valgrind/AddressSanitizer 检查内存问题
- [ ] 使用 ThreadSanitizer 检查线程安全问题
- [ ] 性能对比测试（确保修复没有引入性能倒退）
- [ ] 更新相关文档
- [ ] Code Review

---

## 🔧 推荐的修复顺序

### 第一阶段：关键安全问题（1天）
1. ✅ TODO-1: UniformManager 栈数组溢出
2. ✅ TODO-2: 指针参数空指针检查

**验证**: 运行所有 shader 相关测试

### 第二阶段：边界检查增强（1天）
3. ✅ TODO-3: UpdateVertices 边界检查
4. ✅ TODO-4: RecalculateNormals 错误处理
5. ✅ TODO-5: CalculateBounds 防御性编程

**验证**: 运行所有 mesh 相关测试

### 第三阶段：文档和警告（半天）
6. ✅ TODO-6: ForEach 方法文档

**验证**: Review 文档更新

### 第四阶段：长期优化（可选，分多次进行）
7. ⏳ TODO-7: 整数溢出保护
8. ⏳ TODO-8: 异常安全性
9. ⏳ TODO-9: 性能优化

---

## 📊 测试计划

### 单元测试

创建新测试文件：`tests/security_tests.cpp`

```cpp
#include <gtest/gtest.h>
#include "render/uniform_manager.h"
#include "render/mesh.h"

// TODO-1 测试
TEST(SecurityTest, UniformManagerLongNames) {
    // 测试超长 uniform 名称不会崩溃
}

// TODO-2 测试
TEST(SecurityTest, UniformManagerNullPointer) {
    UniformManager mgr(validProgramID);
    mgr.SetIntArray("test", nullptr, 10);  // 应该不崩溃
}

// TODO-3 测试
TEST(SecurityTest, MeshUpdateVerticesBounds) {
    Mesh mesh;
    // 测试各种边界情况
}
```

### 集成测试

```bash
# 内存安全检查
valgrind --leak-check=full --show-leak-kinds=all ./bin/test_program

# 地址消毒器
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
./bin/test_program

# 线程消毒器
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..
./bin/test_program
```

---

## 📈 预期成果

修复完成后，项目将达到：

| 指标 | 修复前 | 修复后 | 提升 |
|------|--------|--------|------|
| 内存安全 | 9/10 | 10/10 | ⬆️ 11% |
| 边界检查 | 7/10 | 9/10 | ⬆️ 29% |
| 指针安全 | 8/10 | 10/10 | ⬆️ 25% |
| 文档完整性 | 7/10 | 9/10 | ⬆️ 29% |
| **综合评分** | **8.5/10** | **9.5/10** | **⬆️ 12%** |

---

## 📚 参考资料

- [CERT C++ Coding Standard](https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=88046682)
- [Google C++ Style Guide - Safety](https://google.github.io/styleguide/cppguide.html)
- [CWE-120: Buffer Copy without Checking Size of Input](https://cwe.mitre.org/data/definitions/120.html)
- [CWE-476: NULL Pointer Dereference](https://cwe.mitre.org/data/definitions/476.html)

---

## 📝 修复进度跟踪

| TODO ID | 优先级 | 状态 | 修复人 | 完成日期 | 备注 |
|---------|--------|------|--------|----------|------|
| TODO-1 | 🔴 高 | ⬜ 未开始 | - | - | 栈数组溢出 |
| TODO-2 | 🔴 高 | ⬜ 未开始 | - | - | 空指针检查 |
| TODO-3 | 🟡 中 | ⬜ 未开始 | - | - | 边界检查 |
| TODO-4 | 🟡 中 | ⬜ 未开始 | - | - | 错误处理 |
| TODO-5 | 🟡 中 | ⬜ 未开始 | - | - | 防御性编程 |
| TODO-6 | 🟡 中 | ⬜ 未开始 | - | - | 文档更新 |
| TODO-7 | 🟢 低 | ⬜ 未开始 | - | - | 溢出保护 |
| TODO-8 | 🟢 低 | ⬜ 未开始 | - | - | 异常安全 |
| TODO-9 | 🟢 低 | ⬜ 未开始 | - | - | 性能优化 |

---

**最后更新**: 2025-11-01  
**下次审查**: 修复完成后

