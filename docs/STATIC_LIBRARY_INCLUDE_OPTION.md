# 静态库包含方案评估（推荐）⭐

## 方案概述

**不合并静态库**，而是将所有依赖的静态库文件（SDL3.lib, SDL3_image.lib, Assimp.lib等）也直接放入预编译包的lib文件夹中。用户在使用时需要链接所有这些库文件。

## 方案优势

### ✅ 改动范围极小
- **只修改打包脚本**：`scripts/package_prebuilt.ps1`（约30-50行代码）
- **可选修改CMakeLists.txt**：如果需要，可以在安装时包含依赖库（但这不是必须的）
- **不需要修改源代码**：完全不涉及C++代码
- **不需要复杂工具**：不需要lib.exe合并工具

### ✅ 符合标准做法
- 用户链接所有需要的库是标准的CMake做法
- 不需要特殊的合并操作
- 库文件保持独立，便于调试和维护

### ✅ 实现简单
- 只需要在打包时找到并复制库文件
- 不需要复杂的依赖分析
- 不需要处理合并可能出现的符号冲突

## 改动范围

### 文件改动

1. **修改文件**：`scripts/package_prebuilt.ps1`
   - 在复制lib目录后，额外复制第三方库文件
   - 约30-50行代码（主要是查找和复制库文件的逻辑）

2. **可选修改**：`CMakeLists.txt`
   - 如果需要在安装时包含依赖库，可以添加install规则
   - 但这不是必须的，因为可以在打包脚本中直接从build目录复制

3. **修改文件**：`docs/PREBUILT_LIBRARY_USAGE.md`
   - 更新文档，说明需要链接哪些库
   - 更新示例代码，展示如何链接所有库

### 代码改动

- ❌ **不需要修改任何C++源代码**
- ❌ **不需要修改任何头文件**
- ✅ **只修改打包脚本和文档**

## 实现方法

### 方法1：从build目录直接复制（推荐）

在打包脚本中，直接从build目录复制第三方库的静态库文件：

```powershell
# 第三方库文件位置（在build目录的子目录中）
$ThirdPartyLibs = @(
    "SDL3\lib\SDL3-static.lib",
    "SDL3_image\lib\SDL3_image-static.lib",
    "SDL3_ttf\lib\SDL3_ttf-static.lib",
    "assimp\code\Release\assimp.lib",  # 或Debug\assimpd.lib
    "meshoptimizer\Release\meshoptimizer.lib",
    "bullet3\src\BulletDynamics\Release\BulletDynamics.lib",
    "bullet3\src\BulletCollision\Release\BulletCollision.lib",
    "bullet3\src\LinearMath\Release\LinearMath.lib"
)

foreach ($lib in $ThirdPartyLibs) {
    $srcPath = Join-Path $BuildDirAbs $lib
    if (Test-Path $srcPath) {
        Copy-Item $srcPath $LibDest -Force
        Write-Host "  Copied: $lib" -ForegroundColor Green
    } else {
        Write-Host "  Warning: Library not found: $lib" -ForegroundColor Yellow
    }
}
```

**优点**：
- 实现简单
- 不需要修改CMakeLists.txt
- 直接从构建输出复制

**缺点**：
- 需要知道库文件的精确路径
- 不同构建配置（Debug/Release）可能有不同路径

### 方法2：通过CMake安装依赖库（可选）

在CMakeLists.txt中添加install规则，安装依赖库到安装目录：

```cmake
# 安装第三方静态库（可选，用于预编译包）
if(RENDER_ENGINE_INSTALL AND WIN32 AND NOT BUILD_SHARED_LIBS)
    # 安装SDL3静态库
    install(FILES $<TARGET_FILE:SDL3::SDL3-static>
        DESTINATION lib
        OPTIONAL
    )
    
    # 安装SDL3_image静态库
    install(FILES $<TARGET_FILE:SDL3_image::SDL3_image-static>
        DESTINATION lib
        OPTIONAL
    )
    
    # ... 其他库
endif()
```

然后在打包脚本中，库文件已经存在于安装目录的lib文件夹中。

**优点**：
- 使用CMake的标准机制
- 自动处理Debug/Release配置
- 路径更可靠

**缺点**：
- 需要修改CMakeLists.txt
- 可能影响正常的安装行为（但可以使用OPTIONAL避免）

## 用户使用方式

用户在使用预编译库时，需要链接所有依赖库：

### 方法1：直接包含（推荐）

```cmake
# 设置预编译库路径
set(RENDER_ENGINE_DIR "path/to/RenderEngine-prebuilt-Release-x64-Static")

# 包含头文件目录
target_include_directories(my_app PRIVATE
    ${RENDER_ENGINE_DIR}/include
)

# 链接所有库
target_link_libraries(my_app PRIVATE
    ${RENDER_ENGINE_DIR}/lib/RenderEngine.lib
    ${RENDER_ENGINE_DIR}/lib/SDL3-static.lib
    ${RENDER_ENGINE_DIR}/lib/SDL3_image-static.lib
    ${RENDER_ENGINE_DIR}/lib/SDL3_ttf-static.lib
    ${RENDER_ENGINE_DIR}/lib/assimp.lib
    ${RENDER_ENGINE_DIR}/lib/meshoptimizer.lib
    ${RENDER_ENGINE_DIR}/lib/BulletDynamics.lib
    ${RENDER_ENGINE_DIR}/lib/BulletCollision.lib
    ${RENDER_ENGINE_DIR}/lib/LinearMath.lib
    OpenGL::GL
    OpenMP::OpenMP_CXX  # 如果启用了OpenMP
)
```

### 方法2：使用find_package（如果CMake配置支持）

```cmake
find_package(RenderEngine REQUIRED)
target_link_libraries(my_app PRIVATE RenderEngine::RenderEngine)
```

但这需要CMake配置文件能够正确处理依赖库。

## 方案对比

| 特性 | 合并库方案 | **包含库方案（推荐）** |
|------|-----------|---------------------|
| 改动范围 | 中等（CMake脚本+合并工具） | **极小（只改打包脚本）** |
| 实现复杂度 | 中等（需要lib.exe工具） | **简单（直接复制文件）** |
| 用户使用 | 简单（只需链接一个库） | **标准（链接多个库）** |
| 维护成本 | 较高（需要维护合并逻辑） | **低（只需要复制文件）** |
| 调试便利性 | 一般（符号在合并库中） | **好（库文件独立）** |
| 跨平台支持 | 仅Windows/MSVC | **需要不同平台的实现** |

## 总结

**包含库方案（推荐）**：
- ✅ **改动范围最小**：只修改打包脚本
- ✅ **实现最简单**：直接复制库文件
- ✅ **符合标准做法**：用户链接所有需要的库
- ✅ **维护成本低**：不需要复杂的合并逻辑
- ✅ **不涉及源代码**：完全不影响代码

**建议**：采用包含库方案，在打包脚本中复制所有依赖的静态库文件到lib目录。
