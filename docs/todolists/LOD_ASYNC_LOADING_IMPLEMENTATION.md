# LOD异步加载实现方案

## 概述

本方案旨在为LOD系统添加完整的异步加载支持，包括：
- 异步文件加载（使用AsyncResourceLoader）
- 异步LOD生成（使用TaskScheduler在工作线程执行）
- 统一的异步接口和任务管理
- 进度回调和错误处理
- 向后兼容的同步接口

## 设计目标

1. **性能优化**: 将CPU密集型操作移到工作线程，避免阻塞主线程
2. **用户体验**: 提供加载进度反馈，支持流式加载
3. **灵活性**: 支持同步和异步两种模式，按需选择
4. **可扩展性**: 易于扩展支持更多异步操作
5. **向后兼容**: 保持现有同步接口不变

## 架构设计

### 1. 复用现有基础设施

**核心原则**: 最大化复用 `AsyncResourceLoader` 和 `TaskScheduler`，不创建重复的任务管理系统。

**文件加载**: 直接使用 `AsyncResourceLoader::LoadMeshAsync`
- 返回 `std::shared_ptr<MeshLoadTask>`
- 自动处理工作线程加载和GPU上传
- 已有完整的状态管理和回调机制

**LOD生成**: 使用 `TaskScheduler::SubmitLambda` + 自定义上传逻辑
- 在工作线程执行网格简化
- 在主线程回调中处理GPU上传
- 复用 `AsyncResourceLoader` 的上传队列机制（可选）

### 2. 轻量级包装器

创建一个简单的 `LODLoadTask` 包装器，内部管理：
- 多个 `MeshLoadTask`（文件加载）
- 多个 `TaskHandle`（LOD生成）
- 统一的进度和状态管理
- 统一的回调接口

```
LODLoadTask (轻量级包装器)
├── 管理多个 MeshLoadTask (复用 AsyncResourceLoader)
└── 管理多个 TaskHandle (复用 TaskScheduler)
```

### 3. 异步接口设计

```cpp
// 异步加载接口
std::shared_ptr<LODLoadTask> LoadLODConfigAsync(
    Ref<Mesh> baseMesh,
    const LODLoadOptions& options,
    std::function<void(const LODLoadResult&)> callback = nullptr
);

// 同步接口（保持向后兼容）
LODConfig LoadLODConfig(
    Ref<Mesh> baseMesh,
    const LODLoadOptions& options
);
```

### 4. 工作流程

**文件加载流程**（完全复用AsyncResourceLoader）:
```
主线程 -> AsyncResourceLoader::LoadMeshAsync 
-> TaskScheduler (工作线程: 文件I/O) 
-> AsyncResourceLoader完成队列 
-> 主线程(ProcessCompletedTasks: GPU上传) 
-> MeshLoadTask回调
-> LODLoadTask聚合回调
```

**LOD生成流程**（复用TaskScheduler + 自定义上传）:
```
主线程 -> TaskScheduler::SubmitLambda 
-> 工作线程(网格简化) 
-> TaskHandle完成 
-> 主线程回调(GPU上传) 
-> LODLoadTask聚合回调
```

## 实现任务清单

### 阶段1: 核心数据结构 (优先级: 高)

#### 1.1 创建LOD加载结果和轻量级任务包装器
- [ ] **文件**: `include/render/lod_loader.h`
- [ ] **任务**: 定义 `LODLoadResult` 和 `LODLoadTask` 包装器（复用现有任务系统）
- [ ] **内容**:
  ```cpp
  /**
   * @brief LOD加载结果
   */
  struct LODLoadResult {
      LODConfig config;
      std::string name;
      LoadStatus status;
      std::string errorMessage;
      LoadErrorType errorType = LoadErrorType::None;
      float progress = 0.0f;  // 0.0-1.0
      
      bool IsSuccess() const {
          return status == LoadStatus::Completed && config.enabled;
      }
  };
  
  /**
   * @brief LOD加载任务包装器
   * 
   * 轻量级包装器，内部复用 AsyncResourceLoader 和 TaskScheduler
   * 不重复实现任务管理，只负责聚合多个子任务的状态
   */
  class LODLoadTask {
  public:
      std::string name;
      std::atomic<LoadStatus> status{LoadStatus::Pending};
      std::atomic<float> progress{0.0f};
      std::string errorMessage;
      LoadErrorType errorType = LoadErrorType::None;
      
      std::function<void(const LODLoadResult&)> callback;
      
      LODConfig result;
      
      /**
       * @brief 取消所有子任务
       */
      void Cancel();
      
      /**
       * @brief 检查是否完成
       */
      bool IsCompleted() const;
      
      /**
       * @brief 获取当前状态
       */
      LoadStatus GetStatus() const { return status.load(); }
      
  private:
      // 文件加载任务（复用 AsyncResourceLoader）
      std::vector<std::shared_ptr<MeshLoadTask>> m_meshTasks;
      
      // LOD生成任务（复用 TaskScheduler）
      std::vector<std::shared_ptr<TaskHandle>> m_generateTasks;
      
      // 内部状态
      std::atomic<size_t> m_completedCount{0};
      size_t m_totalCount = 0;
      std::mutex m_mutex;  // 保护内部状态
      
      // 内部方法
      void OnSubTaskCompleted();
      void CheckCompletion();
      void NotifyCompletion();
  };
  ```
- [ ] **依赖**: `AsyncResourceLoader`, `TaskScheduler`
- [ ] **测试**: 创建基本任务对象并验证状态管理

