[返回文档首页](../README.md)

---

# 工具链集成指南

**更新时间**: 2025-11-21  
**参考文档**: [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md), [Module_Guide.md](Module_Guide.md)

---

## 1. 概述

工具链集成接口为 Phase 2 的工具链（HUD、材质面板、Shader面板、LayerMask编辑器、场景图可视化工具等）提供统一的数据访问接口。这些接口位于 `render/application/toolchain/` 目录下。

### 核心接口

1. **ModuleRegistry 模块状态查询接口** - HUD读取模块状态
2. **MaterialShaderPanelDataSource** - 材质/Shader面板数据源
3. **LayerMaskEditorDataSource** - LayerMask编辑器集成
4. **SceneGraphVisualizerDataSource** - 场景图可视化工具

---

## 2. HUD读取模块状态接口

### 2.1 接口位置

`ModuleRegistry` 类提供了模块状态查询接口，位于 `include/render/application/module_registry.h`。

### 2.2 核心接口

```cpp
// 模块状态信息
struct ModuleState {
    std::string name;
    bool active = false;
    bool registered = false;
    ModuleDependencies dependencies;
    int preFramePriority = 0;
    int postFramePriority = 0;
};

// 获取模块状态
std::optional<ModuleState> GetModuleState(std::string_view name) const;

// 获取所有模块状态列表
std::vector<ModuleState> GetAllModuleStates() const;

// 检查模块是否激活/已注册
bool IsModuleActive(std::string_view name) const;
bool IsModuleRegistered(std::string_view name) const;
```

### 2.3 使用示例

```cpp
#include "render/application/module_registry.h"
#include "render/application/application_host.h"

// 获取ApplicationHost实例
ApplicationHost& host = GetApplicationHost();
auto& moduleRegistry = host.GetModuleRegistry();

// 获取所有模块状态
auto allStates = moduleRegistry.GetAllModuleStates();

// 在HUD中显示模块信息
for (const auto& state : allStates) {
    ImGui::Text("%s: %s", state.name.c_str(), 
                state.active ? "Active" : "Inactive");
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("PreFrame Priority: %d", state.preFramePriority);
        ImGui::Text("PostFrame Priority: %d", state.postFramePriority);
        
        if (!state.dependencies.empty()) {
            ImGui::Text("Dependencies:");
            for (const auto& dep : state.dependencies) {
                ImGui::BulletText("%s", dep.c_str());
            }
        }
        ImGui::EndTooltip();
    }
}
```

---

## 3. 材质/Shader面板数据源接口

### 3.1 接口位置

`MaterialShaderPanelDataSource` 类提供材质和着色器的查询接口，位于 `include/render/application/toolchain/material_shader_panel.h`。

### 3.2 核心接口

```cpp
// 材质信息
struct MaterialInfo {
    std::string name;
    std::string shaderName;
    Color ambientColor;
    Color diffuseColor;
    Color specularColor;
    Color emissiveColor;
    float shininess = 0.0f;
    float metallic = 0.0f;
    float roughness = 0.0f;
    std::vector<std::string> textureNames;
    bool isValid = false;
};

// Uniform信息（用于工具链面板）
struct UniformInfo {
    std::string name;
    int location = -1;
    std::string typeName;  // 类型名称（如 "float", "vec3", "mat4" 等）
};

// 着色器信息
struct ShaderInfo {
    std::string name;
    uint32_t programID = 0;
    std::vector<UniformInfo> uniforms;
    bool isValid = false;
};

// 数据源类
class MaterialShaderPanelDataSource {
public:
    MaterialShaderPanelDataSource(ResourceManager& resourceManager);
    
    // 材质查询
    std::vector<std::string> GetMaterialNames() const;
    std::optional<MaterialInfo> GetMaterialInfo(const std::string& name) const;
    std::vector<MaterialInfo> GetAllMaterialInfos() const;
    
    // 着色器查询
    std::vector<std::string> GetShaderNames() const;
    std::optional<ShaderInfo> GetShaderInfo(const std::string& name) const;
    std::vector<ShaderInfo> GetAllShaderInfos() const;
    
    // 材质修改（供编辑器使用）
    bool UpdateMaterialProperty(const std::string& name, 
                                const std::string& propertyName, 
                                const std::string& value);
};
```

