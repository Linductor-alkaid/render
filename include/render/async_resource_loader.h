#pragma once

#include "render/types.h"
#include "render/mesh.h"
#include "render/texture.h"
#include "render/material.h"
#include "render/model_loader.h"
#include "render/texture_loader.h"
#include <memory>
#include <string>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <vector>

namespace Render {

// 前向声明
class Shader;

/**
 * @brief 异步资源类型
 */
enum class AsyncResourceType {
    Mesh,       // 网格
    Texture,    // 纹理
    Material,   // 材质
    Model       // 模型（包含多个网格和材质）
};

/**
 * @brief 加载状态
 */
enum class LoadStatus {
    Pending,        // 等待中
    Loading,        // 加载中（工作线程）
    Loaded,         // 已加载（等待GPU上传）
    Uploading,      // 上传中（主线程）
    Completed,      // 完成
    Failed          // 失败
};

/**
 * @brief 加载结果（模板）
 */
template<typename T>
struct LoadResult {
    std::shared_ptr<T> resource;
    std::string name;
    LoadStatus status;
    std::string errorMessage;
    
    bool IsSuccess() const { 
        return status == LoadStatus::Completed && resource != nullptr; 
    }
};

// 类型别名
using MeshLoadResult = LoadResult<Mesh>;
using TextureLoadResult = LoadResult<Texture>;
using MaterialLoadResult = LoadResult<Material>;
struct ModelLoadResult : LoadResult<Model> {
    std::vector<std::string> meshResourceNames;
    std::vector<std::string> materialResourceNames;
};

/**
 * @brief 加载任务基类
 */
struct LoadTaskBase {
    std::string name;                           // 资源名称
    AsyncResourceType type;                     // 资源类型
    std::atomic<LoadStatus> status{LoadStatus::Pending};
    std::string errorMessage;
    float priority = 0.0f;                      // 优先级（越高越优先）
    
    virtual ~LoadTaskBase() = default;
    virtual void ExecuteLoad() = 0;              // 工作线程：加载数据
    virtual void ExecuteUpload() = 0;            // 主线程：GPU上传
};

/**
 * @brief 网格加载任务
 */
struct MeshLoadTask : public LoadTaskBase {
    using LoadFunc = std::function<Ref<Mesh>()>;
    using UploadFunc = std::function<void(Ref<Mesh>)>;
    using CallbackFunc = std::function<void(const MeshLoadResult&)>;
    
    LoadFunc loadFunc;              // 加载函数（工作线程）
    UploadFunc uploadFunc;          // 上传函数（主线程）
    CallbackFunc callback;          // 完成回调
    
    Ref<Mesh> result;              // 加载结果
    
    void ExecuteLoad() override {
        try {
            if (loadFunc) {
                result = loadFunc();
            }
        } catch (const std::exception& e) {
            errorMessage = e.what();
            status = LoadStatus::Failed;
        }
    }
    
    void ExecuteUpload() override {
        try {
            if (uploadFunc && result) {
                uploadFunc(result);
            }
        } catch (const std::exception& e) {
            errorMessage = "Upload failed: " + std::string(e.what());
            status = LoadStatus::Failed;
        }
    }
};

/**
 * @brief 纹理加载任务
 */
struct TextureLoadTask : public LoadTaskBase {
    using LoadFunc = std::function<std::unique_ptr<TextureLoader::TextureStagingData>()>;
    using UploadFunc = std::function<Ref<Texture>(TextureLoader::TextureStagingData&&)>;
    using CallbackFunc = std::function<void(const TextureLoadResult&)>;
    
    LoadFunc loadFunc;
    UploadFunc uploadFunc;
    CallbackFunc callback;
    
    std::unique_ptr<TextureLoader::TextureStagingData> stagingData;
    Ref<Texture> result;
    
    void ExecuteLoad() override {
        try {
            if (loadFunc) {
                stagingData = loadFunc();
                if (!stagingData) {
                    status = LoadStatus::Failed;
                    if (errorMessage.empty()) {
                        errorMessage = "Texture staging data is empty";
                    }
                }
            }
        } catch (const std::exception& e) {
            errorMessage = e.what();
            status = LoadStatus::Failed;
        }
    }
    
    void ExecuteUpload() override {
        if (status == LoadStatus::Failed) {
            return;
        }

        try {
            if (!uploadFunc) {
                return;
            }

            if (!stagingData) {
                status = LoadStatus::Failed;
                if (errorMessage.empty()) {
                    errorMessage = "Upload failed: staging data missing";
                }
                return;
            }

            auto data = std::move(*stagingData);
            stagingData.reset();
            result = uploadFunc(std::move(data));
            if (!result) {
                status = LoadStatus::Failed;
                if (errorMessage.empty()) {
                    errorMessage = "Upload failed: texture creation returned null";
                }
            }
        } catch (const std::exception& e) {
            errorMessage = "Upload failed: " + std::string(e.what());
            status = LoadStatus::Failed;
        }
    }
};

struct ModelLoadTask : public LoadTaskBase {
    using LoadFunc = std::function<ModelLoadOutput()>;
    using UploadFunc = std::function<void(ModelLoadOutput&)>;
    using CallbackFunc = std::function<void(const ModelLoadResult&)>;

    LoadFunc loadFunc;
    UploadFunc uploadFunc;
    CallbackFunc callback;

    ModelLoadOptions requestedOptions;
    std::string filepath;
    std::string overrideName;

    ModelLoadOutput result;