#### 1.2 扩展LODLoadOptions
- [ ] **文件**: `include/render/lod_loader.h`
- [ ] **任务**: 完善异步加载选项
- [ ] **内容**:
  ```cpp
  struct LoadStrategy {
      bool preloadAllLODs = true;
      
      /**
       * @brief 是否异步加载
       * 
       * 如果为 true，使用异步加载器或TaskScheduler
       * 如果为 false，使用同步加载（向后兼容）
       */
      bool asyncLoad = false;
      
      /**
       * @brief 异步加载时的优先级
       * 
       * 0.0-1.0，越高越优先
       * 默认: 0.5（中等优先级）
       */
      float asyncPriority = 0.5f;
      
      /**
       * @brief 是否在后台生成LOD（仅异步模式）
       * 
       * 如果为 true，LOD生成在工作线程执行
       * 如果为 false，即使异步模式也同步生成（不推荐）
       */
      bool backgroundGeneration = true;
      
      bool fallbackToGenerate = false;
  };
  ```
- [ ] **依赖**: 无
- [ ] **测试**: 验证选项正确传递

### 阶段2: 异步文件加载 (优先级: 高)

#### 2.1 复用AsyncResourceLoader进行文件加载
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 直接使用 `AsyncResourceLoader::LoadMeshAsync`，无需额外封装
- [ ] **内容**:
  ```cpp
  /**
   * @brief 异步加载单个LOD级别的网格（复用AsyncResourceLoader）
   * 
   * 直接使用 AsyncResourceLoader，无需额外封装
   * 返回的 MeshLoadTask 已经包含完整的状态管理和回调机制
   */
  std::shared_ptr<MeshLoadTask> LoadSingleLODMeshAsync(
      const std::string& filepath,
      int lodLevel,
      const std::string& name,
      float priority
  ) {
      if (!AsyncResourceLoader::GetInstance().IsInitialized()) {
          LOG_WARNING("AsyncResourceLoader not initialized, falling back to sync");
          // 回退到同步加载（见2.2）
          return nullptr;  // 调用者需要处理同步回退
      }
      
      std::string resourceName = name.empty() 
          ? filepath + "_lod" + std::to_string(lodLevel)
          : name;
      
      // 直接复用 AsyncResourceLoader
      return AsyncResourceLoader::GetInstance().LoadMeshAsync(
          filepath,
          resourceName,
          nullptr,  // 回调由LODLoadTask统一处理
          priority
      );
  }
  ```
- [ ] **依赖**: 1.1, 1.2
- [ ] **优势**: 
  - 完全复用现有基础设施
  - 无需重复实现任务管理
  - 自动获得重试、错误处理等功能
- [ ] **测试**: 
  - 测试异步加载单个LOD文件
  - 测试回退到同步加载
  - 验证任务状态正确传递

#### 2.2 实现批量异步文件加载（复用MeshLoadTask）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 在 `LODLoadTask` 中聚合多个 `MeshLoadTask`
- [ ] **内容**:
  ```cpp
  /**
   * @brief 启动文件加载任务
   * 
   * 复用 AsyncResourceLoader，为每个LOD级别创建 MeshLoadTask
   */
  void LODLoadTask::StartFileLoading(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options
  ) {
      m_totalCount = 4;  // LOD0-LOD3
      result.lodMeshes.resize(4);
      
      // LOD0: 使用提供的baseMesh或加载
      if (baseMesh) {
          result.lodMeshes[0] = baseMesh;
          m_completedCount++;
      } else {
          // 异步加载LOD0
          auto task = LoadSingleLODMeshAsync(
              BuildLODFilePath(options.basePath, 0, ...),
              0, name, priority
          );
          if (task) {
              m_meshTasks.push_back(task);
              SetupMeshTaskCallback(task, 0);
          }
      }
      
      // LOD1-LOD3: 异步加载
      for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
          auto task = LoadSingleLODMeshAsync(
              BuildLODFilePath(options.basePath, lodLevel, ...),
              lodLevel, name, GetLODPriority(lodLevel) * priority
          );
          if (task) {
              m_meshTasks.push_back(task);
              SetupMeshTaskCallback(task, lodLevel);
          }
      }
      
      UpdateProgress();
  }
  
  /**
   * @brief 设置MeshLoadTask的回调（复用现有回调机制）
   */
  void LODLoadTask::SetupMeshTaskCallback(
      std::shared_ptr<MeshLoadTask> task,
      int lodLevel
  ) {
      // 保存原始回调
      auto originalCallback = task->callback;
      
      // 设置新的回调，聚合到LODLoadTask
      task->callback = [this, lodLevel, originalCallback](const MeshLoadResult& result) {
          // 调用原始回调（如果存在）
          if (originalCallback) {
              originalCallback(result);
          }
          
          // 处理结果
          if (result.IsSuccess()) {
              result.lodMeshes[lodLevel] = result.resource;
          }
          
          // 更新进度
          m_completedCount++;
          UpdateProgress();
          
          // 检查是否全部完成
          CheckCompletion();
      };
  }
  ```
- [ ] **依赖**: 2.1
- [ ] **优势**:
  - 完全复用 `MeshLoadTask` 的回调和状态管理
  - 无需重复实现任务队列、重试等机制
  - 自动获得 `AsyncResourceLoader` 的所有功能
- [ ] **测试**:
  - 测试批量加载所有LOD级别
  - 测试部分失败的情况
  - 测试进度更新
  - 验证回调正确执行

### 阶段3: 异步LOD生成 (优先级: 高)