### 3.3 使用示例

```cpp
#include "render/application/toolchain/material_shader_panel.h"
#include "render/resource_manager.h"

// 创建数据源
auto& resourceManager = ResourceManager::GetInstance();
MaterialShaderPanelDataSource dataSource(resourceManager);

// 显示材质列表
auto materialNames = dataSource.GetMaterialNames();
for (const auto& name : materialNames) {
    if (ImGui::Selectable(name.c_str())) {
        auto info = dataSource.GetMaterialInfo(name);
        if (info.has_value()) {
            // 显示材质属性编辑器
            ShowMaterialEditor(info.value());
        }
    }
}

// 显示材质属性
auto materialInfo = dataSource.GetMaterialInfo("default.material");
if (materialInfo.has_value()) {
    ImGui::ColorEdit3("Diffuse", materialInfo->diffuseColor.data());
    ImGui::SliderFloat("Metallic", &materialInfo->metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &materialInfo->roughness, 0.0f, 1.0f);
    
    // 更新材质属性
    if (ImGui::Button("Apply")) {
        dataSource.UpdateMaterialProperty("default.material", 
                                         "metallic", 
                                         std::to_string(materialInfo->metallic));
    }
}

// 显示着色器列表
auto shaderNames = dataSource.GetShaderNames();
for (const auto& name : shaderNames) {
    auto info = dataSource.GetShaderInfo(name);
    if (info.has_value()) {
        ImGui::Text("Shader: %s", name.c_str());
        ImGui::Text("Program ID: %u", info->programID);
        
        // 显示Uniform列表
        if (ImGui::TreeNode("Uniforms")) {
            for (const auto& uniform : info->uniforms) {
                ImGui::BulletText("%s: %s", uniform.name.c_str(), 
                                 uniform.typeName.c_str());
            }
            ImGui::TreePop();
        }
    }
}
```

---

## 4. LayerMask编辑器集成接口

### 4.1 接口位置

`LayerMaskEditorDataSource` 类提供LayerMask的查询和编辑功能，位于 `include/render/application/toolchain/layermask_editor.h`。

### 4.2 核心接口

```cpp
// LayerMask信息
struct LayerMaskInfo {
    uint32_t layerMask = 0xFFFFFFFF;
    std::string name;
    std::vector<RenderLayerId> enabledLayers;
};

// 数据源类
class LayerMaskEditorDataSource {
public:
    LayerMaskEditorDataSource(RenderLayerRegistry& registry);
    
    // LayerMask转换
    std::vector<RenderLayerId> LayerMaskToLayers(uint32_t layerMask) const;
    uint32_t LayersToLayerMask(const std::vector<RenderLayerId>& layers) const;
    
    // 层级状态查询
    bool IsLayerInMask(uint32_t layerMask, RenderLayerId layerId) const;
    uint32_t SetLayerInMask(uint32_t layerMask, RenderLayerId layerId, bool enabled) const;
    
    // 获取所有层级信息
    std::vector<RenderLayerRecord> GetAllLayers() const;
    
    // LayerMask操作
    uint32_t CreateEmptyMask() const;
    uint32_t CreateFullMask() const;
    bool ValidateLayerMask(uint32_t layerMask) const;
};
```

### 4.3 使用示例

```cpp
#include "render/application/toolchain/layermask_editor.h"
#include "render/renderer.h"

// 获取Renderer的LayerRegistry
auto& renderer = GetRenderer();
auto& layerRegistry = renderer.GetLayerRegistry();

// 创建数据源
LayerMaskEditorDataSource dataSource(layerRegistry);

// 显示LayerMask编辑器
uint32_t currentMask = cameraComponent.layerMask;

// 显示所有层级
auto allLayers = dataSource.GetAllLayers();
for (const auto& record : allLayers) {
    bool enabled = dataSource.IsLayerInMask(currentMask, record.descriptor.id);
    
    if (ImGui::Checkbox(record.descriptor.name.c_str(), &enabled)) {
        currentMask = dataSource.SetLayerInMask(currentMask, 
                                                record.descriptor.id, 
                                                enabled);
        // 更新CameraComponent
        cameraComponent.layerMask = currentMask;
    }
}

// 快捷操作
if (ImGui::Button("Select All")) {
    currentMask = dataSource.CreateFullMask();
    cameraComponent.layerMask = currentMask;
}

if (ImGui::Button("Clear All")) {
    currentMask = dataSource.CreateEmptyMask();
    cameraComponent.layerMask = currentMask;
}
```

