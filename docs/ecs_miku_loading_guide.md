# ECS异步加载Miku模型指南

## 🎵 概述

33测试现在支持异步加载Miku模型（参考20测试），展示了ECS系统与异步资源加载的完美结合。

---

## 🔄 使用方式

### 方式1：加载Miku模型（默认）

```cpp
const bool USE_REAL_ASYNC_LOADING = true;  // 启用异步加载
const bool USE_MIKU_MODEL = true;          // 使用miku模型
```

**特点：**
- 加载 `models/miku/v4c5.0short.pmx`
- 单个实体，居中放置
- 自动缩放至0.1倍（miku模型很大）
- 相机调整到更高更远的位置

**运行：**
```bash
cmake --build build --config Release --target 33_ecs_async_test
./build/bin/Release/33_ecs_async_test.exe
```

### 方式2：加载简单Cube模型

```cpp
const bool USE_REAL_ASYNC_LOADING = true;
const bool USE_MIKU_MODEL = false;  // 改为false
```

**特点：**
- 加载 `models/cube.obj`
- 5个实体，圆形排列
- 标准缩放
- 标准相机位置

---

## 📊 配置对比

| 配置项 | Miku模型 | Cube模型 |
|-------|---------|---------|
| 文件路径 | `models/miku/v4c5.0short.pmx` | `models/cube.obj` |
| 实体数量 | 1 | 5 |
| 位置 | (0, 0, 0) | 圆形排列 |
| 缩放 | 0.1 | 1.0 |
| 相机位置 | (0, 10, 20) | (0, 2, 8) |
| 相机目标 | (0, 8, 0) | (0, 0, 0) |
| 近裁剪面 | 0.01 | 0.01 |

---

## 🎯 技术细节

### 相机设置

```cpp
if (USE_MIKU_MODEL) {
    // miku模型：相机看向模型中心，稍高的位置
    cameraTransform.SetPosition(Vector3(0, 10, 20));
    cameraTransform.LookAt(Vector3(0, 8, 0));
    
    camera->SetPerspective(60.0f, 1280.0f / 720.0f, 0.01f, 1000.0f);
} else {
    // 普通模型：标准相机位置
    cameraTransform.SetPosition(Vector3(0, 2, 8));
    cameraTransform.LookAt(Vector3(0, 0, 0));
}
```

**为什么近裁剪面是0.01？**
- Miku模型细节丰富
- 更小的近裁剪面提供更高的精度
- 避免模型近处被裁剪

### Transform设置

```cpp
if (USE_MIKU_MODEL) {
    transform.SetPosition(Vector3(0, 0, 0));  // 居中
    transform.SetScale(0.1f);                  // 缩小到10%
} else {
    // 圆形排列
    float angle = (float)i * (360.0f / 5.0f);
    float radius = 3.0f;
    float x = radius * std::cos(angle * 3.14159f / 180.0f);
    float z = radius * std::sin(angle * 3.14159f / 180.0f);
    transform.SetPosition(Vector3(x, 0, z));
}
```

**为什么缩放到0.1？**
- PMX模型通常使用mm作为单位
- 原始尺寸太大（约1700mm高）
- 缩小到合适的观察尺寸

### 异步加载流程

```
1. 设置meshName = "models/miku/v4c5.0short.pmx"
   ↓
2. ResourceLoadingSystem检测到需要加载
   ↓
3. 调用AsyncResourceLoader::LoadMeshAsync()
   ↓
4. 工作线程加载PMX文件（使用Assimp）
   ↓
5. 解析顶点、材质、纹理
   ↓
6. 回调通知加载完成（加入延迟队列）
   ↓
7. 主线程: ApplyPendingUpdates()
   ↓
8. GPU上传（Upload mesh data）
   ↓
9. 设置resourcesLoaded = true
   ↓
10. 开始渲染！
```

---

## 📝 预期日志输出

### 成功加载Miku模型