#### 3.1 复用TaskScheduler进行LOD生成
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 直接使用 `TaskScheduler::SubmitLambda`，无需额外封装
- [ ] **内容**:
  ```cpp
  /**
   * @brief 异步生成单个LOD级别（复用TaskScheduler）
   * 
   * 直接使用 TaskScheduler，在工作线程执行网格简化
   * 返回的 TaskHandle 可以查询状态和等待完成
   */
  std::shared_ptr<TaskHandle> GenerateLODLevelAsync(
      Ref<Mesh> baseMesh,
      int lodLevel,
      const LODGenerator::SimplifyOptions& options
  ) {
      if (!TaskScheduler::GetInstance().IsInitialized()) {
          LOG_WARNING("TaskScheduler not initialized, falling back to sync");
          return nullptr;  // 调用者需要处理同步回退
      }
      
      // 直接复用 TaskScheduler
      // 注意：需要确保baseMesh的顶点数据在工作线程中可访问
      // 方案：LODGenerator内部会处理数据拷贝
      return TaskScheduler::GetInstance().SubmitLambda(
          [baseMesh, lodLevel, options]() -> Ref<Mesh> {
              // 在工作线程中执行网格简化
              return LODGenerator::GenerateLODLevel(baseMesh, lodLevel, options);
          },
          TaskPriority::Normal,  // 可以根据lodLevel调整优先级
          ("LOD_Generate_L" + std::to_string(lodLevel)).c_str()
      );
  }
  ```
- [ ] **依赖**: 1.1, 1.2
- [ ] **优势**:
  - 完全复用 `TaskScheduler` 的线程池和任务管理
  - 无需重复实现工作线程管理
  - 自动获得优先级调度、任务等待等功能
- [ ] **注意事项**: 
  - 确保Mesh数据在工作线程中安全访问（LODGenerator内部处理）
  - GPU上传需要在主线程回调中执行
- [ ] **测试**:
  - 测试异步生成单个LOD级别
  - 测试大网格的生成性能
  - 测试错误处理
  - 验证任务状态正确传递

#### 3.2 实现批量异步LOD生成（复用TaskHandle）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 在 `LODLoadTask` 中聚合多个 `TaskHandle`，处理GPU上传
- [ ] **内容**:
  ```cpp
  /**
   * @brief 启动LOD生成任务
   * 
   * 复用 TaskScheduler，为每个LOD级别创建生成任务
   * 注意：需要在主线程回调中处理GPU上传
   */
  void LODLoadTask::StartLODGeneration(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options
  ) {
      m_totalCount = 3;  // LOD1-LOD3（LOD0使用baseMesh）
      result.lodMeshes.resize(4);
      result.lodMeshes[0] = baseMesh;  // LOD0使用原始网格
      m_completedCount++;  // LOD0立即完成
      
      // 提交LOD1-LOD3的生成任务（复用TaskScheduler）
      for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
          auto handle = GenerateLODLevelAsync(
              baseMesh,
              lodLevel,
              options.simplifyOptions
          );
          
          if (handle) {
              m_generateTasks.push_back(handle);
              SetupGenerateTaskCallback(handle, lodLevel);
          } else {
              // 回退到同步生成
              result.lodMeshes[lodLevel] = LODGenerator::GenerateLODLevel(
                  baseMesh, lodLevel, options.simplifyOptions
              );
              m_completedCount++;
          }
      }
      
      UpdateProgress();
  }
  
  /**
   * @brief 设置生成任务的回调（处理GPU上传）
   * 
   * 方案：使用TaskScheduler的SubmitLambda，在生成任务中直接处理结果
   * 或者：在主线程定期轮询TaskHandle状态（在Update中调用）
   */
  void LODLoadTask::SetupGenerateTaskCallback(
      std::shared_ptr<TaskHandle> handle,
      int lodLevel
  ) {
      // 方案1（推荐）: 在生成任务中直接处理结果
      // 修改GenerateLODLevelAsync，在任务完成后立即处理
      // 但需要注意GPU上传必须在主线程
      
      // 方案2: 使用包装任务
      // 创建一个后续任务，在生成完成后执行GPU上传和回调
      auto uploadTask = TaskScheduler::GetInstance().SubmitLambda(
          [this, handle, lodLevel]() {
              // 等待生成任务完成
              handle->Wait();
              
              // 获取生成结果（需要从共享位置获取）
              // 注意：这里仍然在工作线程，不能直接上传GPU
              // 需要将结果传递到主线程
          },
          TaskPriority::Normal,
          ("LOD_Upload_L" + std::to_string(lodLevel)).c_str()
      );
      
      // 方案3（最简单）: 在主线程定期检查
      // 在LODLoadTask的Update方法中轮询所有TaskHandle
      // 当发现完成时，在主线程执行GPU上传
  }
  
  /**
   * @brief 更新任务状态（在主线程调用）
   * 
   * 轮询所有TaskHandle，检查生成任务是否完成
   * 在主线程中执行GPU上传
   */
  void LODLoadTask::Update() {
      if (status.load() != LoadStatus::Pending && 
          status.load() != LoadStatus::Loading) {
          return;  // 已完成或失败
      }
      
      // 检查所有生成任务
      for (size_t i = 0; i < m_generateTasks.size(); ++i) {
          auto& handle = m_generateTasks[i];
          if (handle && handle->IsCompleted()) {
              // 任务完成，获取结果并上传GPU
              // 注意：需要从共享位置获取生成的Mesh
              // 这里需要修改GenerateLODLevelAsync来存储结果
              int lodLevel = static_cast<int>(i + 1);  // LOD1-3
              ProcessGeneratedMesh(lodLevel);
          }
      }
      
      // 检查所有文件加载任务（通过MeshLoadTask的状态）
      // MeshLoadTask的状态由AsyncResourceLoader管理，无需轮询
      // 回调会自动触发
  }
  
  /**
   * @brief 处理生成的Mesh（在主线程执行GPU上传）
   */
  void LODLoadTask::OnLODGenerated(int lodLevel, Ref<Mesh> mesh) {
      if (!mesh) {
          // 生成失败
          m_completedCount++;
          UpdateProgress();
          CheckCompletion();
          return;
      }
      
      // GPU上传（必须在主线程）
      if (!mesh->IsUploaded()) {
          mesh->Upload();
      }
      
      result.lodMeshes[lodLevel] = mesh;
      m_completedCount++;
      UpdateProgress();
      CheckCompletion();
  }
  ```
