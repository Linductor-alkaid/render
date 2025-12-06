/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/async_resource_loader.h"
#include "render/mesh_loader.h"
#include "render/texture_loader.h"
#include "render/model_loader.h"
#include "render/logger.h"
#include "render/gl_thread_checker.h"
#include "render/error.h"
#include "render/task_scheduler.h"
#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <unordered_map>

namespace Render {

AsyncResourceLoader& AsyncResourceLoader::GetInstance() {
    static AsyncResourceLoader instance;
    return instance;
}

AsyncResourceLoader::AsyncResourceLoader() {
    Logger::GetInstance().Info("AsyncResourceLoader: 构造");
}

AsyncResourceLoader::~AsyncResourceLoader() {
    Shutdown();
}

void AsyncResourceLoader::Initialize() {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("初始化异步资源加载器");
    Logger::GetInstance().Info("========================================");
    
    // ✅ 检查TaskScheduler是否已初始化
    if (!TaskScheduler::GetInstance().IsInitialized()) {
        Logger::GetInstance().Warning(
            "AsyncResourceLoader: TaskScheduler未初始化，将自动初始化"
        );
        TaskScheduler::GetInstance().Initialize();
    }
    
    Logger::GetInstance().InfoFormat(
        "AsyncResourceLoader: 使用TaskScheduler (%zu 个工作线程)",
        TaskScheduler::GetInstance().GetWorkerCount()
    );
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("异步资源加载器初始化完成");
    Logger::GetInstance().Info("========================================");
}

void AsyncResourceLoader::Shutdown() {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("关闭异步资源加载器");
    Logger::GetInstance().Info("========================================");
    
    // ✅ 清空完成队列
    ClearAllPendingTasks();
    
    // ✅ 打印统计
    PrintStatistics();
    
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("异步资源加载器已关闭");
    Logger::GetInstance().Info("========================================");
    
    // ✅ 注意：不再关闭工作线程，因为线程由TaskScheduler管理
}

void AsyncResourceLoader::ClearAllPendingTasks() {
    size_t completedCleared = 0;
    
    // 清空已完成队列（这些任务的回调将不会被执行）
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        completedCleared = m_completedTasks.size();
        while (!m_completedTasks.empty()) {
            m_completedTasks.pop();
        }
    }
    
    if (completedCleared > 0) {
        Logger::GetInstance().InfoFormat(
            "清理任务队列: %zu 个已完成（待上传）",
            completedCleared
        );
    }
    
    // ✅ 注意：待处理任务由TaskScheduler管理，这里不再清理
}

// ✅ WorkerThreadFunc已移除，所有任务通过TaskScheduler执行