---

## 5. 场景图可视化工具接口

### 5.1 接口位置

`SceneGraphVisualizerDataSource` 类提供场景图的可视化功能，位于 `include/render/application/toolchain/scene_graph_visualizer.h`。

### 5.2 核心接口

```cpp
// 场景图节点信息
struct SceneNodeInfo {
    std::string name;
    bool active = true;
    bool attached = false;
    bool entered = false;
    size_t childCount = 0;
    std::vector<std::string> childrenNames;
    size_t resourceCount = 0;
};

// 数据源类
class SceneGraphVisualizerDataSource {
public:
    SceneGraphVisualizerDataSource();
    
    void SetSceneGraph(SceneGraph* sceneGraph);
    
    // 节点查询
    std::optional<SceneNodeInfo> GetRootNodeInfo() const;
    std::optional<SceneNodeInfo> GetNodeInfo(const std::string& nodeName) const;
    std::vector<SceneNodeInfo> GetChildNodeInfos(const std::string& nodeName) const;
    std::vector<SceneNodeInfo> GetAllNodeInfos() const;
    
    // 遍历场景图
    void ForEachNode(std::function<void(const SceneNodeInfo&, int depth)> callback) const;
    
    // 场景图结构
    std::string GetTreeStructure() const;
    
    // 统计信息
    struct SceneGraphStats {
        size_t totalNodes = 0;
        size_t activeNodes = 0;
        size_t attachedNodes = 0;
        size_t enteredNodes = 0;
        size_t totalResources = 0;
        int maxDepth = 0;
    };
    SceneGraphStats GetStats() const;
};
```

### 5.3 使用示例

```cpp
#include "render/application/toolchain/scene_graph_visualizer.h"
#include "render/application/scene_manager.h"

// 获取当前场景的SceneGraph
auto& sceneManager = GetSceneManager();
auto* activeScene = sceneManager.GetActiveScene();
if (!activeScene) {
    return;
}

// 创建数据源
SceneGraphVisualizerDataSource visualizer;
// 注意：需要从Scene获取SceneGraph，这里假设Scene有GetSceneGraph方法
// visualizer.SetSceneGraph(&activeScene->GetSceneGraph());

// 显示场景图树形结构
auto treeStructure = visualizer.GetTreeStructure();
ImGui::Text("Scene Graph:");
ImGui::BeginChild("SceneGraphTree", ImVec2(0, 300), true);
ImGui::TextUnformatted(treeStructure.c_str());
ImGui::EndChild();

// 显示统计信息
auto stats = visualizer.GetStats();
ImGui::Text("Total Nodes: %zu", stats.totalNodes);
ImGui::Text("Active Nodes: %zu", stats.activeNodes);
ImGui::Text("Max Depth: %d", stats.maxDepth);

// 显示节点列表
auto allNodes = visualizer.GetAllNodeInfos();
if (ImGui::BeginTable("SceneGraphNodes", 4)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Active");
    ImGui::TableSetupColumn("Children");
    ImGui::TableSetupColumn("Resources");
    ImGui::TableHeadersRow();
    
    for (const auto& node : allNodes) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", node.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s", node.active ? "Yes" : "No");
        ImGui::TableNextColumn();
        ImGui::Text("%zu", node.childCount);
        ImGui::TableNextColumn();
        ImGui::Text("%zu", node.resourceCount);
    }
    
    ImGui::EndTable();
}
```

---

## 6. 完整集成示例

以下是一个完整的工具链集成示例，展示如何在HUD中集成所有工具：