- [ ] **依赖**: 3.1
- [ ] **注意事项**:
  - `TaskHandle` 没有内置回调机制，需要轮询或扩展
  - GPU上传必须在主线程执行
  - 可以考虑扩展 `TaskScheduler` 支持回调（可选优化）
- [ ] **测试**:
  - 测试批量生成所有LOD级别
  - 测试并行生成性能
  - 测试进度更新
  - 验证GPU上传正确执行

### 阶段4: 统一异步接口 (优先级: 高)

#### 4.1 实现LoadLODConfigAsync主接口（统一复用现有系统）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 实现统一的异步加载接口，内部复用AsyncResourceLoader和TaskScheduler
- [ ] **内容**:
  ```cpp
  /**
   * @brief 异步加载LOD配置（统一接口）
   * 
   * 内部根据选项复用 AsyncResourceLoader 或 TaskScheduler
   * 不创建新的任务管理系统，只做轻量级聚合
   */
  std::shared_ptr<LODLoadTask> LoadLODConfigAsync(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options,
      std::function<void(const LODLoadResult&)> callback
  ) {
      auto task = std::make_shared<LODLoadTask>();
      task->name = options.basePath.empty() ? "LOD_Config" : options.basePath;
      task->callback = callback;
      
      // 设置距离阈值
      if (!options.distanceThresholds.empty()) {
          task->result.distanceThresholds = options.distanceThresholds;
      }
      
      // 根据选项选择加载方式
      if (options.autoGenerateLOD) {
          // 自动生成模式
          if (options.loadStrategy.asyncLoad && options.loadStrategy.backgroundGeneration) {
              // 异步生成（复用TaskScheduler）
              task->StartLODGeneration(baseMesh, options);
          } else {
              // 同步生成（向后兼容）
              task->result = GenerateLODMeshesSync(baseMesh, options);
              task->status = LoadStatus::Completed;
              task->NotifyCompletion();
          }
      } else {
          // 从文件加载模式
          if (options.loadStrategy.asyncLoad) {
              // 异步加载（复用AsyncResourceLoader）
              task->StartFileLoading(baseMesh, options);
          } else {
              // 同步加载（向后兼容）
              task->result = LoadLODMeshesFromFilesSync(baseMesh, options);
              task->status = LoadStatus::Completed;
              task->NotifyCompletion();
          }
      }
      
      return task;
  }
  ```
- [ ] **依赖**: 2.2, 3.2
- [ ] **优势**:
  - 完全复用现有基础设施
  - 轻量级包装，不重复实现任务管理
  - 统一的接口，内部自动选择最佳实现
- [ ] **测试**:
  - 测试各种选项组合
  - 测试回退到同步模式
  - 测试回调执行
  - 验证任务状态正确管理

#### 4.2 实现混合模式（文件+生成）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 支持文件优先，不存在则生成的混合模式
- [ ] **内容**:
  ```cpp
  class LODHybridLoadTask : public LODLoadTask {
  private:
      // 先尝试加载文件，失败则生成
      void LoadOrGenerate(int lodLevel) {
          // 1. 先尝试异步加载文件
          auto fileTask = LoadSingleLODMeshAsync(...);
          
          fileTask->callback = [this, lodLevel](const MeshLoadResult& result) {
              if (result.IsSuccess()) {
                  lodMeshes[lodLevel] = result.resource;
                  OnLODReady(lodLevel);
              } else {
                  // 文件加载失败，异步生成
                  GenerateLODLevelAsync(...);
              }
          };
      }
  };
  ```
- [ ] **依赖**: 2.1, 3.1
- [ ] **测试**:
  - 测试文件存在的情况
  - 测试文件不存在回退到生成
  - 测试部分文件存在的情况

### 阶段4.5: TaskHandle结果处理优化 - 使用ECS组件变化事件 (优先级: 高)

#### 4.5.1 创建LODLoadingComponent组件
- [ ] **文件**: `include/render/ecs/components.h`
- [ ] **任务**: 创建LOD加载状态组件，用于在ECS中跟踪加载进度
- [ ] **内容**:
  ```cpp
  /**
   * @brief LOD加载状态组件
   * 
   * 用于在ECS中跟踪LOD异步加载的进度和状态
   * 当组件更新时，会触发ComponentChangeEvent，系统可以响应
   */
  struct LODLoadingComponent {
      /**
       * @brief 加载状态
       */
      enum class LoadingState {
          Pending,        // 等待开始
          Loading,        // 加载中
          Completed,      // 完成
          Failed          // 失败
      };
      
      LoadingState state = LoadingState::Pending;
      
      /**
       * @brief 加载进度 (0.0-1.0)
       */
      float progress = 0.0f;
      
      /**
       * @brief LOD配置（加载完成后填充）
       */
      LODConfig config;
      
      /**
       * @brief 错误消息（如果失败）
       */
      std::string errorMessage;
      
      /**
       * @brief 加载任务句柄（用于取消等操作）
       */
      std::shared_ptr<LODLoadTask> task;
      
      /**
       * @brief 已完成的LOD级别数量
       */
      size_t completedLevels = 0;
      
      /**
       * @brief 总LOD级别数量
       */
      size_t totalLevels = 4;  // LOD0-LOD3
      
      /**
       * @brief 更新进度
       */
      void UpdateProgress(float newProgress) {
          progress = newProgress;
          // 组件变化会触发ComponentChangeEvent
      }
      
      /**
       * @brief 标记为完成
       */
      void MarkCompleted(const LODConfig& lodConfig) {
          state = LoadingState::Completed;
          progress = 1.0f;
          config = lodConfig;
          completedLevels = totalLevels;
          // 组件变化会触发ComponentChangeEvent
      }
      
      /**
       * @brief 标记为失败
       */
      void MarkFailed(const std::string& error) {
          state = LoadingState::Failed;
          errorMessage = error;
          // 组件变化会触发ComponentChangeEvent
      }
  };
  ```