std::shared_ptr<MeshLoadTask> AsyncResourceLoader::LoadMeshAsync(
    const std::string& filepath,
    const std::string& name,
    std::function<void(const MeshLoadResult&)> callback,
    float priority)
{
    auto task = std::make_shared<MeshLoadTask>();
    task->name = name.empty() ? filepath : name;
    task->type = AsyncResourceType::Mesh;
    task->priority = priority;
    task->callback = callback;
    task->submitTime = std::chrono::steady_clock::now();
    task->maxRetries = 3;
    
    // 定义加载函数（在工作线程执行）
    task->loadFunc = [filepath]() -> Ref<Mesh> {
        Logger::GetInstance().Info("⭐ TaskScheduler：开始加载网格数据 " + filepath);
        
        try {
            auto meshes = MeshLoader::LoadFromFile(filepath, true, false);
            if (!meshes.empty()) {
                auto mesh = meshes[0];
                
                if (mesh->IsUploaded()) {
                    Logger::GetInstance().Error("❌ 错误：网格在工作线程被上传了！");
                    return nullptr;
                }
                
                Logger::GetInstance().Info("✅ TaskScheduler：网格数据加载完成（未上传）");
                return mesh;
            }
        } catch (const std::exception& e) {
            Logger::GetInstance().Error("TaskScheduler加载异常: " + std::string(e.what()));
        }
        
        return nullptr;
    };
    
    // 定义上传函数（在主线程执行）
    task->uploadFunc = [filepath](Ref<Mesh> mesh) {
        if (!mesh) {
            Logger::GetInstance().Error("上传失败：网格为空");
            return;
        }
        
        if (mesh->IsUploaded()) {
            Logger::GetInstance().Warning("网格已上传，跳过: " + filepath);
            return;
        }
        
        try {
            GL_THREAD_CHECK();
            Logger::GetInstance().Info("⭐ 主线程：开始上传网格到GPU: " + filepath);
            mesh->Upload();
            Logger::GetInstance().Info("✅ 主线程：网格上传完成: " + filepath);
        } catch (const std::exception& e) {
            Logger::GetInstance().Error("GPU上传异常: " + std::string(e.what()));
            throw;
        }
    };
    
    // ✅ 提交任务到TaskScheduler
    m_totalTasks++;
    m_loadingCount++;
    
    // 转换优先级
    TaskPriority taskPriority = TaskPriority::Low; // 资源加载默认低优先级
    if (priority > 0.5f) {
        taskPriority = TaskPriority::Normal;
    }
    if (priority > 0.8f) {
        taskPriority = TaskPriority::High;
    }
    
    TaskScheduler::GetInstance().SubmitLambda(
        [this, task]() {
            // 执行加载
            try {
                task->ExecuteLoad();
                
                m_loadingCount--;
                
                if (task->IsCancelled()) {
                    return;
                }
                
                if (task->status != LoadStatus::Failed) {
                    task->status = LoadStatus::Loaded;
                    
                    // 移到完成队列
                    std::lock_guard<std::mutex> lock(m_completedMutex);
                    m_completedTasks.push(task);
                } else {
                    m_failedTasks++;
                }
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "AsyncResourceLoader: 任务执行异常: %s",
                    e.what()
                );
                task->status = LoadStatus::Failed;
                task->errorMessage = e.what();
                m_failedTasks++;
                m_loadingCount--;
            }
        },
        taskPriority,
        task->name.c_str()
    );
    
    Logger::GetInstance().InfoFormat(
        "✅ 提交异步加载任务: %s (优先级: %.2f)",
        task->name.c_str(),
        priority
    );
    
    return task;
}

std::shared_ptr<TextureLoadTask> AsyncResourceLoader::LoadTextureAsync(
    const std::string& filepath,
    const std::string& name,
    bool generateMipmap,
    std::function<void(const TextureLoadResult&)> callback,
    float priority)
{
    auto task = std::make_shared<TextureLoadTask>();
    task->name = name.empty() ? filepath : name;
    task->type = AsyncResourceType::Texture;
    task->priority = priority;
    task->callback = callback;
    task->submitTime = std::chrono::steady_clock::now();
    task->maxRetries = 3;
    
    // 定义加载函数（工作线程）
    task->loadFunc = [filepath, generateMipmap]() -> std::unique_ptr<TextureLoader::TextureStagingData> {
        Logger::GetInstance().Debug("TaskScheduler：解码纹理数据 " + filepath);

        auto staging = std::make_unique<TextureLoader::TextureStagingData>();
        std::string errorMessage;
        if (!TextureLoader::GetInstance().DecodeTextureToStaging(
                filepath,
                generateMipmap,
                staging.get(),
                &errorMessage)) {
            if (errorMessage.empty()) {
                errorMessage = "TaskScheduler：纹理解码失败 " + filepath;
            }
            throw std::runtime_error(errorMessage);
        }

        return staging;
    };
    
    // 定义上传函数（主线程）
    const std::string resolvedName = task->name;
    task->uploadFunc = [resolvedName, filepath](TextureLoader::TextureStagingData&& staging)
        -> Ref<Texture> {
        std::string cacheName = resolvedName.empty() ? filepath : resolvedName;
        auto texture = TextureLoader::GetInstance().UploadStagedTexture(cacheName, std::move(staging));
        if (!texture) {
            throw std::runtime_error("主线程：上传纹理失败 " + cacheName);
        }
        return texture;
    };
    
    // ✅ 提交任务到TaskScheduler
    m_totalTasks++;
    m_loadingCount++;
    
    TaskPriority taskPriority = TaskPriority::Low;
    if (priority > 0.5f) taskPriority = TaskPriority::Normal;
    if (priority > 0.8f) taskPriority = TaskPriority::High;
    
    TaskScheduler::GetInstance().SubmitLambda(
        [this, task]() {
            try {
                task->ExecuteLoad();
                m_loadingCount--;
                
                if (!task->IsCancelled() && task->status != LoadStatus::Failed) {
                    task->status = LoadStatus::Loaded;
                    std::lock_guard<std::mutex> lock(m_completedMutex);
                    m_completedTasks.push(task);
                } else {
                    m_failedTasks++;
                }
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "AsyncResourceLoader: 纹理加载异常: %s",
                    e.what()
                );
                task->status = LoadStatus::Failed;
                task->errorMessage = e.what();
                m_failedTasks++;
                m_loadingCount--;
            }
        },
        taskPriority,
        task->name.c_str()
    );
    
    Logger::GetInstance().Info("✅ 提交纹理加载任务: " + task->name);
    
    return task;
}

