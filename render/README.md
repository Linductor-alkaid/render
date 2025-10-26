# RenderEngine

一个基于 OpenGL 和 SDL3 构建的现代渲染引擎。

## 特性

- OpenGL 4.5+ 渲染后端
- SDL3 窗口管理
- 模块化架构设计
- 完整的 2D 和 3D 渲染支持
- ECS 集成

## 构建

### Windows

使用 Visual Studio 2022:

```batch
# 配置项目
build.bat

# 编译
cmake --build build --config Release
```

### Linux/macOS

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 依赖项

- **SDL3**: 窗口管理和输入处理
- **Eigen3**: 数学库
- **GLAD**: OpenGL 加载库
- **OpenGL 4.5+**: 渲染 API

所有第三方库已包含在 `third_party/` 目录中。

## 示例

运行基础窗口示例:

```batch
.\build\bin\Release\01_basic_window.exe
```

## 文档

完整文档请查看 [docs/](docs/README.md) 目录。

- [架构设计](docs/ARCHITECTURE.md)
- [API 参考](docs/API_REFERENCE.md)
- [开发指南](docs/DEVELOPMENT_GUIDE.md)
- [Phase 1 TODO](docs/todolists/PHASE1_BASIC_RENDERING.md)

## 许可证

MIT License

## 作者

RenderEngine Team