- [ ] **依赖**: 无
- [ ] **测试**: 创建组件并验证状态更新

#### 4.5.2 使用ECS组件变化事件处理结果
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 将LOD加载结果更新到ECS组件，利用组件变化事件通知系统
- [ ] **内容**:
  ```cpp
  /**
   * @brief 异步加载LOD配置（ECS集成版本）
   * 
   * 如果提供了ECS实体，会将加载状态注册到LODLoadingComponent
   * 系统可以通过组件变化事件监听加载完成
   */
  std::shared_ptr<LODLoadTask> LoadLODConfigAsync(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options,
      std::function<void(const LODLoadResult&)> callback,
      ECS::World* world = nullptr,
      ECS::EntityID entity = ECS::INVALID_ENTITY_ID
  ) {
      auto task = std::make_shared<LODLoadTask>();
      task->name = options.basePath.empty() ? "LOD_Config" : options.basePath;
      task->callback = callback;
      
      // 如果提供了ECS实体，注册到组件
      if (world && entity != ECS::INVALID_ENTITY_ID) {
          // 确保组件已注册
          if (!world->HasComponent<LODLoadingComponent>(entity)) {
              world->AddComponent<LODLoadingComponent>(entity);
          }
          
          auto& loadingComp = world->GetComponent<LODLoadingComponent>(entity);
          loadingComp.state = LODLoadingComponent::LoadingState::Loading;
          loadingComp.task = task;
          
          // 设置回调，更新组件状态
          task->callback = [world, entity, originalCallback = callback](
              const LODLoadResult& result
          ) {
              // 更新ECS组件
              if (world && entity != ECS::INVALID_ENTITY_ID) {
                  auto& loadingComp = world->GetComponent<LODLoadingComponent>(entity);
                  
                  if (result.IsSuccess()) {
                      loadingComp.MarkCompleted(result.config);
                  } else {
                      loadingComp.MarkFailed(result.errorMessage);
                  }
                  
                  // 组件变化会自动触发ComponentChangeEvent
                  // 系统可以通过RegisterComponentChangeCallback监听
              }
              
              // 调用原始回调
              if (originalCallback) {
                  originalCallback(result);
              }
          };
      }
      
      // 启动加载任务...
      return task;
  }
  ```
- [ ] **依赖**: 4.5.1
- [ ] **优势**:
  - 利用ECS的组件变化事件系统
  - 无需轮询，事件驱动
  - 与ECS系统无缝集成
  - 支持多个系统监听同一加载任务
- [ ] **测试**: 
  - 测试组件状态更新
  - 测试组件变化事件触发
  - 测试系统响应事件

#### 4.5.3 创建LOD加载系统（响应组件变化）
- [ ] **文件**: `src/rendering/lod_loading_system.cpp`, `include/render/ecs/systems/lod_loading_system.h`
- [ ] **任务**: 创建系统，监听LODLoadingComponent变化，处理GPU上传等
- [ ] **内容**:
  ```cpp
  /**
   * @brief LOD加载系统
   * 
   * 监听LODLoadingComponent的变化，处理加载完成后的GPU上传等操作
   */
  class LODLoadingSystem : public System {
  public:
      LODLoadingSystem(Renderer* renderer) : System(renderer) {}
      
      void OnRegister(World& world) override {
          // 注册组件变化回调
          m_callbackId = world.GetComponentRegistry().RegisterComponentChangeCallback<LODLoadingComponent>(
              [this](ECS::EntityID entity, const LODLoadingComponent& comp) {
                  OnLODLoadingChanged(entity, comp);
              }
          );
      }
      
      void OnUnregister(World& world) override {
          // 取消注册回调
          if (m_callbackId != 0) {
              world.GetComponentRegistry().UnregisterComponentChangeCallback(m_callbackId);
              m_callbackId = 0;
          }
      }
      
  private:
      void OnLODLoadingChanged(ECS::EntityID entity, const LODLoadingComponent& comp) {
          if (comp.state == LODLoadingComponent::LoadingState::Completed) {
              // LOD加载完成，执行GPU上传
              ProcessLODConfigUpload(entity, comp.config);
              
              // 如果实体有LODComponent，更新它
              if (m_world->HasComponent<LODComponent>(entity)) {
                  auto& lodComp = m_world->GetComponent<LODComponent>(entity);
                  lodComp.config = comp.config;
                  // LODComponent变化也会触发事件，其他系统可以响应
              }
          } else if (comp.state == LODLoadingComponent::LoadingState::Failed) {
              // 处理加载失败
              LOG_ERROR_F("LOD loading failed for entity %u: %s", 
                          entity.index, comp.errorMessage.c_str());
          }
      }
      
      void ProcessLODConfigUpload(ECS::EntityID entity, const LODConfig& config) {
          // 确保所有Mesh都已上传到GPU
          for (auto& mesh : config.lodMeshes) {
              if (mesh && !mesh->IsUploaded()) {
                  mesh->Upload();
              }
          }
      }
      
      uint64_t m_callbackId = 0;
  };
  ```
- [ ] **依赖**: 4.5.2
- [ ] **优势**:
  - 事件驱动，无需轮询
  - 自动处理GPU上传
  - 可以轻松扩展其他处理逻辑
  - 符合ECS架构模式