std::shared_ptr<ModelLoadTask> AsyncResourceLoader::LoadModelAsync(
    const std::string& filepath,
    const std::string& name,
    const ModelLoadOptions& options,
    std::function<void(const ModelLoadResult&)> callback,
    float priority)
{
    auto task = std::make_shared<ModelLoadTask>();
    task->name = name.empty() ? filepath : name;
    task->type = AsyncResourceType::Model;
    task->priority = priority;
    task->callback = callback;
    task->requestedOptions = options;
    task->filepath = filepath;
    task->overrideName = name;
    task->submitTime = std::chrono::steady_clock::now();
    task->maxRetries = 3;

    ModelLoadOptions workerOptions = options;
    workerOptions.autoUpload = false;
    workerOptions.registerModel = false;
    workerOptions.registerMeshes = false;
    workerOptions.registerMaterials = false;
    workerOptions.updateDependencyGraph = false;

    task->loadFunc = [filepath, overrideName = name, workerOptions]() -> ModelLoadOutput {
        Logger::GetInstance().Info("⭐ TaskScheduler：开始解析模型数据 " + filepath);
        return ModelLoader::LoadFromFile(filepath, overrideName, workerOptions);
    };

    task->uploadFunc = [options, filepath](ModelLoadOutput& output) {
        if (!output.model) {
            Logger::GetInstance().Error("❌ 模型上传阶段失败：模型数据为空 - " + filepath);
            return;
        }

        GL_THREAD_CHECK();

        if (options.autoUpload) {
            std::unordered_set<Mesh*> uploaded;
            output.model->AccessParts([&uploaded](const std::vector<ModelPart>& parts) {
                for (const auto& part : parts) {
                    if (!part.mesh) {
                        continue;
                    }
                    Mesh* meshPtr = part.mesh.get();
                    if (uploaded.insert(meshPtr).second && !part.mesh->IsUploaded()) {
                        part.mesh->Upload();
                    }
                }
            });
        }

        // ✅ 处理延迟纹理加载（必须在主线程执行）
        TextureLoader& textureLoader = TextureLoader::GetInstance();
        std::unordered_map<std::string, Ref<Texture>> textureCache;
        output.model->ModifyParts([&](std::vector<ModelPart>& parts) {
            for (auto& part : parts) {
                if (!part.material || !part.extraData) {
                    continue;
                }

                auto& pending = part.extraData->pendingTextureRequests;
                if (pending.empty()) {
                    continue;
                }

                for (const auto& request : pending) {
                    if (request.filePath.empty()) {
                        continue;
                    }

                    const std::string cacheKey = request.textureName.empty()
                        ? request.filePath
                        : request.textureName;

                    Ref<Texture> texture;
                    if (auto cacheIt = textureCache.find(cacheKey); cacheIt != textureCache.end()) {
                        texture = cacheIt->second;
                    } else {
                        try {
                            texture = textureLoader.LoadTexture(
                                cacheKey,
                                request.filePath,
                                request.generateMipmap);
                        } catch (const std::exception& e) {
                            Logger::GetInstance().WarningFormat(
                                "[AsyncResourceLoader] Failed to load deferred texture '%s': %s",
                                request.filePath.c_str(),
                                e.what());
                        }

                        if (texture) {
                            textureCache.emplace(cacheKey, texture);
                        }
                    }

                    if (texture) {
                        part.material->SetTexture(request.slotName, texture);
                    } else {
                        Logger::GetInstance().WarningFormat(
                            "[AsyncResourceLoader] Deferred texture unavailable: slot=%s path=%s",
                            request.slotName.c_str(),
                            request.filePath.c_str());
                    }
                }

                pending.clear();
            }
        });

        if (options.registerModel || options.registerMeshes || options.registerMaterials) {
            ModelLoader::RegisterResources(
                output.modelName.empty() ? filepath : output.modelName,
                output.model,
                options,
                &output.meshResourceNames,
                &output.materialResourceNames
            );
        }
    };

    // ✅ 提交任务到TaskScheduler
    m_totalTasks++;
    m_loadingCount++;
    
    TaskPriority taskPriority = TaskPriority::Low;
    if (priority > 0.5f) taskPriority = TaskPriority::Normal;
    if (priority > 0.8f) taskPriority = TaskPriority::High;
    
    TaskScheduler::GetInstance().SubmitLambda(
        [this, task]() {
            try {
                task->ExecuteLoad();
                m_loadingCount--;
                
                if (!task->IsCancelled() && task->status != LoadStatus::Failed) {
                    task->status = LoadStatus::Loaded;
                    std::lock_guard<std::mutex> lock(m_completedMutex);
                    m_completedTasks.push(task);
                } else {
                    m_failedTasks++;
                }
            } catch (const std::exception& e) {
                Logger::GetInstance().ErrorFormat(
                    "AsyncResourceLoader: 模型加载异常: %s",
                    e.what()
                );
                task->status = LoadStatus::Failed;
                task->errorMessage = e.what();
                m_failedTasks++;
                m_loadingCount--;
            }
        },
        taskPriority,
        task->name.c_str()
    );

    Logger::GetInstance().Info("✅ 提交模型加载任务: " + task->name);

    return task;
}

