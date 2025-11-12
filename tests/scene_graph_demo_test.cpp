#include <render/application/application_host.h>
#include <render/application/modules/core_render_module.h>
#include <render/application/modules/input_module.h>
#include <render/application/modules/debug_hud_module.h>
#include <render/application/scene_graph.h>
#include <render/application/scene.h>
#include <render/async_resource_loader.h>
#include <render/logger.h>
#include <render/renderer.h>
#include <render/resource_manager.h>
#include <render/mesh_loader.h>
#include <render/material.h>
#include <render/shader_cache.h>
#include <render/ecs/components.h>
#include <render/ecs/world.h>
#include <render/ecs/systems.h>
#include <render/camera.h>
#include <render/math_utils.h>

#include <iostream>
#include <algorithm>
#include <optional>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using namespace Render;
using namespace Render::Application;

namespace {

class TestCubeNode : public SceneNode {
public:
    TestCubeNode()
        : SceneNode("TestCubeNode") {
        RegisterRequiredResource("scenegraph.demo.mesh", "mesh");
        RegisterRequiredResource("scenegraph.demo.material", "material");
        RegisterRequiredResource("scenegraph.demo.shader", "shader");
    }

protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto& rm = *ctx.resourceManager;

        if (!rm.HasMesh("scenegraph.demo.mesh")) {
            rm.RegisterMesh("scenegraph.demo.mesh", MeshLoader::CreateCube(1.0f, 1.0f, 1.0f, Color::White()));
        }

        auto shader = ShaderCache::GetInstance().LoadShader(
            "scenegraph.demo.shader", "shaders/material_phong.vert", "shaders/material_phong.frag");