- [ ] **测试**:
  - 测试系统注册和注销
  - 测试组件变化响应
  - 测试GPU上传执行

#### 4.5.4 优化生成任务的结果传递（ECS版本）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 使用ECS组件存储生成结果，避免共享存储
- [ ] **内容**:
  ```cpp
  /**
   * @brief 异步生成LOD级别（ECS集成版本）
   * 
   * 生成结果直接更新到ECS组件的中间状态
   * 完成后触发组件变化事件
   */
  void LODLoadTask::StartLODGenerationWithECS(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options,
      ECS::World* world,
      ECS::EntityID entity
  ) {
      if (!world || entity == ECS::INVALID_ENTITY_ID) {
          // 回退到非ECS版本
          StartLODGeneration(baseMesh, options);
          return;
      }
      
      auto& loadingComp = world->GetComponent<LODLoadingComponent>(entity);
      loadingComp.result.lodMeshes.resize(4);
      loadingComp.result.lodMeshes[0] = baseMesh;
      
      // 为每个LOD级别创建生成任务
      for (int lodLevel = 1; lodLevel <= 3; ++lodLevel) {
          auto handle = TaskScheduler::GetInstance().SubmitLambda(
              [world, entity, baseMesh, lodLevel, options]() {
                  // 在工作线程中生成LOD
                  auto mesh = LODGenerator::GenerateLODLevel(
                      baseMesh, lodLevel, options.simplifyOptions
                  );
                  
                  // 更新到ECS组件（需要线程安全）
                  if (world && entity != ECS::INVALID_ENTITY_ID) {
                      // 方案：使用World的线程安全接口更新组件
                      // 或者：将结果存储到临时位置，在主线程更新
                      StoreGeneratedMesh(world, entity, lodLevel, mesh);
                  }
                  
                  return mesh;
              },
              TaskPriority::Normal,
              ("LOD_Generate_L" + std::to_string(lodLevel)).c_str()
          );
          
          m_generateTasks.push_back(handle);
      }
      
      // 在主线程定期检查任务完成（在Update中）
      // 当任务完成时，从临时存储获取结果并更新组件
  }
  ```
- [ ] **依赖**: 4.5.1, 4.5.2
- [ ] **注意事项**:
  - ECS组件更新需要在主线程执行
  - 工作线程生成的结果需要安全传递到主线程
  - 可以使用临时存储 + 主线程轮询的方式
- [ ] **测试**: 验证结果正确传递和组件更新

### 阶段5: GPU上传集成 (优先级: 中)

#### 5.1 集成GPU上传流程
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 确保生成的Mesh正确上传到GPU
- [ ] **内容**:
  ```cpp
  void LODGenerateTask::OnLODGenerated(int lodLevel, Ref<Mesh> mesh) {
      if (!mesh) {
          // 生成失败
          OnLODReady(lodLevel, nullptr);
          return;
      }
      
      // 检查是否需要上传到GPU
      if (!mesh->IsUploaded()) {
          // 将上传任务添加到AsyncResourceLoader的完成队列
          // 或者直接在主线程回调中上传
          // 方案：在主线程回调中上传（更简单）
      }
      
      lodMeshes[lodLevel] = mesh;
      OnLODReady(lodLevel, mesh);
  }
  
  // 在主线程回调中
  void OnLODConfigReady(const LODLoadResult& result) {
      // 确保所有Mesh都已上传到GPU
      for (auto& mesh : result.config.lodMeshes) {
          if (mesh && !mesh->IsUploaded()) {
              mesh->Upload();
          }
      }
      
      // 调用用户回调
      if (userCallback) {
          userCallback(result);
      }
  }
  ```
- [ ] **依赖**: 4.1
- [ ] **注意事项**: 
  - GPU上传必须在主线程执行
  - 需要确保OpenGL上下文可用
- [ ] **测试**:
  - 测试GPU上传正确执行
  - 测试上传失败的错误处理

### 阶段6: 进度和状态管理 (优先级: 中)

#### 6.1 实现进度跟踪
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 实现细粒度的进度更新
- [ ] **内容**:
  ```cpp
  class LODLoadTask {
  protected:
      void UpdateProgress(float newProgress) {
          progress.store(newProgress);
          
          // 可选：触发进度回调
          if (progressCallback) {
              progressCallback(newProgress);
          }
      }
      
      std::function<void(float)> progressCallback;
  };
  
  // 在文件加载中
  void OnMeshLoaded(int lodLevel, const MeshLoadResult& result) {
      completedCount++;
      float fileProgress = (float)completedCount / totalCount;
      
      // 考虑GPU上传进度
      float uploadProgress = CalculateUploadProgress();
      float totalProgress = fileProgress * 0.8f + uploadProgress * 0.2f;
      
      UpdateProgress(totalProgress);
  }
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试进度值正确更新
  - 测试进度回调触发

#### 6.2 实现任务取消
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 支持取消正在进行的加载任务
- [ ] **内容**:
  ```cpp
  void LODLoadTask::Cancel() {
      cancelled = true;
      status = LoadStatus::Failed;
      errorType = LoadErrorType::Cancelled;
      
      // 取消所有子任务
      for (auto& task : meshTasks) {
          if (task) {
              task->Cancel();
          }
      }
      
      for (auto& handle : generateTasks) {
          if (handle && !handle->IsCompleted()) {
              // TaskScheduler不支持取消，但可以标记为忽略结果
          }
      }
  }
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试取消文件加载任务
  - 测试取消生成任务
  - 测试取消后的资源清理

### 阶段7: 错误处理和重试 (优先级: 中)

