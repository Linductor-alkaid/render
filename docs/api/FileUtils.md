# FileUtils API 参考

[返回 API 首页](README.md)

---

## 概述

`FileUtils` 提供文件操作工具函数（静态类）。

**头文件**: `render/file_utils.h`  
**命名空间**: `Render`

---

## 方法

### ReadFile

读取文件内容。

```cpp
static std::string ReadFile(const std::string& filepath);
```

**返回值**: 文件内容字符串，失败返回空字符串

**示例**:
```cpp
std::string shaderCode = FileUtils::ReadFile("shaders/basic.vert");
if (shaderCode.empty()) {
    LOG_ERROR("Failed to read file");
}
```

---

### FileExists

检查文件是否存在。

```cpp
static bool FileExists(const std::string& filepath);
```

---

### GetFileExtension

获取文件扩展名。

```cpp
static std::string GetFileExtension(const std::string& filepath);
```

**示例**:
```cpp
std::string ext = FileUtils::GetFileExtension("shader.vert");
// ext = "vert"
```

---

### GetFileName

获取文件名（不含路径）。

```cpp
static std::string GetFileName(const std::string& filepath);
```

---

## 示例

```cpp
#include "render/file_utils.h"

// 读取文件
std::string content = FileUtils::ReadFile("config.txt");

// 检查文件
if (FileUtils::FileExists("texture.png")) {
    // 加载纹理...
}

// 获取扩展名
std::string ext = FileUtils::GetFileExtension(path);
if (ext == "vert" || ext == "frag") {
    // 是着色器文件
}
```

---

[上一篇: Logger](Logger.md) | [下一篇: Types](Types.md)

