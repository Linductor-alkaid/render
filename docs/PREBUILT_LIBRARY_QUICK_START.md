# 预编译库快速开始

## 快速构建预编译库

```powershell
# 一键完成构建、安装和打包
PowerShell -ExecutionPolicy Bypass -File ".\scripts\build_and_package.ps1"
```

完成后，预编译库将位于 `prebuilt/RenderEngine-prebuilt-Release-x64/` 目录。

## 在其他项目中使用

### 1. 复制预编译库到你的项目

将 `RenderEngine-prebuilt-Release-x64` 目录复制到你的项目中，例如：
```
your_project/
├── third_party/
│   └── RenderEngine-prebuilt-Release-x64/
└── CMakeLists.txt
```

### 2. 在你的 CMakeLists.txt 中添加

```cmake
cmake_minimum_required(VERSION 3.15)
project(YourProject)

set(CMAKE_CXX_STANDARD 20)

# 设置RenderEngine路径
set(RenderEngine_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/RenderEngine-prebuilt-Release-x64/lib/cmake/RenderEngine")

# 查找并链接RenderEngine
find_package(RenderEngine REQUIRED)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE RenderEngine::RenderEngine)

# 链接OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(your_app PRIVATE OpenGL::GL)
```

### 3. 编译你的项目

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

完成！现在你的项目可以使用预编译的RenderEngine库，无需重新编译整个RenderEngine项目。

## 更多信息

- 详细使用指南: [PREBUILT_LIBRARY_USAGE.md](PREBUILT_LIBRARY_USAGE.md)
- 示例代码: [examples/using_prebuilt_library/](../examples/using_prebuilt_library/)