#### 7.1 实现错误处理
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 完善的错误处理和报告
- [ ] **内容**:
  ```cpp
  void LODFileLoadTask::OnMeshLoaded(int lodLevel, const MeshLoadResult& result) {
      if (!result.IsSuccess()) {
          // 记录错误
          if (lodLevel == 0) {
              // LOD0加载失败是严重错误
              status = LoadStatus::Failed;
              errorType = result.errorType;
              errorMessage = "Failed to load LOD0: " + result.errorMessage;
              NotifyCompletion();
              return;
          } else {
              // LOD1-3失败可以容忍
              LOG_WARNING_F("Failed to load LOD%d: %s", lodLevel, result.errorMessage.c_str());
          }
      }
      
      // 继续处理...
  }
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试各种错误情况
  - 测试错误消息正确传递

#### 7.2 实现重试机制（可选）
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 为文件加载添加重试机制
- [ ] **内容**:
  ```cpp
  struct RetryPolicy {
      size_t maxRetries = 3;
      std::chrono::milliseconds retryDelay{1000};
      std::vector<LoadErrorType> retryableErrors = {
          LoadErrorType::NetworkTimeout,
          LoadErrorType::OutOfMemory
      };
  };
  
  void LODFileLoadTask::OnMeshLoadFailed(int lodLevel, const MeshLoadResult& result) {
      if (ShouldRetry(result.errorType)) {
          // 延迟后重试
          ScheduleRetry(lodLevel);
      } else {
          // 不可重试的错误
          HandleFinalFailure(lodLevel, result);
      }
  }
  ```
- [ ] **依赖**: 7.1
- [ ] **测试**:
  - 测试重试逻辑
  - 测试重试次数限制

### 阶段8: 向后兼容和同步接口 (优先级: 高)

#### 8.1 保持同步接口不变
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 确保现有同步接口继续工作
- [ ] **内容**:
  ```cpp
  // 保持原有接口不变
  LODConfig LoadLODConfig(
      Ref<Mesh> baseMesh,
      const LODLoadOptions& options
  ) {
      // 如果启用了异步，但用户调用同步接口，强制同步执行
      LODLoadOptions syncOptions = options;
      syncOptions.loadStrategy.asyncLoad = false;
      
      // 原有实现...
  }
  ```
- [ ] **依赖**: 无
- [ ] **测试**:
  - 测试所有现有同步接口
  - 确保行为一致

#### 8.2 添加同步等待辅助方法
- [ ] **文件**: `include/render/lod_loader.h`, `src/rendering/lod_loader.cpp`
- [ ] **任务**: 提供从异步任务获取同步结果的辅助方法
- [ ] **内容**:
  ```cpp
  /**
   * @brief 等待异步任务完成并返回结果（同步阻塞）
   * 
   * @param task 异步任务
   * @param timeoutSeconds 超时时间（0表示无限等待）
   * @return LODConfig 加载结果
   */
  static LODConfig WaitForLODConfig(
      std::shared_ptr<LODLoadTask> task,
      float timeoutSeconds = 0.0f
  );
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试等待任务完成
  - 测试超时处理

### 阶段9: 文档和示例 (优先级: 低)

#### 9.1 更新API文档
- [ ] **文件**: `docs/api/LOD.md`
- [ ] **任务**: 添加异步加载API文档
- [ ] **内容**:
  - 异步接口说明
  - 使用示例
  - 最佳实践
  - 性能建议

#### 9.2 创建使用示例
- [ ] **文件**: `examples/XX_lod_async_loading.cpp`
- [ ] **任务**: 创建完整的异步加载示例
- [ ] **内容**:
  ```cpp
  // 示例1: 异步文件加载
  auto task = LODLoader::LoadLODConfigAsync(
      nullptr,
      options,
      [](const LODLoadResult& result) {
          if (result.IsSuccess()) {
              // 使用LOD配置
          }
      }
  );
  
  // 示例2: 异步生成
  options.autoGenerateLOD = true;
  options.loadStrategy.asyncLoad = true;
  options.loadStrategy.backgroundGeneration = true;
  
  auto task = LODLoader::LoadLODConfigAsync(baseMesh, options, callback);
  
  // 示例3: 等待完成
  auto config = LODLoader::WaitForLODConfig(task);
  ```
- [ ] **依赖**: 8.2

### 阶段10: 性能优化 (优先级: 中)