    void ExecuteLoad() override {
        try {
            if (loadFunc) {
                result = loadFunc();
            }
        } catch (const std::exception& e) {
            errorMessage = e.what();
            status = LoadStatus::Failed;
        }
    }

    void ExecuteUpload() override {
        try {
            if (uploadFunc) {
                uploadFunc(result);
            }
        } catch (const std::exception& e) {
            errorMessage = "Upload failed: " + std::string(e.what());
            status = LoadStatus::Failed;
        }
    }
};

/**
 * @brief 异步资源加载器（单例）
 * 
 * 提供异步资源加载功能，支持后台线程加载和主线程GPU上传
 * 
 * 线程模型：
 * - 工作线程池：执行文件I/O和数据解析
 * - 主线程：轮询完成的任务并执行GPU上传
 * 
 * 使用流程：
 * 1. 主线程提交加载任务（LoadMeshAsync）
 * 2. 工作线程执行文件加载和数据解析
 * 3. 主线程轮询并处理完成的任务（ProcessCompletedTasks）
 * 4. 主线程执行GPU上传
 * 5. 调用完成回调（如果提供）
 * 
 * 线程安全：
 * - ✅ 所有公共方法都是线程安全的
 * - ✅ 使用任务队列和条件变量同步
 * - ✅ GPU上传保证在主线程执行
 */
class AsyncResourceLoader {
public:
    /**
     * @brief 获取单例实例
     */
    static AsyncResourceLoader& GetInstance();
    
    /**
     * @brief 初始化异步加载器
     * @param numThreads 工作线程数（默认为CPU核心数）
     */
    void Initialize(size_t numThreads = 0);
    
    /**
     * @brief 关闭异步加载器（等待所有任务完成）
     */
    void Shutdown();
    
    /**
     * @brief 清空所有待处理的任务队列
     * 
     * 注意：此操作会丢弃所有未完成的任务，仅在关闭时调用
     */
    void ClearAllPendingTasks();
    
    /**
     * @brief 是否已初始化
     */
    bool IsInitialized() const { return m_running.load(); }
    
    // ========================================================================
    // 异步加载接口
    // ========================================================================
    
    /**
     * @brief 异步加载网格
     * @param filepath 文件路径
     * @param name 资源名称（用于ResourceManager注册）
     * @param callback 完成回调（可选）
     * @param priority 优先级（默认0，越高越优先）
     * @return 任务句柄（可用于查询状态）
     */
    std::shared_ptr<MeshLoadTask> LoadMeshAsync(
        const std::string& filepath,
        const std::string& name = "",
        std::function<void(const MeshLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );
    
    /**
     * @brief 异步加载纹理
     * @param filepath 文件路径
     * @param name 资源名称
     * @param generateMipmap 是否生成mipmap
     * @param callback 完成回调（可选）
     * @param priority 优先级
     * @return 任务句柄
     */
    std::shared_ptr<TextureLoadTask> LoadTextureAsync(
        const std::string& filepath,
        const std::string& name = "",
        bool generateMipmap = true,
        std::function<void(const TextureLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );

    std::shared_ptr<ModelLoadTask> LoadModelAsync(
        const std::string& filepath,
        const std::string& name = "",
        const ModelLoadOptions& options = {},
        std::function<void(const ModelLoadResult&)> callback = nullptr,
        float priority = 0.0f
    );
    
    // ========================================================================
    // 任务处理（主线程调用）
    // ========================================================================
    
    /**
     * @brief 处理已完成的加载任务（执行GPU上传）
     * @param maxTasks 本帧最多处理的任务数（默认10，避免帧率下降）
     * @return 处理的任务数
     * 
     * 注意：必须在主线程（渲染循环）中调用
     */
    size_t ProcessCompletedTasks(size_t maxTasks = 10);
    
    /**
     * @brief 等待所有任务完成
     * @param timeoutSeconds 超时时间（秒，0表示无限等待）
     * @return true表示所有任务完成，false表示超时
     */
    bool WaitForAll(float timeoutSeconds = 0.0f);
    
    // ========================================================================
    // 状态查询
    // ========================================================================
    
    /**
     * @brief 获取待处理任务数
     */
    size_t GetPendingTaskCount() const;
    
    /**
     * @brief 获取正在加载的任务数
     */
    size_t GetLoadingTaskCount() const;
    
    /**
     * @brief 获取等待上传的任务数
     */
    size_t GetWaitingUploadCount() const;
    
    /**
     * @brief 打印加载统计信息
     */
    void PrintStatistics() const;
    
private:
    AsyncResourceLoader();
    ~AsyncResourceLoader();
    
    // 禁止拷贝
    AsyncResourceLoader(const AsyncResourceLoader&) = delete;
    AsyncResourceLoader& operator=(const AsyncResourceLoader&) = delete;
    
    // 工作线程函数
    void WorkerThreadFunc();
    
    // 任务队列
    std::queue<std::shared_ptr<LoadTaskBase>> m_pendingTasks;   // 待处理任务
    std::queue<std::shared_ptr<LoadTaskBase>> m_completedTasks; // 已完成任务（等待上传）
    
    // 线程同步
    mutable std::mutex m_pendingMutex;
    mutable std::mutex m_completedMutex;
    std::condition_variable m_taskAvailable;
    
    // 工作线程池
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    
    // 统计信息
    std::atomic<size_t> m_totalTasks{0};
    std::atomic<size_t> m_completedCount{0};
    std::atomic<size_t> m_failedTasks{0};
    std::atomic<size_t> m_loadingCount{0};
};

} // namespace Render