```cpp
#include "render/application/application_host.h"
#include "render/application/toolchain/material_shader_panel.h"
#include "render/application/toolchain/layermask_editor.h"
#include "render/application/toolchain/scene_graph_visualizer.h"
#include "render/resource_manager.h"
#include "render/renderer.h"

class ToolchainHUD {
public:
    void Initialize(ApplicationHost& host, Renderer& renderer) {
        // 初始化数据源
        auto& resourceManager = ResourceManager::GetInstance();
        m_materialShaderPanel = std::make_unique<MaterialShaderPanelDataSource>(resourceManager);
        
        auto& layerRegistry = renderer.GetLayerRegistry();
        m_layermaskEditor = std::make_unique<LayerMaskEditorDataSource>(layerRegistry);
        
        m_sceneGraphVisualizer = std::make_unique<SceneGraphVisualizerDataSource>();
        
        m_host = &host;
        m_renderer = &renderer;
    }
    
    void Render() {
        if (!ImGui::Begin("Toolchain HUD")) {
            ImGui::End();
            return;
        }
        
        // 标签页
        if (ImGui::BeginTabBar("ToolchainTabs")) {
            // 模块状态标签页
            if (ImGui::BeginTabItem("Modules")) {
                RenderModuleStatus();
                ImGui::EndTabItem();
            }
            
            // 材质面板标签页
            if (ImGui::BeginTabItem("Materials")) {
                RenderMaterialPanel();
                ImGui::EndTabItem();
            }
            
            // 着色器面板标签页
            if (ImGui::BeginTabItem("Shaders")) {
                RenderShaderPanel();
                ImGui::EndTabItem();
            }
            
            // LayerMask编辑器标签页
            if (ImGui::BeginTabItem("LayerMask")) {
                RenderLayerMaskEditor();
                ImGui::EndTabItem();
            }
            
            // 场景图可视化标签页
            if (ImGui::BeginTabItem("Scene Graph")) {
                RenderSceneGraphVisualizer();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::End();
    }
    
private:
    void RenderModuleStatus() {
        auto& moduleRegistry = m_host->GetModuleRegistry();
        auto allStates = moduleRegistry.GetAllModuleStates();
        
        if (ImGui::BeginTable("Modules", 5)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Active");
            ImGui::TableSetupColumn("Registered");
            ImGui::TableSetupColumn("PreFrame Priority");
            ImGui::TableSetupColumn("PostFrame Priority");
            ImGui::TableHeadersRow();
            
            for (const auto& state : allStates) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", state.name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", state.active ? "Yes" : "No");
                ImGui::TableNextColumn();
                ImGui::Text("%s", state.registered ? "Yes" : "No");
                ImGui::TableNextColumn();
                ImGui::Text("%d", state.preFramePriority);
                ImGui::TableNextColumn();
                ImGui::Text("%d", state.postFramePriority);
            }
            
            ImGui::EndTable();
        }
    }
    
    void RenderMaterialPanel() {
        if (!m_materialShaderPanel) {
            return;
        }
        
        auto materialNames = m_materialShaderPanel->GetMaterialNames();
        
        // 显示材质列表
        ImGui::Text("Materials (%zu):", materialNames.size());
        ImGui::BeginChild("MaterialList", ImVec2(200, 400), true);
        
        for (const auto& name : materialNames) {
            if (ImGui::Selectable(name.c_str(), m_selectedMaterial == name)) {
                m_selectedMaterial = name;
            }
        }
        ImGui::EndChild();
        
        // 显示选中的材质信息
        if (!m_selectedMaterial.empty()) {
            ImGui::SameLine();
            ImGui::BeginGroup();
            
            auto materialInfo = m_materialShaderPanel->GetMaterialInfo(m_selectedMaterial);
            if (materialInfo.has_value()) {
                ImGui::Text("Material: %s", m_selectedMaterial.c_str());
                ImGui::Separator();
                
                // 显示材质属性编辑器
                static MaterialInfo editingInfo = materialInfo.value();
                
                ImGui::ColorEdit3("Ambient", editingInfo.ambientColor.data());
                ImGui::ColorEdit3("Diffuse", editingInfo.diffuseColor.data());
                ImGui::ColorEdit3("Specular", editingInfo.specularColor.data());
                ImGui::ColorEdit4("Emissive", editingInfo.emissiveColor.data());
                
                ImGui::Separator();
                
                ImGui::SliderFloat("Shininess", &editingInfo.shininess, 0.0f, 128.0f);
                ImGui::SliderFloat("Metallic", &editingInfo.metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &editingInfo.roughness, 0.0f, 1.0f);
                
                ImGui::Separator();
                
                // 显示着色器信息
                ImGui::Text("Shader: %s", editingInfo.shaderName.c_str());
                
                // 显示纹理列表
                if (!editingInfo.textureNames.empty()) {
                    if (ImGui::TreeNode("Textures (%zu)", editingInfo.textureNames.size())) {
                        for (const auto& texName : editingInfo.textureNames) {
                            ImGui::BulletText("%s", texName.c_str());
                        }
                        ImGui::TreePop();
                    }
                }
                
                ImGui::Separator();
                
                // 应用更改按钮
                if (ImGui::Button("Apply Changes")) {
                    // 更新材质属性
                    m_materialShaderPanel->UpdateMaterialProperty(
                        m_selectedMaterial, "diffuseColor", 
                        ColorToString(editingInfo.diffuseColor));
                    m_materialShaderPanel->UpdateMaterialProperty(
                        m_selectedMaterial, "metallic", 
                        std::to_string(editingInfo.metallic));
                    m_materialShaderPanel->UpdateMaterialProperty(
                        m_selectedMaterial, "roughness", 
                        std::to_string(editingInfo.roughness));
                    
                    // 刷新信息
                    materialInfo = m_materialShaderPanel->GetMaterialInfo(m_selectedMaterial);
                    if (materialInfo.has_value()) {
                        editingInfo = materialInfo.value();
                    }
                }
                
                // 显示引用计数
                long refCount = m_materialShaderPanel->GetMaterialReferenceCount(m_selectedMaterial);
                ImGui::Text("Reference Count: %ld", refCount);
            }
            
            ImGui::EndGroup();
        }
    }
    
    void RenderShaderPanel() {
        if (!m_materialShaderPanel) {
            return;
        }
        
        auto shaderNames = m_materialShaderPanel->GetShaderNames();
        
        // 显示着色器列表
        ImGui::Text("Shaders (%zu):", shaderNames.size());
        ImGui::BeginChild("ShaderList", ImVec2(200, 400), true);
        
        for (const auto& name : shaderNames) {
            if (ImGui::Selectable(name.c_str(), m_selectedShader == name)) {
                m_selectedShader = name;
            }
        }
        ImGui::EndChild();
        
        // 显示选中的着色器信息
        if (!m_selectedShader.empty()) {
            ImGui::SameLine();
            ImGui::BeginGroup();
            
            auto shaderInfo = m_materialShaderPanel->GetShaderInfo(m_selectedShader);
            if (shaderInfo.has_value()) {
                ImGui::Text("Shader: %s", m_selectedShader.c_str());
                ImGui::Separator();
                
                ImGui::Text("Program ID: %u", shaderInfo->programID);
                ImGui::Text("Valid: %s", shaderInfo->isValid ? "Yes" : "No");
                
                // 显示Uniform列表
                if (ImGui::TreeNode("Uniforms (%zu)", shaderInfo->uniforms.size())) {
                    for (const auto& uniform : shaderInfo->uniforms) {
                        ImGui::BulletText("%s: %s (Location: %d)", 
                                         uniform.name.c_str(),
                                         uniform.typeName.c_str(),
                                         uniform.location);
                    }
                    ImGui::TreePop();
                }
            }
            
            ImGui::EndGroup();
        }
    }
    
    void RenderLayerMaskEditor() {
        if (!m_layermaskEditor) {
            return;
        }
        
        ImGui::Text("LayerMask Editor");
        ImGui::Separator();
        
        // 显示当前选中的LayerMask（假设从某个CameraComponent获取）
        static uint32_t currentMask = 0xFFFFFFFF;
        
        ImGui::Text("Current LayerMask: 0x%08X", currentMask);
        
        // 显示所有层级
        auto allLayers = m_layermaskEditor->GetAllLayers();
        ImGui::Text("Layers (%zu):", allLayers.size());
        
        ImGui::BeginChild("LayerList", ImVec2(0, 300), true);
        for (const auto& record : allLayers) {
            bool enabled = m_layermaskEditor->IsLayerInMask(currentMask, record.descriptor.id);
            
            ImGui::PushID(static_cast<int>(record.descriptor.id.value));
            
            if (ImGui::Checkbox(record.descriptor.name.c_str(), &enabled)) {
                currentMask = m_layermaskEditor->SetLayerInMask(currentMask, 
                                                               record.descriptor.id, 
                                                               enabled);
            }
            
            // 显示层级详细信息
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("ID: %u", record.descriptor.id.value);
                ImGui::Text("Priority: %u", record.descriptor.priority);
                ImGui::Text("Type: %d", static_cast<int>(record.descriptor.type));
                ImGui::Text("Enabled: %s", record.state.enabled ? "Yes" : "No");
                ImGui::EndTooltip();
            }
            
            ImGui::PopID();
        }
        ImGui::EndChild();
        
        // 快捷操作按钮
        ImGui::Separator();
        if (ImGui::Button("Select All")) {
            currentMask = m_layermaskEditor->CreateFullMask();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) {
            currentMask = m_layermaskEditor->CreateEmptyMask();
        }
        ImGui::SameLine();
        if (ImGui::Button("Invert")) {
            uint32_t fullMask = m_layermaskEditor->CreateFullMask();
            currentMask = ~currentMask & fullMask;
        }
        
        // 验证LayerMask
        bool isValid = m_layermaskEditor->ValidateLayerMask(currentMask);
        if (!isValid) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: Invalid LayerMask");
        }
    }
    
    void RenderSceneGraphVisualizer() {
        if (!m_sceneGraphVisualizer) {
            return;
        }
        
        ImGui::Text("Scene Graph Visualizer");
        ImGui::Separator();
        
        // 显示统计信息
        auto stats = m_sceneGraphVisualizer->GetStats();
        ImGui::Text("Total Nodes: %zu", stats.totalNodes);
        ImGui::SameLine();
        ImGui::Text("Active: %zu", stats.activeNodes);
        ImGui::SameLine();
        ImGui::Text("Max Depth: %d", stats.maxDepth);
        
        ImGui::Separator();
        
        // 显示树形结构
        if (ImGui::TreeNode("Tree Structure")) {
            auto treeStructure = m_sceneGraphVisualizer->GetTreeStructure();
            ImGui::BeginChild("TreeView", ImVec2(0, 300), true);
            ImGui::TextUnformatted(treeStructure.c_str());
            ImGui::EndChild();
            ImGui::TreePop();
        }
        
        // 显示节点列表
        if (ImGui::TreeNode("Node List")) {
            auto allNodes = m_sceneGraphVisualizer->GetAllNodeInfos();
            
            if (ImGui::BeginTable("Nodes", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Active");
                ImGui::TableSetupColumn("Attached");
                ImGui::TableSetupColumn("Children");
                ImGui::TableSetupColumn("Resources");
                ImGui::TableHeadersRow();
                
                for (const auto& node : allNodes) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", node.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", node.active ? "Yes" : "No");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", node.attached ? "Yes" : "No");
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", node.childCount);
                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", node.resourceCount);
                }
                
                ImGui::EndTable();
            }
            
            ImGui::TreePop();
        }
    }
    
private:
    ApplicationHost* m_host = nullptr;
    Renderer* m_renderer = nullptr;
    
    std::unique_ptr<MaterialShaderPanelDataSource> m_materialShaderPanel;
    std::unique_ptr<LayerMaskEditorDataSource> m_layermaskEditor;
    std::unique_ptr<SceneGraphVisualizerDataSource> m_sceneGraphVisualizer;
    
    std::string m_selectedMaterial;
    std::string m_selectedShader;
    
    // 辅助函数
    std::string ColorToString(const Color& color) const {
        return std::to_string(color.r) + "," + std::to_string(color.g) + "," + 
               std::to_string(color.b) + "," + std::to_string(color.a);
    }
};
```

