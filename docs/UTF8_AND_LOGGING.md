# UTF-8 编码和日志系统使用说明

## UTF-8 编码支持

### 编译器设置

项目已在 `CMakeLists.txt` 中配置了全局 UTF-8 支持：

```cmake
# 编译选项
if(MSVC)
    # UTF-8 编码支持
    add_compile_options(/utf-8)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # UTF-8 编码支持
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
```

### Windows 控制台 UTF-8 支持

Logger 类在初始化时自动配置 Windows 控制台支持 UTF-8 输出：

```cpp
#ifdef _WIN32
    // Windows 控制台 UTF-8 支持
    SetConsoleOutputCP(CP_UTF8);
    // 启用 ANSI 转义序列支持（Windows 10+）
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
```

这确保了：
- 控制台正确显示中文字符
- 不会出现乱码
- 支持 Windows 10 及以上版本的 ANSI 颜色输出（未来可用）

## 日志系统

### 日志文件功能

Logger 类支持同时输出到控制台和文件：

#### 启用日志文件

```cpp
// 自动生成带时间戳的日志文件（保存到 logs/ 目录）
Render::Logger::GetInstance().SetLogToFile(true);

// 或指定文件名
Render::Logger::GetInstance().SetLogToFile(true, "my_log.txt");
```

#### 日志文件特性

1. **UTF-8 BOM**: 日志文件自动添加 UTF-8 BOM，便于文本编辑器识别
2. **时间戳文件名**: 默认生成格式为 `render_YYYYMMDD_HHMMSS.log`
3. **自动创建目录**: 如果 `logs/` 目录不存在会自动创建
4. **文件头信息**: 包含创建时间和说明

#### 日志文件示例

```
﻿========================================
RenderEngine 日志文件
创建时间: 2025-10-27 22:45:13
========================================
[INFO] [22:45:13] === 纹理加载测试 ===
[INFO] [22:45:13] 日志文件: logs/render_20251027_224513.log
[INFO] [22:45:13] SDL3_image 已就绪
...
```

### 日志级别

```cpp
// 设置日志级别
Render::Logger::GetInstance().SetLogLevel(Render::LogLevel::Debug);

// 记录不同级别的日志
Render::Logger::GetInstance().Debug("调试信息");
Render::Logger::GetInstance().Info("常规信息");
Render::Logger::GetInstance().Warning("警告信息");
Render::Logger::GetInstance().Error("错误信息");
```

### 便捷宏（可选）

如果想使用更简洁的语法，可以使用预定义的宏：

```cpp
LOG_DEBUG("调试信息");
LOG_INFO("常规信息");
LOG_WARNING("警告信息");
LOG_ERROR("错误信息");
```

### 日志输出控制

```cpp
// 禁用控制台输出（仅输出到文件）
Render::Logger::GetInstance().SetLogToConsole(false);

// 禁用文件输出（仅输出到控制台）
Render::Logger::GetInstance().SetLogToFile(false);

// 获取当前日志文件路径
std::string logPath = Render::Logger::GetInstance().GetCurrentLogFile();
```

## 使用示例

### 示例：纹理加载测试

```cpp
int main() {
    // 1. 启用日志文件
    Render::Logger::GetInstance().SetLogToFile(true);
    
    // 2. 输出日志（中文不会乱码）
    Render::Logger::GetInstance().Info("=== 纹理加载测试 ===");
    Render::Logger::GetInstance().Info("日志文件: " + 
        Render::Logger::GetInstance().GetCurrentLogFile());
    
    // 3. 程序运行...
    auto texture = std::make_shared<Render::Texture>();
    if (texture->LoadFromFile("test.png")) {
        Render::Logger::GetInstance().Info("加载纹理成功");
    } else {
        Render::Logger::GetInstance().Error("加载纹理失败");
    }
    
    // 4. 退出时提醒日志位置
    Render::Logger::GetInstance().Info("程序结束");
    Render::Logger::GetInstance().Info("日志已保存到: " + 
        Render::Logger::GetInstance().GetCurrentLogFile());
    
    return 0;
}
```

### 输出效果

**控制台输出：**
```
[INFO] [22:45:13] === 纹理加载测试 ===
[INFO] [22:45:13] 日志文件: logs/render_20251027_224513.log
[INFO] [22:45:14] 加载纹理: test.png (512x512)
[INFO] [22:45:15] 程序结束
[INFO] [22:45:15] 日志已保存到: logs/render_20251027_224513.log
```

**日志文件内容（logs/render_20251027_224513.log）：**
```
﻿========================================
RenderEngine 日志文件
创建时间: 2025-10-27 22:45:13
========================================
[INFO] [22:45:13] === 纹理加载测试 ===
[INFO] [22:45:13] 日志文件: logs/render_20251027_224513.log
[INFO] [22:45:14] 加载纹理: test.png (512x512)
[INFO] [22:45:15] 程序结束
[INFO] [22:45:15] 日志已保存到: logs/render_20251027_224513.log
```

## 注意事项

1. **Windows PowerShell/CMD**: 确保终端使用 UTF-8 编码。可以在 PowerShell 中运行：
   ```powershell
   chcp 65001
   ```

2. **日志文件位置**: 默认保存在 `logs/` 目录，该目录会自动创建

3. **多次运行**: 每次运行会生成新的日志文件（基于时间戳），不会覆盖旧日志

4. **性能考虑**: 日志系统使用互斥锁保证线程安全，但频繁写入可能影响性能

5. **文本编辑器**: UTF-8 BOM 可确保 Windows 记事本等编辑器正确识别编码

## 相关文件

- `CMakeLists.txt` - 编译器 UTF-8 配置
- `src/utils/logger.cpp` - Logger 实现
- `include/render/logger.h` - Logger 接口
- `examples/05_texture_test.cpp` - 使用示例

---

[返回文档首页](README.md)

