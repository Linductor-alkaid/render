#include <render/async_resource_loader.h>
#include <render/logger.h>
#include <render/mesh.h>
#include <render/resource_manager.h>
#include <render/renderer.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Render;

namespace {

std::string BuildMissingFilePath() {
#ifdef PROJECT_SOURCE_DIR
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/async_missing_model.obj";
#else
    return "async_missing_model.obj";
#endif
}

void PrintTaskState(const std::shared_ptr<MeshLoadTask>& task, const std::string& prefix) {
    if (!task) {
        std::cout << prefix << "任务句柄为空\n";
        return;
    }

    auto status = task->status.load();
    std::cout << prefix
              << "状态=" << static_cast<int>(status)
              << " 结果=" << (task->result ? "有效" : "空")
              << " 错误信息=" << (task->errorMessage.empty() ? "<空>" : task->errorMessage)
              << '\n';
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);

    Renderer renderer;
    if (!renderer.Initialize("AsyncResourceLoaderFailureTest", 320, 240)) {
        std::cerr << "[async_resource_loader_failure_test] 渲染器初始化失败\n";
        return 1;
    }

    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(1);

    const std::string missingPath = BuildMissingFilePath();
    std::cout << "尝试异步加载不存在的模型: " << missingPath << '\n';

    auto task = asyncLoader.LoadMeshAsync(
        missingPath,
        "async_missing_model",
        [](const MeshLoadResult& result) {
            std::cout << "[回调] 状态=" << static_cast<int>(result.status)
                      << " 资源=" << (result.resource ? "有效" : "空")
                      << " 错误信息=" << (result.errorMessage.empty() ? "<空>" : result.errorMessage)
                      << '\n';
        });

    if (!task) {
        std::cerr << "[async_resource_loader_failure_test] 提交任务失败\n";
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    bool processed = false;
    const int maxIterations = 200;
    for (int i = 0; i < maxIterations; ++i) {
        if (asyncLoader.GetWaitingUploadCount() > 0) {
            asyncLoader.ProcessCompletedTasks(8);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        auto status = task->status.load();
        if (status == LoadStatus::Completed || status == LoadStatus::Failed) {
            processed = true;
            break;
        }
    }

    PrintTaskState(task, "[主线程] ");

    if (!processed) {
        std::cerr << "[async_resource_loader_failure_test] 在超时前未处理完任务\n";
    }

    bool bugDetected = task &&
                       task->status.load() == LoadStatus::Completed &&
                       task->result == nullptr;

    if (bugDetected) {
        std::cout << "\n[发现问题] LoadMeshAsync 在资源加载失败时仍返回 Completed 状态且结果为空。\n"
                  << "预期行为：status 应为 Failed，并提供错误信息。\n";
    } else {
        std::cout << "\n未触发问题，当前实现可能已修复。\n";
    }

    asyncLoader.Shutdown();
    ResourceManager::GetInstance().Clear();
    renderer.Shutdown();

    return 0;
}