size_t AsyncResourceLoader::ProcessCompletedTasks(size_t maxTasks) {
    // ✅ 确保在主线程（OpenGL上下文线程）
    try {
        GL_THREAD_CHECK();
    } catch (const std::exception& e) {
        Logger::GetInstance().Error("❌ ProcessCompletedTasks 必须在主线程调用！");
        Logger::GetInstance().Error("异常: " + std::string(e.what()));
        return 0;
    }
    
    size_t processed = 0;
    
    for (size_t i = 0; i < maxTasks; ++i) {
        std::shared_ptr<LoadTaskBase> task;
        
        // 获取已完成的任务
        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            if (m_completedTasks.empty()) {
                break;
            }
            task = m_completedTasks.front();
            m_completedTasks.pop();
        }
        
        if (!task) {
            break;
        }
        
        // 检查任务是否已取消
        if (task->IsCancelled()) {
            Logger::GetInstance().Info("跳过已取消的任务: " + task->name);
            continue;
        }
        
        try {
            task->status = LoadStatus::Uploading;
            
            // 执行GPU上传（主线程）
            task->ExecuteUpload();
            
            // 再次检查是否已取消
            if (task->IsCancelled()) {
                Logger::GetInstance().Info("任务在上传过程中被取消: " + task->name);
                continue;
            }
            
            if (task->status != LoadStatus::Failed) {
                task->status = LoadStatus::Completed;
                m_completedCount++;
                
                Logger::GetInstance().Info("✅ 资源上传完成: " + task->name);
            } else {
                Logger::GetInstance().Error("❌ 资源上传失败: " + task->name);
                m_failedTasks++;
            }
            
            processed++;
            
            // 调用回调
            if (auto meshTask = std::dynamic_pointer_cast<MeshLoadTask>(task)) {
                if (meshTask->callback) {
                    MeshLoadResult result;
                    result.resource = meshTask->result;
                    result.name = meshTask->name;
                    result.status = task->status;
                    result.errorMessage = task->errorMessage;
                    result.errorType = task->errorType;
                    meshTask->callback(result);
                }
            } else if (auto texTask = std::dynamic_pointer_cast<TextureLoadTask>(task)) {
                if (texTask->callback) {
                    TextureLoadResult result;
                    result.resource = texTask->result;
                    result.name = texTask->name;
                    result.status = task->status;
                    result.errorMessage = task->errorMessage;
                    result.errorType = task->errorType;
                    texTask->callback(result);
                }
            } else if (auto modelTask = std::dynamic_pointer_cast<ModelLoadTask>(task)) {
                if (modelTask->callback) {
                    ModelLoadResult result;
                    result.resource = modelTask->result.model;
                    result.name = modelTask->result.modelName.empty()
                        ? modelTask->name
                        : modelTask->result.modelName;
                    result.status = task->status;
                    result.errorMessage = task->errorMessage;
                    result.errorType = task->errorType;
                    result.meshResourceNames = modelTask->result.meshResourceNames;
                    result.materialResourceNames = modelTask->result.materialResourceNames;
                    modelTask->callback(result);
                }
            }
            
        } catch (const std::exception& e) {
            Logger::GetInstance().Error("GPU上传异常: " + std::string(e.what()));
            task->status = LoadStatus::Failed;
            task->errorMessage = "Upload exception: " + std::string(e.what());
            m_failedTasks++;
        }
    }
    
    return processed;
}

