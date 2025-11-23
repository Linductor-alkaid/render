[返回文档首页](../README.md)

# Phase 2: 应用层与工具开发 TODO 列表

## 阶段目标

- [x] 提供可扩展的应用层框架，支持场景管理、模块化系统和统一事件流转 ✅ **已完成（2025-11-22，约90%）**
- [x] 打造可视化调试/配置工具链，加速内容生产与调试效率 ✅ **接口已完成（2025-11-21，UI界面待实现）**
- [ ] 建立基础复用 UI 组件库，遵循 Blender 风格交互约定 🔄 **进行中**
- [ ] 针对 Phase 1 功能栈进行性能与内存优化，强化产品级指标 🔄 **进行中**

**整体进度**: 约 90% 完成（2025-11-22）

**参考文档**: 
- [应用层开发进度汇报](../application/APPLICATION_LAYER_PROGRESS_REPORT.md)
- [应用层框架设计方案](../SCENE_MODULE_FRAMEWORK.md)

---

## 应用层基础设施 ✅ **已完成（2025-11-22）**

### 场景与模块框架 ✅
- [x] SceneGraph 扩展 ✅
  - [x] 场景节点生命周期（OnEnter/OnExit/OnUpdate）✅
  - [x] 热切换/多场景管理 ✅
  - [x] 场景资源预加载与释放策略（依赖 ResourceManager）✅
- [x] ModuleRegistry（原SystemRegistry）✅
  - [x] 系统注册/依赖排序 ✅
  - [x] 帧循环钩子（PreFrame/PostFrame）✅
  - [x] 热插拔调试接口 ✅
  - [x] 模块状态查询接口（工具链集成）✅
- [x] 应用层事件总线 ✅
  - [x] 输入、渲染、资源事件统一调度 ✅
  - [x] 事件监听优先级/过滤 ✅
  - [x] 与 UI/工具联动 ✅（工具链接口已实现）
  - [x] Blender风格操作映射 ✅
  - [x] 事件过滤器系统（TagEventFilter、SceneEventFilter、CompositeEventFilter）✅

### 示例与流程 ✅ **基本完成**

- [x] BootScene 示例 ✅
  - [x] 场景图节点实现 ✅
  - [x] Mesh渲染演示 ✅
  - [x] 基础光照 ✅
  - [x] 场景生命周期演示 ✅
- [x] 2D UI Demo（通过DebugHUDModule）✅
  - [x] HUD + 统计信息显示 ✅
  - [x] 文本渲染 ✅
  - [x] 渲染层级可视化开关 ✅
  - [x] Uniform/材质状态检查 ✅
- [x] 场景保存/加载 ✅
  - [x] JSON格式 ✅
  - [x] 资源路径校验 ✅
  - [x] ECS组件序列化支持（Transform、MeshRender、Camera、Light、Name）✅
  - [ ] CMake 资源列表同步（新增文件需更新）🔄 **待完善**

**测试程序**:
- ✅ `examples/53_event_system_test.cpp` - 事件系统测试
- ✅ `examples/54_module_hud_test.cpp` - 模块和HUD测试
- ✅ `examples/55_scene_serialization_test.cpp` - 场景序列化测试
- ✅ `examples/56_scene_manager_stress_test.cpp` - 多场景热切换压力测试
- ✅ `examples/57_toolchain_integration_test.cpp` - 工具链集成测试

---

## 可视化工具链 ✅ **接口已完成（2025-11-21），UI实现待完善**

### 调试与统计 ✅ **接口完成，UI实现待完善**
- [x] Renderer HUD（DebugHUDModule）✅
  - [x] FPS/帧时间/Draw Call/批次数 ✅
  - [x] 渲染层级可视化开关 ✅
  - [x] Uniform/材质状态检查 ✅
  - [x] 性能统计、渲染统计、资源统计、内存统计 ✅
- [ ] 性能分析面板 🔄 **待实现**
  - [ ] CPU/ GPU 时间轴（分阶段采样）
  - [ ] 资源使用视图（纹理/缓冲/内存）
  - [ ] 导出 CSV/JSON 报告
- [ ] 日志与诊断 🔄 **待实现**
  - [ ] Logger 级别过滤 UI
  - [ ] 运行时错误高亮与跳转
  - [ ] RenderDoc/调试工具集成指引

### 内容可视化工具 ✅ **数据源接口已完成（2025-11-21），UI界面待实现**
- [x] Layer Mask 编辑器数据源 ✅
  - [x] LayerMaskEditorDataSource 接口 ✅
  - [x] 层级启用/禁用、遮罩匹配测试 ✅
  - [x] LayerMask转换功能 ✅
  - [ ] UI界面实现 🔄 **待实现**
- [x] 材质/Shader 面板数据源 ✅
  - [x] MaterialShaderPanelDataSource 接口 ✅
  - [x] 材质信息查询、着色器信息查询 ✅
  - [x] 材质属性更新接口 ✅
  - [ ] UI界面实现 🔄 **待实现**
  - [ ] Shader 热重载与错误提示 🔄 **待实现**
  - [ ] 资源依赖图谱（Texture/Mesh/Material）🔄 **待实现**
