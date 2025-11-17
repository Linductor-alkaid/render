#include <SDL3/SDL.h>

#include "render/application/application_host.h"
#include "render/application/events/frame_events.h"
#include "render/application/events/scene_events.h"
#include "render/application/modules/core_render_module.h"
#include "render/application/modules/debug_hud_module.h"
#include "render/application/modules/input_module.h"
#include "render/application/modules/ui_runtime_module.h"
#include "render/application/scenes/boot_scene.h"
#include "render/logger.h"
#include "render/renderer.h"
#include "render/resource_manager.h"
#include "render/async_resource_loader.h"

using namespace Render;
using namespace Render::Application;

namespace {

using Render::Application::FrameBeginEvent;
using Render::Application::FrameEndEvent;
using Render::Application::FrameTickEvent;
using Render::Application::Events::SceneLifecycleEvent;

void ConfigureLogger() {
    auto& logger = Logger::GetInstance();
    logger.SetLogToConsole(true);
    logger.SetLogToFile(false);
    logger.SetLogLevel(LogLevel::Debug);
}

bool InitializeRenderer(Renderer*& renderer) {
    renderer = Renderer::Create();
    if (!renderer) {
        Logger::GetInstance().Error("[ApplicationBootDemo] Failed to create renderer");
        return false;
    }

    if (!renderer->Initialize("Application Boot Demo", 1280, 720)) {
        Logger::GetInstance().Error("[ApplicationBootDemo] Failed to initialize renderer");
        Renderer::Destroy(renderer);
        renderer = nullptr;
        return false;
    }

    renderer->SetClearColor(0.1f, 0.12f, 0.16f, 1.0f);
    renderer->SetVSync(true);
    return true;
}

struct DemoEventSubscriptions {
    explicit DemoEventSubscriptions(EventBus& bus)
        : eventBus(&bus) {}

    ~DemoEventSubscriptions() {
        if (!eventBus) {
            return;
        }
        if (frameBeginListener != 0) {
            eventBus->Unsubscribe(frameBeginListener);
        }
        if (frameTickListener != 0) {
            eventBus->Unsubscribe(frameTickListener);
        }
        if (frameEndListener != 0) {
            eventBus->Unsubscribe(frameEndListener);
        }
        if (sceneLifecycleListener != 0) {
            eventBus->Unsubscribe(sceneLifecycleListener);
        }
    }

    EventBus* eventBus = nullptr;
    EventBus::ListenerId frameBeginListener = 0;
    EventBus::ListenerId frameTickListener = 0;
    EventBus::ListenerId frameEndListener = 0;
    EventBus::ListenerId sceneLifecycleListener = 0;
};

} // namespace

enum class ExperimentPhase {
    CoreOnly = 0,
    WithInput,
    WithUIRuntime,
    WithDebugHUD
};

constexpr ExperimentPhase kCurrentPhase = ExperimentPhase::CoreOnly;

constexpr bool kEnableInputModule = kCurrentPhase >= ExperimentPhase::WithInput;
constexpr bool kEnableUIRuntimeModule = kCurrentPhase >= ExperimentPhase::WithUIRuntime;
constexpr bool kEnableDebugHUDModule = kCurrentPhase >= ExperimentPhase::WithDebugHUD;