---

## 7. 最佳实践

### 7.1 性能考虑

1. **缓存查询结果**：工具链数据源接口的查询结果建议在HUD中缓存，避免每帧重复查询。
2. **延迟更新**：对于不经常变化的数据（如模块状态、材质列表），可以每几帧更新一次。
3. **批量操作**：使用 `GetAllMaterialInfos()` 而不是多次调用 `GetMaterialInfo()`，减少锁竞争。

```cpp
// ✅ 好：批量获取
auto allMaterials = dataSource.GetAllMaterialInfos();

// ❌ 不好：逐个获取
for (const auto& name : materialNames) {
    auto info = dataSource.GetMaterialInfo(name);
}
```

### 7.2 线程安全

所有工具链数据源接口都是线程安全的，可以在多线程环境中使用：

```cpp
// ✅ 安全：可以从其他线程访问
std::thread t([&]() {
    auto materials = dataSource.GetAllMaterialInfos();
    // 处理材质列表
});
```

### 7.3 错误处理

始终检查返回值：

```cpp
// ✅ 好：检查返回值
auto materialInfo = dataSource.GetMaterialInfo("material.name");
if (materialInfo.has_value()) {
    // 使用材质信息
} else {
    Logger::GetInstance().Warning("Material not found: material.name");
}
```