        if (!rm.HasMaterial("scenegraph.demo.material")) {
            auto material = std::make_shared<Material>();
            material->SetName("scenegraph.demo.material");
            material->SetShader(shader);
            material->SetAmbientColor(Color(0.2f, 0.2f, 0.2f, 1.0f));
            material->SetDiffuseColor(Color(0.6f, 0.2f, 1.0f, 1.0f));
            material->SetSpecularColor(Color(0.8f, 0.8f, 0.8f, 1.0f));
            material->SetShininess(48.0f);
            rm.RegisterMaterial("scenegraph.demo.material", material);
        }
    }

    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "SceneGraphDemo.Cube"});

        ECS::TransformComponent transform{};
        transform.SetPosition(Vector3(0.0f, 0.5f, 0.0f));
        world.AddComponent(*m_entity, transform);

        auto& rm = GetResourceManager();
        ECS::MeshRenderComponent meshComp;
        meshComp.meshName = "scenegraph.demo.mesh";
        meshComp.materialName = "scenegraph.demo.material";
        meshComp.mesh = rm.GetMesh("scenegraph.demo.mesh");
        meshComp.material = rm.GetMaterial("scenegraph.demo.material");
        meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
        meshComp.layerID = Layers::World::Midground.value;
        world.AddComponent(*m_entity, meshComp);
    }

    void OnUpdate(const FrameUpdateArgs& frame) override {
        if (!m_entity) {
            return;
        }
        auto& world = GetWorld();
        if (!world.IsValidEntity(*m_entity)) {
            return;
        }
        auto& transform = world.GetComponent<ECS::TransformComponent>(*m_entity);
        float angle = 30.0f * static_cast<float>(frame.absoluteTime);
        transform.SetRotation(MathUtils::FromEulerDegrees(0.0f, angle, 0.0f));
    }

    void OnExit() override {
        if (m_entity && GetWorld().IsValidEntity(*m_entity)) {
            GetWorld().DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

class TestPlaneNode : public SceneNode {
public:
    TestPlaneNode()
        : SceneNode("TestPlaneNode") {
        RegisterRequiredResource("scenegraph.demo.plane.mesh", "mesh");
        RegisterRequiredResource("scenegraph.demo.plane.material", "material");
    }

protected:
    void OnAttach(Scene&, AppContext& ctx) override {
        auto& rm = *ctx.resourceManager;
        if (!rm.HasMesh("scenegraph.demo.plane.mesh")) {
            rm.RegisterMesh("scenegraph.demo.plane.mesh", MeshLoader::CreatePlane(1.0f, 1.0f, 1u, 1u, Color::White()));
        }

        if (!rm.HasMaterial("scenegraph.demo.plane.material")) {
            auto shader = ShaderCache::GetInstance().LoadShader(
                "scenegraph.demo.shader", "shaders/material_phong.vert", "shaders/material_phong.frag");
            auto material = std::make_shared<Material>();
            material->SetName("scenegraph.demo.plane.material");
            material->SetShader(shader);
            material->SetDiffuseColor(Color(0.3f, 0.3f, 0.3f, 1.0f));
            material->SetAmbientColor(Color(0.2f, 0.2f, 0.2f, 1.0f));
            rm.RegisterMaterial("scenegraph.demo.plane.material", material);
        }
    }

    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "SceneGraphDemo.Plane"});

        ECS::TransformComponent transform{};
        transform.SetPosition(Vector3::Zero());
        transform.SetScale(Vector3(6.0f, 1.0f, 6.0f));
        world.AddComponent(*m_entity, transform);

        auto& rm = GetResourceManager();
        ECS::MeshRenderComponent meshComp;
        meshComp.meshName = "scenegraph.demo.plane.mesh";
        meshComp.materialName = "scenegraph.demo.plane.material";
        meshComp.mesh = rm.GetMesh("scenegraph.demo.plane.mesh");
        meshComp.material = rm.GetMaterial("scenegraph.demo.plane.material");
        meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
        meshComp.layerID = Layers::World::Background.value;
        world.AddComponent(*m_entity, meshComp);
    }

    void OnExit() override {
        if (m_entity && GetWorld().IsValidEntity(*m_entity)) {
            GetWorld().DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

class TestPointLightNode : public SceneNode {
public:
    TestPointLightNode(Vector3 position, Color color)
        : SceneNode("SceneGraphDemo.Light")
        , m_position(position)
        , m_color(color) {}

protected:
    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "SceneGraphDemo.Light", .active = true});

        ECS::TransformComponent transform{};
        transform.SetPosition(m_position);
        world.AddComponent(*m_entity, transform);

        ECS::LightComponent light{};
        light.type = ECS::LightType::Point;
        light.color = m_color;
        light.intensity = 3.0f;
        light.range = 12.0f;
        light.attenuation = 0.25f;
        world.AddComponent(*m_entity, light);
    }

    void OnExit() override {
        if (m_entity && GetWorld().IsValidEntity(*m_entity)) {
            GetWorld().DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
    Vector3 m_position;
    Color m_color;
};

class TestCameraNode : public SceneNode {
public:
    TestCameraNode()
        : SceneNode("SceneGraphDemo.Camera") {}

protected:
    void OnEnter(const SceneEnterArgs&) override {
        auto& world = GetWorld();
        m_entity = world.CreateEntity({.name = "SceneGraphDemo.Camera", .active = true});

        ECS::TransformComponent transform{};
        transform.SetPosition(Vector3(0.0f, 2.0f, 6.0f));
        transform.transform->LookAt(Vector3::Zero());
        world.AddComponent(*m_entity, transform);

        ECS::CameraComponent camera{};
        camera.camera = std::make_shared<Camera>();
        auto* renderer = GetContext().renderer;
        float aspect = 16.0f / 9.0f;
        if (renderer) {
            float width = static_cast<float>(renderer->GetWidth());
            float height = static_cast<float>(renderer->GetHeight());
            aspect = width / std::max(1.0f, height);
        }
        camera.camera->SetPerspective(60.0f, aspect, 0.1f, 100.0f);
        world.AddComponent(*m_entity, camera);
    }

    void OnExit() override {
        if (m_entity && GetWorld().IsValidEntity(*m_entity)) {
            GetWorld().DestroyEntity(*m_entity);
        }
        m_entity.reset();
    }

private:
    std::optional<ECS::EntityID> m_entity;
};

class SceneGraphDemoScene final : public Scene {
public:
    std::string_view Name() const override { return "SceneGraphDemo"; }

    void OnAttach(AppContext& ctx, ModuleRegistry& modules) override {
        m_context = &ctx;
        m_moduleRegistry = &modules;

        auto& world = *ctx.world;
        world.RegisterComponent<ECS::TransformComponent>();
        world.RegisterComponent<ECS::MeshRenderComponent>();
        world.RegisterComponent<ECS::CameraComponent>();
        world.RegisterComponent<ECS::LightComponent>();

        if (!world.GetSystem<ECS::TransformSystem>()) world.RegisterSystem<ECS::TransformSystem>();
        if (!world.GetSystem<ECS::CameraSystem>()) world.RegisterSystem<ECS::CameraSystem>();
        if (ctx.renderer) {
            if (!world.GetSystem<ECS::MeshRenderSystem>()) world.RegisterSystem<ECS::MeshRenderSystem>(ctx.renderer);
            if (!world.GetSystem<ECS::LightSystem>()) world.RegisterSystem<ECS::LightSystem>(ctx.renderer);
            if (!world.GetSystem<ECS::UniformSystem>()) world.RegisterSystem<ECS::UniformSystem>(ctx.renderer);
        }
        world.PostInitialize();

        modules.ActivateModule("CoreRenderModule");
        modules.ActivateModule("InputModule");
        modules.ActivateModule("DebugHUDModule");

        auto root = std::make_shared<SceneNode>("SceneGraphDemo.Root");
        root->AddChild(std::make_shared<TestCubeNode>());
        root->AddChild(std::make_shared<TestPlaneNode>());
        root->AddChild(std::make_shared<TestCameraNode>());
        root->AddChild(std::make_shared<TestPointLightNode>(Vector3(3.0f, 4.0f, 2.0f), Color(1.0f, 0.6f, 0.3f)));
        root->AddChild(std::make_shared<TestPointLightNode>(Vector3(-2.0f, 3.5f, -3.0f), Color(0.3f, 0.8f, 1.0f)));

        m_graph.SetRoot(root);
        m_graph.Attach(*this, ctx);
    }

    void OnDetach(AppContext& ctx) override {
        m_graph.Detach();
        if (m_moduleRegistry) {
            m_moduleRegistry->DeactivateModule("DebugHUDModule");
            m_moduleRegistry->DeactivateModule("InputModule");
            m_moduleRegistry->DeactivateModule("CoreRenderModule");
        }
        m_context = nullptr;
        m_moduleRegistry = nullptr;
    }

    SceneResourceManifest BuildManifest() const override {
        return m_graph.BuildManifest();
    }

    void OnEnter(const SceneEnterArgs& args) override {
        m_graph.Enter(args);
    }

    void OnUpdate(const FrameUpdateArgs& frame) override {
        m_graph.Update(frame);
    }

    SceneSnapshot OnExit(const SceneExitArgs&) override {
        m_graph.Exit();
        SceneSnapshot snapshot;
        snapshot.sceneId = std::string(Name());
        return snapshot;
    }

private:
    AppContext* m_context = nullptr;
    ModuleRegistry* m_moduleRegistry = nullptr;
    SceneGraph m_graph;
};

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Logger::GetInstance().SetLogLevel(LogLevel::Debug);
    Logger::GetInstance().SetLogToConsole(false);
    Logger::GetInstance().SetLogToFile(false);

    Renderer renderer;
    if (!renderer.Initialize("SceneGraphDemoTest", 640, 480)) {
        std::cerr << "[scene_graph_demo_test] Renderer initialization failed." << std::endl;
        return 1;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    auto& asyncLoader = AsyncResourceLoader::GetInstance();
    asyncLoader.Initialize(2);

    ApplicationHost host;
    ApplicationHost::Config config;
    config.renderer = &renderer;
    config.resourceManager = &resourceManager;
    config.asyncLoader = &asyncLoader;
    config.uniformManager = nullptr;
    config.createWorldIfMissing = true;

    if (!host.Initialize(config)) {
        std::cerr << "[scene_graph_demo_test] ApplicationHost initialization failed." << std::endl;
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    auto& moduleRegistry = host.GetModuleRegistry();
    if (!moduleRegistry.RegisterModule(std::make_unique<CoreRenderModule>())) {
        std::cerr << "[scene_graph_demo_test] Failed to register CoreRenderModule." << std::endl;
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }
    moduleRegistry.RegisterModule(std::make_unique<InputModule>(), true);
    moduleRegistry.RegisterModule(std::make_unique<DebugHUDModule>(), true);

    host.RegisterSceneFactory("SceneGraphDemo", []() {
        return std::make_unique<SceneGraphDemoScene>();
    });

    if (!host.PushScene("SceneGraphDemo")) {
        std::cerr << "[scene_graph_demo_test] Failed to push SceneGraphDemo scene." << std::endl;
        host.Shutdown();
        asyncLoader.Shutdown();
        renderer.Shutdown();
        return 1;
    }

    double absoluteTime = 0.0;
    const float deltaTime = 0.016f;

    for (uint64_t frameIndex = 0; frameIndex < 10; ++frameIndex) {
        FrameUpdateArgs frame{};
        frame.deltaTime = deltaTime;
        frame.absoluteTime = absoluteTime;
        frame.frameIndex = frameIndex;

        renderer.BeginFrame();
        renderer.Clear();

        host.UpdateFrame(frame);
        host.UpdateWorld(deltaTime);

        renderer.FlushRenderQueue();
        renderer.EndFrame();
        renderer.Present();

        absoluteTime += deltaTime;
    }

    host.Shutdown();
    asyncLoader.Shutdown();
    renderer.Shutdown();

    std::cout << "[scene_graph_demo_test] SceneGraph demo executed successfully." << std::endl;
    return 0;
}