constexpr uint64_t kMaxFramesWithoutInputModule = 480; // fallback for ~8 seconds at 60fps

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    ConfigureLogger();

    try {
        Renderer* renderer = nullptr;
        if (!InitializeRenderer(renderer)) {
            return -1;
        }

        auto& resourceManager = ResourceManager::GetInstance();
        auto& asyncLoader = AsyncResourceLoader::GetInstance();
        asyncLoader.Initialize(1);

        ApplicationHost host;
        ApplicationHost::Config hostConfig{};
        hostConfig.renderer = renderer;
        hostConfig.resourceManager = &resourceManager;
        hostConfig.asyncLoader = &asyncLoader;
        hostConfig.uniformManager = nullptr; // UniformSystem 将视需求注册全局 uniform

        if (!host.Initialize(hostConfig)) {
            Logger::GetInstance().Error("[ApplicationBootDemo] Failed to initialize ApplicationHost");
            asyncLoader.Shutdown();
            Renderer::Destroy(renderer);
            return -1;
        }

        auto& moduleRegistry = host.GetModuleRegistry();
        moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>());
        Logger::GetInstance().Info("[Experiment] Registered CoreRenderModule");
        if (kEnableInputModule) {
            moduleRegistry.RegisterModule(std::make_unique<InputModule>());
            Logger::GetInstance().Info("[Experiment] Registered InputModule");
        }
        if (kEnableUIRuntimeModule) {
            moduleRegistry.RegisterModule(std::make_unique<UIRuntimeModule>());
            Logger::GetInstance().Info("[Experiment] Registered UIRuntimeModule");
        }
        if (kEnableDebugHUDModule) {
            moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>(), false);
            Logger::GetInstance().Info("[Experiment] Registered DebugHUDModule (inactive by default)");
        }
        host.RegisterSceneFactory("BootScene", []() { return std::make_unique<BootScene>(); });
        host.PushScene("BootScene");

        bool running = true;
        uint64_t frameIndex = 0;
        double absoluteTime = 0.0;

        Logger::GetInstance().Info("[ApplicationBootDemo] Press ESC or close window to exit.");

        // Demo 专用事件监听暂时关闭，以缩小排查范围
        // auto& eventBus = host.GetEventBus();
        // DemoEventSubscriptions subscriptions(eventBus);
        // subscriptions.frameBeginListener =
        //     eventBus.Subscribe<FrameBeginEvent>([](const FrameBeginEvent& evt) {
        //         if (evt.frame.frameIndex % 120 == 0) {
        //             Logger::GetInstance().InfoFormat("[EventBus] FrameBegin index=%llu time=%.2fs",
        //                                              static_cast<unsigned long long>(evt.frame.frameIndex),
        //                                              evt.frame.absoluteTime);
        //         }
        //     });
        // subscriptions.frameTickListener = eventBus.Subscribe<FrameTickEvent>([](const FrameTickEvent&) {
        //     // 可以在此处理游戏逻辑事件
        // });
        // subscriptions.frameEndListener =
        //     eventBus.Subscribe<FrameEndEvent>([](const FrameEndEvent& evt) {
        //         if (evt.frame.frameIndex % 240 == 0) {
        //             Logger::GetInstance().InfoFormat("[EventBus] FrameEnd index=%llu dt=%.4f",
        //                                              static_cast<unsigned long long>(evt.frame.frameIndex),
        //                                              evt.frame.deltaTime);
        //         }
        //     });
        // subscriptions.sceneLifecycleListener =
        //     eventBus.Subscribe<SceneLifecycleEvent>([](const SceneLifecycleEvent& evt) {
        //         Logger::GetInstance().DebugFormat("[EventBus] Scene '%s' stage %d",
        //                                           evt.sceneId.c_str(),
        //                                           static_cast<int>(evt.stage));
        //     });

        auto* inputModule = static_cast<InputModule*>(moduleRegistry.GetModule("InputModule"));

        uint64_t frameCountSinceStart = 0;
        bool preFrameSucceeded = true;
        bool sceneUpdateSucceeded = true;
        bool postFrameSucceeded = true;

        while (running) {
            Logger::GetInstance().DebugFormat("[BootDemo] Frame %llu begin", static_cast<unsigned long long>(frameIndex));
            renderer->BeginFrame();
            renderer->Clear();

            const float deltaTime = renderer->GetDeltaTime();
            absoluteTime += static_cast<double>(deltaTime);

            FrameUpdateArgs frameArgs{};
            frameArgs.deltaTime = deltaTime;
            frameArgs.absoluteTime = absoluteTime;
            frameArgs.frameIndex = frameIndex++;

            bool quitRequested = false;
            preFrameSucceeded = sceneUpdateSucceeded = postFrameSucceeded = false;

            try {
                Logger::GetInstance().Debug("[BootDemo] PreFrame phase start");
                host.GetModuleRegistry().InvokePhase(ModulePhase::PreFrame, frameArgs);
                preFrameSucceeded = true;
            } catch (const std::exception& ex) {
                Logger::GetInstance().ErrorFormat("[Experiment] PreFrame exception: %s", ex.what());
                quitRequested = true;
            }

            if (!quitRequested) {
                try {
                    Logger::GetInstance().Debug("[Experiment] SceneManager.Update start");
                    host.GetSceneManager().Update(frameArgs);
                    sceneUpdateSucceeded = true;
                } catch (const std::exception& ex) {
                    Logger::GetInstance().ErrorFormat("[Experiment] SceneManager.Update exception: %s", ex.what());
                    quitRequested = true;
                }
            }

            if (!quitRequested) {
                try {
                    Logger::GetInstance().Debug("[Experiment] PostFrame phase start");
                    host.GetModuleRegistry().InvokePhase(ModulePhase::PostFrame, frameArgs);
                    postFrameSucceeded = true;
                } catch (const std::exception& ex) {
                    Logger::GetInstance().ErrorFormat("[Experiment] PostFrame exception: %s", ex.what());
                    quitRequested = true;
                }
            }

            host.GetContext().lastFrame = frameArgs;

            if (inputModule) {
                quitRequested = inputModule->WasQuitRequested() ||
                                inputModule->IsKeyDown(SDL_SCANCODE_ESCAPE);
            } else {
                SDL_Event evt;
                while (SDL_PollEvent(&evt)) {
                    if (evt.type == SDL_EVENT_QUIT) {
                        quitRequested = true;
                        break;
                    }
                }
            }

            Logger::GetInstance().Debug("[BootDemo] Calling host.UpdateWorld");
            host.UpdateWorld(deltaTime);

            // 注释掉自动退出逻辑，让程序持续运行
            // 如果需要自动退出，可以取消注释下面的代码
            // if (!inputModule) {
            //     ++frameCountSinceStart;
            //     if (frameCountSinceStart >= kMaxFramesWithoutInputModule) {
            //         Logger::GetInstance().Warning("[Experiment] Reached max fallback frames without InputModule, exiting loop.");
            //         quitRequested = true;
            //     }
            // }

            if (quitRequested) {
                running = false;
                renderer->EndFrame();
                break;
            }

            Logger::GetInstance().Debug("[BootDemo] Flushing render queue");
            renderer->FlushRenderQueue();
            renderer->EndFrame();
            renderer->Present();

            asyncLoader.ProcessCompletedTasks(4);
            Logger::GetInstance().Debug("[BootDemo] Frame finished");
        }

        host.Shutdown();
        asyncLoader.Shutdown();

        Renderer::Destroy(renderer);

        Logger::GetInstance().Info("[ApplicationBootDemo] Shutdown complete.");
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[ApplicationBootDemo] Unhandled std::exception: %s", e.what());
        return -1;
    } catch (...) {
        Logger::GetInstance().Error("[ApplicationBootDemo] Unhandled unknown exception");
        return -1;
    }

    return 0;
}


