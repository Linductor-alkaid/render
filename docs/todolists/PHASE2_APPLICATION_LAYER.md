[返回文档首页](../README.md)

# Phase 2: 应用层与工具开发 TODO 列表

## 阶段目标

- [ ] 提供可扩展的应用层框架，支持场景管理、模块化系统和统一事件流转
- [ ] 打造可视化调试/配置工具链，加速内容生产与调试效率
- [ ] 建立基础复用 UI 组件库，遵循 Blender 风格交互约定
- [ ] 针对 Phase 1 功能栈进行性能与内存优化，强化产品级指标

---

## 应用层基础设施

### 场景与模块框架
- [ ] SceneGraph 扩展
  - [ ] 场景节点生命周期（OnEnter/OnExit/OnUpdate）
  - [ ] 热切换/多场景管理
  - [ ] 场景资源预加载与释放策略（依赖 ResourceManager）
- [ ] SystemRegistry
  - [ ] 系统注册/依赖排序
  - [ ] 帧循环钩子（PreFrame/PostFrame）
  - [ ] 热插拔调试接口
- [ ] 应用层事件总线
  - [ ] 输入、渲染、资源事件统一调度
  - [ ] 事件监听优先级/过滤
  - [ ] 与 UI/工具联动

### 示例与流程
- [ ] Simple Scene Demo
  - [ ] 多相机场景切换（透视/正交/Debug）
  - [ ] Mesh + Sprite + Text 融合演示
  - [ ] 基础光照与层级遮罩组合
- [ ] 2D UI Demo
  - [ ] HUD + 面板布局（参考 Blender UI）
  - [ ] 文本渲染与图标资源整合（通过 ResourceManager 加载）
  - [ ] 操作记录/交互日志输出
- [ ] 场景保存/加载
  - [ ] JSON/自定义格式
  - [ ] 资源路径校验
  - [ ] CMake 资源列表同步（新增文件需更新）

---

## 可视化工具链

### 调试与统计
- [ ] Renderer HUD
  - [ ] FPS/帧时间/Draw Call/批次数
  - [ ] 渲染层级可视化开关
  - [ ] Uniform/材质状态检查（依托 UniformManager）
- [ ] 性能分析面板
  - [ ] CPU/ GPU 时间轴（分阶段采样）
  - [ ] 资源使用视图（纹理/缓冲/内存）
  - [ ] 导出 CSV/JSON 报告
- [ ] 日志与诊断
  - [ ] Logger 级别过滤 UI
  - [ ] 运行时错误高亮与跳转
  - [ ] RenderDoc/调试工具集成指引

### 内容可视化工具
- [ ] Layer Mask 编辑器
  - [ ] 层级启用/禁用、遮罩匹配测试
  - [ ] 关联 Camera LayerMask 预览
- [ ] 材质/Shader 面板
  - [ ] 参数实时调整 → UniformManager 自动提交
  - [ ] Shader 热重载与错误提示
  - [ ] 资源依赖图谱（Texture/Mesh/Material）
- [ ] 动画调节器
  - [ ] SpriteAnimation 控制台（播放、速度、PingPong、事件回调）
  - [ ] 关键帧预览与事件编辑

---

## 基础复用 UI 开发

### UI 框架
- [ ] UIRenderer 扩展
  - [ ] Widget 树结构与布局（水平/垂直/网格）
  - [ ] 屏幕与世界坐标转换统一入口
  - [ ] 主题/皮肤配置（颜色、字体、尺寸）
- [ ] 输入适配
  - [ ] 鼠标/键盘/数值输入匹配 Blender 控制规范
  - [ ] 快捷键映射表与冲突检测
  - [ ] 手柄/触摸可选扩展
- [ ] Widget 组件库
  - [ ] Button/Toggle/Slider/ColorPicker
  - [ ] TreeView/PropertyPanel
  - [ ] Docking/浮动窗口管理

### UI 测试与示例
- [ ] UI Automation
  - [ ] 交互回放脚本
  - [ ] UI 状态断言/截图对比
- [ ] UI Performance Demo
  - [ ] Widget 批量渲染压力测试
  - [ ] 输入响应延迟测量

---

## 优化与质量保证

### 性能优化
- [ ] 渲染优化
  - [ ] RenderQueue 状态合并策略升级
  - [ ] 批处理阈值调优（Sprite/Text/Model）
  - [ ] GPU 缓冲重用与映射方式评估
- [ ] 多线程与任务调度
  - [ ] Render/Update 分离
  - [ ] 资源加载后台线程性能测试
  - [ ] Job System 原型（任务队列 + 线程池）
- [ ] 内存与资源
  - [ ] 纹理/网格内存统计
  - [ ] 对象池策略（SpriteRenderable/TextRenderable）
  - [ ] 资源生命周期可视化

### QA 与流程
- [ ] 自动化测试
  - [ ] 单元测试覆盖率报告
  - [ ] 场景回归基准（帧率/内存快照）
  - [ ] 事件总线/系统注册回归测试
- [ ] 文档与指南
  - [ ] HUD/工具使用手册
  - [ ] UI 组件开发指南
  - [ ] 优化实践案例
- [ ] 发布准备
  - [ ] CMake 构建预设（Debug/Release/Tools）
  - [ ] 打包脚本（资源 + 配置）
  - [ ] CI 流程基线（构建 + 基础测试）

---

[上一章](PHASE1_BASIC_RENDERING.md) | [下一章](PHASE3_ADVANCED_FEATURES.md)