- [x] 场景图可视化工具数据源 ✅
  - [x] SceneGraphVisualizerDataSource 接口 ✅
  - [x] 节点信息查询、树形结构输出 ✅
  - [ ] UI界面实现 🔄 **待实现**
- [x] 模块状态查询接口 ✅
  - [x] ModuleRegistry状态查询接口 ✅
  - [x] 模块激活状态、依赖关系、优先级查询 ✅
  - [ ] UI界面实现 🔄 **待实现**
- [ ] 动画调节器 🔄 **待实现**
  - [ ] SpriteAnimation 控制台（播放、速度、PingPong、事件回调）
  - [ ] 关键帧预览与事件编辑

**工具链集成文档**: ✅ [工具链集成指南](../application/Toolchain_Intergration_Guide.md)

---

## 基础复用 UI 开发 🔄 **进行中**

### UI 框架 🔄
- [x] 输入适配（InputModule）✅
  - [x] 鼠标/键盘/数值输入匹配 Blender 控制规范 ✅
  - [x] 快捷键映射表（OperationMappingManager）✅
  - [x] 快捷键上下文管理（ShortcutContext）✅
  - [x] 手势检测（Drag、Click、DoubleClick、Pan、Rotate、Zoom等）✅
  - [x] 操作事件和手势事件发布 ✅
  - [ ] 冲突检测 🔄 **待实现**
  - [ ] 手柄/触摸可选扩展 🔄 **待实现**
- [ ] UIRenderer 扩展 🔄 **待实现**
  - [ ] Widget 树结构与布局（水平/垂直/网格）
  - [ ] 屏幕与世界坐标转换统一入口
  - [ ] 主题/皮肤配置（颜色、字体、尺寸）
- [ ] Widget 组件库 🔄 **待实现**
  - [ ] Button/Toggle/Slider/ColorPicker
  - [ ] TreeView/PropertyPanel
  - [ ] Docking/浮动窗口管理

### UI 测试与示例 🔄 **待实现**
- [ ] UI Automation
  - [ ] 交互回放脚本
  - [ ] UI 状态断言/截图对比
- [ ] UI Performance Demo
  - [ ] Widget 批量渲染压力测试
  - [ ] 输入响应延迟测量

---

## 优化与质量保证 🔄 **部分完成**

### 性能优化 🔄
- [x] 渲染优化 ✅ **部分完成**
  - [x] RenderQueue 批处理系统 ✅
  - [x] 批处理阈值调优（Sprite/Text/Model）✅
  - [ ] RenderQueue 状态合并策略升级 🔄 **待优化**
  - [ ] GPU 缓冲重用与映射方式评估 🔄 **待实现**
- [x] 多线程与任务调度 ✅ **部分完成**
  - [x] 资源加载后台线程（AsyncResourceLoader）✅
  - [x] 资源加载后台线程性能测试 ✅
  - [ ] Render/Update 分离 🔄 **待实现**
  - [ ] Job System 原型（任务队列 + 线程池）🔄 **待实现**
- [x] 内存与资源 ✅ **部分完成**
  - [x] 纹理/网格内存统计（DebugHUDModule）✅
  - [x] 资源生命周期管理 ✅
  - [ ] 对象池策略（SpriteRenderable/TextRenderable）🔄 **待实现**
  - [ ] 资源生命周期可视化 🔄 **待实现**

### QA 与流程 ✅ **部分完成**
- [x] 自动化测试 ✅ **部分完成**
  - [x] 单元测试（SceneManager资源管理）✅
  - [x] 压力测试（场景切换、资源加载）✅
  - [x] 场景回归基准（多场景热切换测试）✅
  - [x] 事件总线测试 ✅
  - [x] 系统注册测试 ✅
  - [ ] 单元测试覆盖率报告 🔄 **待完善**
- [x] 文档与指南 ✅ **基本完成**
  - [x] 应用层开发进度汇报 ✅
  - [x] 模块开发指南 ✅
  - [x] 事件总线使用指南 ✅
  - [x] 工具链集成指南 ✅
  - [x] SceneGraph节点开发手册 ✅
  - [x] API文档（ApplicationHost、ModuleRegistry、AppContext、EventBus、SceneManager）✅
  - [ ] HUD/工具使用手册 🔄 **待完善（UI实现后）**
  - [ ] UI 组件开发指南 🔄 **待实现**
  - [ ] 优化实践案例 🔄 **待完善**
- [x] 发布准备 ✅ **部分完成**
  - [x] CMake 构建配置 ✅
  - [ ] CMake 构建预设（Debug/Release/Tools）🔄 **待完善**
  - [ ] 打包脚本（资源 + 配置）🔄 **待实现**
  - [ ] CI 流程基线（构建 + 基础测试）🔄 **待实现**

---

[上一章](PHASE1_BASIC_RENDERING.md) | [下一章](PHASE3_ADVANCED_FEATURES.md)