bool AsyncResourceLoader::WaitForAll(float timeoutSeconds) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (true) {
        size_t waiting = GetWaitingUploadCount();
        size_t loading = m_loadingCount.load();
        
        // ✅ 注意：不再检查待处理队列，因为由TaskScheduler管理
        if (waiting == 0 && loading == 0) {
            return true;  // 所有任务完成
        }
        
        // 检查超时
        if (timeoutSeconds > 0.0f) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto elapsedSeconds = std::chrono::duration<float>(elapsed).count();
            
            if (elapsedSeconds >= timeoutSeconds) {
                Logger::GetInstance().Warning("WaitForAll: 超时 (" + 
                                             std::to_string(elapsedSeconds) + "秒)");
                return false;  // 超时
            }
        }
        
        // 短暂休眠避免忙等待
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

size_t AsyncResourceLoader::GetPendingTaskCount() const {
    // ✅ 待处理任务由TaskScheduler管理
    return TaskScheduler::GetInstance().GetPendingTaskCount();
}

size_t AsyncResourceLoader::GetLoadingTaskCount() const {
    return m_loadingCount.load();
}

size_t AsyncResourceLoader::GetWaitingUploadCount() const {
    std::lock_guard<std::mutex> lock(m_completedMutex);
    return m_completedTasks.size();
}

void AsyncResourceLoader::PrintStatistics() const {
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("异步加载器统计");
    Logger::GetInstance().Info("========================================");
    Logger::GetInstance().Info("总任务数: " + std::to_string(m_totalTasks.load()));
    Logger::GetInstance().Info("已完成: " + std::to_string(m_completedCount.load()));
    Logger::GetInstance().Info("失败: " + std::to_string(m_failedTasks.load()));
    Logger::GetInstance().Info("正在加载: " + std::to_string(GetLoadingTaskCount()));
    Logger::GetInstance().Info("待处理: " + std::to_string(GetPendingTaskCount()));
    Logger::GetInstance().Info("等待上传: " + std::to_string(GetWaitingUploadCount()));
    Logger::GetInstance().Info("========================================");
}

} // namespace Render