---

## 8. 常见问题

### Q: 如何获取当前场景的SceneGraph？

A: 需要通过 `SceneManager` 获取活动场景，然后从场景中获取 `SceneGraph`。目前场景类可能需要扩展接口来暴露 `SceneGraph`。

```cpp
auto& sceneManager = GetSceneManager();
auto* activeScene = sceneManager.GetActiveScene();
if (activeScene) {
    // 假设Scene有GetSceneGraph方法
    // auto& sceneGraph = activeScene->GetSceneGraph();
    // visualizer.SetSceneGraph(&sceneGraph);
}
```

### Q: 如何实时更新材质属性？

A: 使用 `UpdateMaterialProperty()` 方法更新材质属性，但注意这个更新是运行时修改，不会持久化到文件。

```cpp
dataSource.UpdateMaterialProperty("material.name", "metallic", "0.5");
```

### Q: LayerMask的掩码位索引如何对应到层级？

A: 每个 `RenderLayerDescriptor` 都有一个 `maskIndex` 字段（0-31），对应32位掩码的位索引。`LayerMaskEditorDataSource` 会自动处理转换。

```cpp
// 检查层级是否在掩码中
bool enabled = dataSource.IsLayerInMask(layerMask, layerId);

// 设置层级在掩码中的状态
uint32_t newMask = dataSource.SetLayerInMask(layerMask, layerId, true);
```

