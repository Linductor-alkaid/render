# ECS异步加载崩溃诊断指南

## 请提供以下信息以准确定位问题

### 1. 崩溃时机
- [ ] 程序启动时立即崩溃
- [ ] 运行一段时间后崩溃
- [ ] 退出时崩溃
- [ ] 不定时崩溃

### 2. 崩溃位置
请提供崩溃时的调用栈信息，或者最后的日志输出。

### 3. 错误信息
请提供具体的错误信息（如访问违例、段错误等）。

---

## 可能的问题点分析

### 问题1：异步加载代码未被注释
检查 `33_ecs_async_test.cpp` 第200-226行的代码是否被注释：

```cpp
// 如果要测试真正的异步加载，取消下面的注释：
/*
g_loadingTotal = entities.size();
...
*/
```

**如果这段代码没有被注释**，会触发真正的异步加载，此时需要确保：
- meshName设置正确
- 文件路径存在
- resourcesLoaded标志正确设置

### 问题2：World不是通过shared_ptr创建
检查是否有其他地方创建了World但没有使用shared_ptr。

已知位置：
- ✅ `33_ecs_async_test.cpp` - 已修改为shared_ptr
- ⚠️ `31_ecs_basic_test.cpp` - 未修改（但不使用异步加载）
- ⚠️ `32_ecs_renderer_test.cpp` - 未修改（但不使用异步加载）

### 问题3：ResourceManager线程安全问题
在 `systems.cpp` 第138-145行：

```cpp
if (!meshComp.materialName.empty() && !meshComp.material) {
    auto& resMgr = ResourceManager::GetInstance();
    meshComp.material = resMgr.GetMaterial(meshComp.materialName);
}
```

这段代码在主线程的Update中执行，但ResourceManager可能不是线程安全的。

### 问题4：实体ID的有效性
Entity可能在回调执行前被销毁，需要额外验证。

---

## 推荐的调试步骤

### 步骤1：添加详细日志
修改回调函数，添加更详细的日志：

```cpp
[this, entityCopy, worldWeak](const MeshLoadResult& result) {
    Logger::GetInstance().InfoFormat("Callback triggered for entity %u", entityCopy.index);
    
    if (auto worldShared = worldWeak.lock()) {
        Logger::GetInstance().Info("World is alive");
        if (!m_shuttingDown.load()) {
            Logger::GetInstance().Info("System not shutting down, processing callback");
            this->OnMeshLoaded(entityCopy, result);
        }
    } else {
        Logger::GetInstance().Info("World is dead, skip callback");
    }
}
```

### 步骤2：使用调试器
运行时使用调试器，设置断点在：
- `ResourceLoadingSystem::OnMeshLoaded`
- `ResourceLoadingSystem::OnDestroy`
- `AsyncResourceLoader::ProcessCompletedTasks`

### 步骤3：使用内存检测工具
- Windows: Dr. Memory 或 Application Verifier
- Linux: valgrind --tool=memcheck
- 编译时启用AddressSanitizer: `-fsanitize=address`

---

## 立即可尝试的修复

### 修复A：完全禁用异步加载测试
在33测试中，确保异步加载的代码被注释，只使用同步创建的Cube。

### 修复B：移除ResourceManager的同步访问
注释掉 `systems.cpp` 第138-145行的材质加载代码，在实体创建时直接设置material。

### 修复C：增强Entity有效性检查
```cpp
void ResourceLoadingSystem::ApplyPendingUpdates() {
    // ... 获取更新队列 ...
    
    for (const auto& update : meshUpdates) {
        // 多重检查
        if (!m_world->IsValidEntity(update.entity)) {
            Logger::GetInstance().Warning("Entity is not valid");
            continue;
        }
        
        if (!m_world->HasComponent<MeshRenderComponent>(update.entity)) {
            Logger::GetInstance().Warning("Entity missing MeshRenderComponent");
            continue;
        }
        
        // 再次检查是否关闭
        if (m_shuttingDown.load()) {
            Logger::GetInstance().Warning("System shutting down during update");
            break;
        }
        
        // 现在才安全访问组件
        auto& meshComp = m_world->GetComponent<MeshRenderComponent>(update.entity);
        // ...
    }
}
```

