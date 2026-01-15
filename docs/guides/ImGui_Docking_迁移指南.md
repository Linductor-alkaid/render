# ImGui Docking 分支迁移指南

## 概述

本指南将帮助您将项目中的 ImGui 从 `master` 分支切换到 `docking` 分支。Docking 分支提供了窗口停靠（Docking）和多视口（Multi-Viewport）功能，这些功能在 master 分支中不可用。

## 当前状态

- **当前分支**: `master`
- **当前版本**: 1.92.6 WIP
- **目标分支**: `docking`
- **项目位置**: `third_party/imgui`

## 主要功能差异

### Docking 分支新增功能

1. **窗口停靠系统（Docking）**
   - 允许窗口相互停靠，创建复杂的布局
   - 支持标签页界面
   - 可拖拽窗口进行停靠/取消停靠

2. **多视口支持（Multi-Viewport）**
   - 窗口可以移出主应用窗口
   - 显示为独立的操作系统级窗口
   - 支持多显示器环境

## 迁移步骤

### 1. 切换到 Docking 分支

```powershell
cd third_party/imgui
git fetch origin
git checkout docking
git pull origin docking
```

### 2. 更新初始化代码

在 `src/ui/ui_renderer_backend_imgui.cpp` 的 `Initialize` 方法中，需要启用 docking 功能：

**当前代码（第88-89行）：**
```cpp
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;       // Enable Gamepad Controls
```

**需要添加：**
```cpp
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;       // Enable Gamepad Controls
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
// 可选：启用多视口支持
// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable Multi-Viewport
```

### 3. 后端更新检查

Docking 分支的后端文件（`imgui_impl_sdl3.cpp` 和 `imgui_impl_opengl3.cpp`）可能需要更新以支持多视口功能。

**检查点：**
- 确保后端支持 `ImGuiBackendFlags_PlatformHasViewports` 和 `ImGuiBackendFlags_RendererHasViewports`
- 如果启用多视口，需要处理多个窗口和渲染上下文

### 4. 使用 Docking API

如果需要在代码中使用 docking 功能，可以在适当的位置添加：

```cpp
// 在 ImGui::NewFrame() 之后
ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
```

**示例位置**（在 `examples/64_imgui_ui_demo.cpp` 的 `RenderImGuiUI` 函数中）：
```cpp
void RenderImGuiUI() {
    // 创建全屏停靠空间（可选）
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    ImGui::End();
    
    // 其他窗口...
}
```

### 5. 编译和测试

1. **清理构建目录**（推荐）：
   ```powershell
   # 删除 build 目录或清理 CMake 缓存
   Remove-Item -Recurse -Force build
   ```

2. **重新配置 CMake**：
   ```powershell
   mkdir build
   cd build
   cmake ..
   ```

3. **编译项目**：
   ```powershell
   cmake --build . --config Release
   ```

4. **运行测试**：
   - 运行 `examples/64_imgui_ui_demo.cpp` 确保基本功能正常
   - 测试窗口是否可以正常显示和交互
   - 如果启用了多视口，测试窗口是否可以移出主窗口

## 注意事项

### ⚠️ 重要警告

1. **API 兼容性**
   - Docking 分支的 API 与 master 分支基本兼容，但有一些新增功能
   - 现有代码应该可以正常工作，但建议进行全面测试

2. **多视口功能**
   - 多视口功能需要后端支持，可能需要额外的平台特定代码
   - 如果不需要多视口，可以只启用 docking 而不启用 viewports

3. **性能影响**
   - Docking 功能会增加一些运行时开销
   - 多视口功能需要管理多个窗口，性能影响更明显

4. **Beta 状态**
   - Docking 和 Multi-Viewport 功能仍处于 beta 状态
   - 虽然被广泛使用，但可能仍有 bug 或 API 变更

### 配置选项

在 `imconfig.h` 中，通常不需要修改任何配置即可使用 docking 功能。但如果遇到问题，可以检查：

- `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` - 确保没有禁用需要的功能
- 其他编译选项应该与 master 分支兼容

### 代码检查清单

- [ ] 切换到 docking 分支
- [ ] 更新 `io.ConfigFlags` 启用 docking
- [ ] 检查后端文件是否需要更新
- [ ] 测试现有代码是否正常工作
- [ ] 如果使用 docking API，添加相应代码
- [ ] 全面测试所有使用 ImGui 的功能
- [ ] 检查是否有编译警告或错误

## 回退方案

如果切换到 docking 分支后遇到问题，可以随时切换回 master 分支：

```powershell
cd third_party/imgui
git checkout master
```

然后重新编译项目。

## 相关资源

- [ImGui Docking Wiki](https://github.com/ocornut/imgui/wiki/Docking)
- [Multi-Viewport Support Documentation](https://github.com/ocornut/imgui/wiki/Multi-Viewports)
- [ImGui GitHub Repository](https://github.com/ocornut/imgui)

## 常见问题

### Q: 切换分支后编译错误怎么办？

A: 首先确保完全清理了构建目录，然后重新配置和编译。如果仍有错误，检查后端文件是否需要更新。

### Q: 是否需要启用多视口？

A: 不一定。如果只需要窗口停靠功能，只需启用 `ImGuiConfigFlags_DockingEnable` 即可。多视口是可选的。

### Q: 现有代码是否需要修改？

A: 通常不需要。Docking 分支向后兼容 master 分支的 API。只有在需要使用 docking 功能时才需要添加新代码。

### Q: 性能会受影响吗？

A: Docking 功能本身开销很小。多视口功能需要管理多个窗口，会有一定性能影响，但通常可以接受。

## 更新日期

本文档最后更新：2025年1月