---

## 9. 扩展接口

如果需要扩展工具链接口，可以：

1. **添加新的数据源类**：在 `toolchain/` 目录下创建新的数据源类
2. **扩展现有接口**：在现有数据源类中添加新的查询方法
3. **自定义可视化**：基于数据源接口实现自定义的可视化工具

### 示例：添加性能监控数据源

```cpp
// include/render/application/toolchain/performance_monitor.h
class PerformanceMonitorDataSource {
public:
    struct PerformanceStats {
        float fps = 0.0f;
        float frameTime = 0.0f;
        uint32_t drawCalls = 0;
        size_t memoryUsage = 0;
    };
    
    PerformanceStats GetStats() const;
    // ...
};
```

---

## 10. 参考文档

- [APPLICATION_LAYER_PROGRESS_REPORT.md](APPLICATION_LAYER_PROGRESS_REPORT.md) - 应用层开发进度报告
- [Module_Guide.md](Module_Guide.md) - 模块开发指南
- [Scene_API.md](Scene_API.md) - 场景API文档
- [SCENE_MODULE_FRAMEWORK.md](../SCENE_MODULE_FRAMEWORK.md) - 场景模块框架设计

---

**上一章**: [Module_Guide.md](Module_Guide.md)  
**下一章**: [Scene_API.md](Scene_API.md)