```
[INFO] [ECS Async Test] Will load Miku model
[INFO] [ECS Async Test] Using REAL async loading via ResourceLoadingSystem
[INFO] [ECS Async Test] Created 1 entities for async loading

[DEBUG] [ResourceLoadingSystem] Starting async load for mesh: models/miku/v4c5.0short.pmx
[INFO] ✅ 提交异步加载任务: models/miku/v4c5.0short.pmx (优先级: 0.000000)

[INFO] ========================================
[INFO] [Thread:xxxxx] 开始加载: models/miku/v4c5.0short.pmx
[INFO] ========================================
[INFO] ⭐ 工作线程：开始加载网格数据 models/miku/v4c5.0short.pmx (autoUpload=false)
[INFO] Loading model from file: models/miku/v4c5.0short.pmx (延迟上传)

... Assimp加载过程 ...

[INFO] ✅ 工作线程：网格数据加载完成（未上传）
[INFO] ========================================
[INFO] [Thread:xxxxx] 加载完成: models/miku/v4c5.0short.pmx
[INFO] ========================================

[INFO] ⭐ 主线程：开始上传网格到GPU: models/miku/v4c5.0short.pmx
[INFO] ✅ 主线程：网格上传完成: models/miku/v4c5.0short.pmx
[INFO] ✅ 资源上传完成: models/miku/v4c5.0short.pmx

[INFO] [ResourceLoadingSystem] Mesh applied successfully to entity 1

[INFO] 总任务数: 1
[INFO] 已完成: 1
[INFO] 失败: 0
```

---

## 🎨 渲染效果

### Miku模型渲染
- **位置**：场景中心
- **大小**：约17个单位高（原始170cm）
- **材质**：使用模型自带材质
- **动画**：通过SimpleRotationSystem自动旋转

### 相机视角
- **初始位置**：(0, 10, 20) - 稍高且远
- **注视点**：(0, 8, 0) - 模型头部附近
- **视野角**：60度
- **宽高比**：16:9

---

## ⚙️ 性能考虑

### 异步加载优势
1. **不阻塞主线程**
   - 文件I/O在工作线程
   - 主线程继续渲染

2. **GPU上传在主线程**
   - OpenGL上下文安全
   - 使用延迟队列机制

3. **线程安全保证**
   - weak_ptr生命周期管理
   - 多重安全检查
   - 关闭标志位保护

### PMX vs OBJ

| 格式 | 文件大小 | 加载时间 | 特点 |
|------|---------|---------|------|
| PMX | ~5-10MB | 1-2秒 | 材质、骨骼、物理 |
| OBJ | ~1-2MB | 0.1-0.5秒 | 简单几何 |

---

## 🐛 故障排查

### 问题1：模型加载失败

**错误：**
```
[ERROR] Assimp failed to load model: Unable to open file "models/miku/v4c5.0short.pmx"
```

**解决：**
- 检查文件是否存在：`ls models/miku/`
- 确认路径正确（相对于可执行文件）
- 检查文件权限

### 问题2：模型太大看不到

**症状：**
- 屏幕全黑或只有一片颜色

**解决：**
- 调整缩放：`transform.SetScale(0.05f);` 或更小
- 调整相机距离：`cameraTransform.SetPosition(Vector3(0, 10, 30));`

### 问题3：模型太小看不清

**解决：**
- 增大缩放：`transform.SetScale(0.2f);`
- 相机靠近：`cameraTransform.SetPosition(Vector3(0, 10, 15));`

---

## 🎓 扩展学习

### 添加更多模型

```cpp
std::vector<std::string> modelPaths = {
    "models/miku/v4c5.0short.pmx",
    "models/miku/v4c5.0.pmx",      // 长发版本
};

size_t entityCount = 2;  // 加载2个模型
```

### 调整旋转速度

修改 `SimpleRotationSystem`：

```cpp
// 在 33_ecs_async_test.cpp 第60行附近
float angle = totalTime * 20.0f + index * 72.0f;  // 从50改为20，旋转更慢
```

### 关闭自动旋转

注释掉旋转系统的注册：

```cpp
// world->RegisterSystem<SimpleRotationSystem>();
```

---

## 📖 参考

- **20_camera_test.cpp** - 原始miku加载实现
- **ecs_async_complete_guide.md** - 异步加载完整指南
- **models/miku/README.md** - Miku模型说明

---

**文档版本：** 1.0  
**创建日期：** 2025-11-04  
**状态：** ✅ 已测试，可用