#### 10.1 优化任务调度
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 优化任务优先级和调度
- [ ] **内容**:
  ```cpp
  // LOD0优先级最高，LOD3最低
  float GetLODPriority(int lodLevel) {
      return 1.0f - (lodLevel * 0.2f);  // LOD0=1.0, LOD1=0.8, LOD2=0.6, LOD3=0.4
  }
  
  // 批量提交时使用合适的优先级
  for (int lodLevel = 0; lodLevel <= 3; ++lodLevel) {
      float priority = GetLODPriority(lodLevel) * options.loadStrategy.asyncPriority;
      // 提交任务...
  }
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试优先级调度
  - 性能对比

#### 10.2 实现资源缓存
- [ ] **文件**: `src/rendering/lod_loader.cpp`
- [ ] **任务**: 缓存已加载的LOD网格
- [ ] **内容**:
  ```cpp
  class LODCache {
  private:
      std::unordered_map<std::string, std::weak_ptr<LODConfig>> cache;
      std::mutex cacheMutex;
      
  public:
      std::shared_ptr<LODConfig> Get(const std::string& key) {
          std::lock_guard<std::mutex> lock(cacheMutex);
          auto it = cache.find(key);
          if (it != cache.end()) {
              return it->second.lock();
          }
          return nullptr;
      }
      
      void Put(const std::string& key, std::shared_ptr<LODConfig> config) {
          std::lock_guard<std::mutex> lock(cacheMutex);
          cache[key] = config;
      }
  };
  ```
- [ ] **依赖**: 4.1
- [ ] **测试**:
  - 测试缓存命中
  - 测试内存管理

### 阶段11: 测试和验证 (优先级: 高)

#### 11.1 单元测试
- [ ] **文件**: `tests/test_lod_async_loading.cpp`
- [ ] **任务**: 创建完整的单元测试
- [ ] **测试用例**:
  - 异步文件加载成功
  - 异步文件加载失败
  - 异步LOD生成成功
  - 异步LOD生成失败
  - 混合模式（文件+生成）
  - 任务取消
  - 进度更新
  - 错误处理
  - 向后兼容性

#### 11.2 性能测试
- [ ] **文件**: `tests/benchmark_lod_async.cpp`
- [ ] **任务**: 性能对比测试
- [ ] **测试内容**:
  - 同步 vs 异步加载时间
  - 主线程阻塞时间对比
  - 内存使用对比
  - 多线程利用率

#### 11.3 集成测试
- [ ] **文件**: `examples/XX_lod_async_integration.cpp`
- [ ] **任务**: 完整场景集成测试
- [ ] **测试场景**:
  - 大量模型的异步LOD加载
  - 实时生成LOD的性能
  - 与渲染系统的集成

## 实现优先级总结

### 高优先级（核心功能）
1. ✅ 阶段1: 核心数据结构
2. ✅ 阶段2: 异步文件加载
3. ✅ 阶段3: 异步LOD生成
4. ✅ 阶段4: 统一异步接口
5. ✅ 阶段8: 向后兼容
6. ✅ 阶段11: 测试和验证

### 中优先级（增强功能）
1. 阶段5: GPU上传集成
2. 阶段6: 进度和状态管理
3. 阶段7: 错误处理和重试
4. 阶段10: 性能优化

### 低优先级（完善功能）
1. 阶段9: 文档和示例

## 注意事项

### 线程安全
- Mesh数据在工作线程中的访问需要确保线程安全
- 可能需要深拷贝顶点数据到工作线程
- GPU上传必须在主线程执行

### 内存管理
- 异步任务的生命周期管理
- 避免循环引用（使用weak_ptr）
- 及时清理完成的任务

### 错误处理
- 网络错误、文件不存在等需要适当处理
- 部分LOD级别失败不应影响整体加载
- 提供清晰的错误消息

### 性能考虑
- 避免过度创建线程
- 合理使用任务优先级
- 考虑资源缓存

## 预期效果

实现完成后，LOD系统将具备：
1. ✅ 完整的异步加载支持（复用现有基础设施）
2. ✅ 主线程不再阻塞
3. ✅ 更好的用户体验（进度反馈）
4. ✅ 充分利用多核CPU
5. ✅ 向后兼容的API
6. ✅ **最小化代码重复**（最大化复用AsyncResourceLoader和TaskScheduler）
7. ✅ **统一的错误处理和重试机制**（自动获得AsyncResourceLoader的功能）

## ECS组件变化事件集成方案

### 方案优势

使用ECS组件变化事件处理LOD异步加载结果的优势：

1. **事件驱动架构**
   - 无需轮询TaskHandle状态
   - 组件变化自动触发事件
   - 符合ECS设计模式

2. **系统解耦**
   - LOD加载器只负责更新组件
   - 其他系统通过监听组件变化响应
   - 易于扩展和维护

3. **统一的事件系统**
   - 复用现有的组件变化事件机制
   - 与其他ECS系统保持一致
   - 支持多个系统同时监听

4. **线程安全**
   - 组件更新在主线程执行
   - 工作线程结果通过安全方式传递
   - 避免竞态条件

### 工作流程

```
1. 创建LODLoadingComponent组件
   ↓
2. 启动异步加载任务
   ↓
3. 工作线程执行加载/生成
   ↓
4. 主线程更新LODLoadingComponent状态
   ↓
5. 触发ComponentChangeEvent
   ↓
6. LODLoadingSystem响应事件
   ↓
7. 执行GPU上传、更新LODComponent等
```

### 与现有方案对比

| 特性 | 轮询方案 | ECS组件变化事件方案 |
|------|---------|-------------------|
| 响应方式 | 主动轮询 | 事件驱动 |
| 性能 | 需要定期检查 | 按需触发 |
| 代码复杂度 | 中等 | 较低 |
| 系统集成 | 需要手动集成 | 自动集成 |
| 扩展性 | 中等 | 高 |

## 复用优势总结

### 复用AsyncResourceLoader的优势
- ✅ 自动获得任务队列管理
- ✅ 自动获得重试机制
- ✅ 自动获得错误处理
- ✅ 自动获得GPU上传管理
- ✅ 自动获得状态查询接口
- ✅ 无需重复实现工作线程池

### 复用TaskScheduler的优势
- ✅ 自动获得线程池管理
- ✅ 自动获得优先级调度
- ✅ 自动获得任务等待机制
- ✅ 自动获得统计信息
- ✅ 无需重复实现任务队列

### 代码量对比
- **原方案**（独立实现）: ~1500行代码
- **复用方案**: ~500行代码（减少67%）
- **维护成本**: 显著降低（复用经过测试的基础设施）

## 时间估算

- 阶段1-4（核心功能）: 3-5天
- 阶段5-7（增强功能）: 2-3天
- 阶段8-11（完善和测试）: 2-3天
- **总计**: 7-11天

## 后续优化方向

1. **流式加载**: 按需加载LOD级别，而不是预加载所有
2. **预测加载**: 基于相机移动预测需要加载的LOD
3. **压缩存储**: LOD网格的压缩存储和快速解压
4. **GPU生成**: 在GPU上生成LOD（使用计算着色器）
