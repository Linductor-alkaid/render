#include "render/application/scene_serializer.h"

#include <fstream>
#include <filesystem>

#include "render/logger.h"
#include "render/ecs/components.h"
#include "render/ecs/entity.h"
#include "render/resource_manager.h"
#include "render/camera.h"
#include "render/lighting/light.h"

namespace Render::Application {

bool SceneSerializer::SaveScene(ECS::World& world, const std::string& sceneName, const std::string& filePath) {
    try {
        nlohmann::json sceneJson;
        sceneJson["version"] = "1.0";
        sceneJson["sceneName"] = sceneName;
        sceneJson["entities"] = nlohmann::json::array();

        // 获取所有实体
        auto allEntities = world.GetEntityManager().GetAllEntities();
        
        for (const auto& entity : allEntities) {
            if (!world.IsValidEntity(entity)) {
                continue;
            }

            nlohmann::json entityJson;
            entityJson["id"] = entity.index;
            entityJson["version"] = entity.version;
            entityJson["components"] = nlohmann::json::object();

            // 序列化实体
            SerializeEntity(world, entity, entityJson);

            sceneJson["entities"].push_back(entityJson);
        }

        // 保存到文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            Logger::GetInstance().ErrorFormat("[SceneSerializer] Failed to open file for writing: %s", filePath.c_str());
            return false;
        }

        file << sceneJson.dump(4);
        file.close();

        Logger::GetInstance().InfoFormat(
            "[SceneSerializer] Scene '%s' saved to '%s' (%zu entities)",
            sceneName.c_str(), filePath.c_str(), allEntities.size());

        return true;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[SceneSerializer] Exception while saving scene: %s", e.what());
        return false;
    }
}

std::optional<std::string> SceneSerializer::LoadScene(ECS::World& world, const std::string& filePath, AppContext& ctx) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            Logger::GetInstance().ErrorFormat("[SceneSerializer] Failed to open file for reading: %s", filePath.c_str());
            return std::nullopt;
        }

        nlohmann::json sceneJson;
        file >> sceneJson;
        file.close();

        // 验证版本
        if (!sceneJson.contains("version") || !sceneJson.contains("sceneName")) {
            Logger::GetInstance().Error("[SceneSerializer] Invalid scene file format: missing version or sceneName");
            return std::nullopt;
        }

        std::string sceneName = sceneJson["sceneName"].get<std::string>();

        // 确保组件已注册
        world.RegisterComponent<ECS::TransformComponent>();
        world.RegisterComponent<ECS::MeshRenderComponent>();
        world.RegisterComponent<ECS::CameraComponent>();
        world.RegisterComponent<ECS::LightComponent>();
        world.RegisterComponent<ECS::NameComponent>();

        // 加载实体
        if (sceneJson.contains("entities") && sceneJson["entities"].is_array()) {
            for (const auto& entityJson : sceneJson["entities"]) {
                DeserializeEntity(world, entityJson, ctx);
            }
        }

        Logger::GetInstance().InfoFormat(
            "[SceneSerializer] Scene '%s' loaded from '%s'",
            sceneName.c_str(), filePath.c_str());

        return sceneName;
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[SceneSerializer] Exception while loading scene: %s", e.what());
        return std::nullopt;
    }
}

bool SceneSerializer::ValidateResourcePath(const std::string& resourcePath, const std::string& resourceType, AppContext& ctx) {
    if (resourcePath.empty()) {
        return false;
    }

    // 检查文件是否存在
    if (std::filesystem::exists(resourcePath)) {
        return true;
    }

    // 检查资源管理器
    if (ctx.resourceManager) {
        if (resourceType == "mesh" && ctx.resourceManager->HasMesh(resourcePath)) {
            return true;
        }
        if (resourceType == "material" && ctx.resourceManager->HasMaterial(resourcePath)) {
            return true;
        }
        if (resourceType == "texture" && ctx.resourceManager->HasTexture(resourcePath)) {
            return true;
        }
        if (resourceType == "shader" && ctx.resourceManager->HasShader(resourcePath)) {
            return true;
        }
    }

    Logger::GetInstance().WarningFormat(
        "[SceneSerializer] Resource path validation failed: type=%s, path=%s",
        resourceType.c_str(), resourcePath.c_str());

    return false;
}

void SceneSerializer::SerializeEntity(ECS::World& world, ECS::EntityID entity, nlohmann::json& jsonObj) {
    auto& components = jsonObj["components"];

    // 序列化NameComponent
    if (world.HasComponent<ECS::NameComponent>(entity)) {
        const auto& comp = world.GetComponent<ECS::NameComponent>(entity);
        SerializeNameComponent(comp, components);
    }

    // 序列化TransformComponent
    if (world.HasComponent<ECS::TransformComponent>(entity)) {
        const auto& comp = world.GetComponent<ECS::TransformComponent>(entity);
        SerializeTransformComponent(comp, components);
    }

    // 序列化MeshRenderComponent
    if (world.HasComponent<ECS::MeshRenderComponent>(entity)) {
        const auto& comp = world.GetComponent<ECS::MeshRenderComponent>(entity);
        SerializeMeshRenderComponent(comp, components);
    }

    // 序列化CameraComponent
    if (world.HasComponent<ECS::CameraComponent>(entity)) {
        const auto& comp = world.GetComponent<ECS::CameraComponent>(entity);
        SerializeCameraComponent(comp, components);
    }

    // 序列化LightComponent
    if (world.HasComponent<ECS::LightComponent>(entity)) {
        const auto& comp = world.GetComponent<ECS::LightComponent>(entity);
        SerializeLightComponent(comp, components);
    }
}

ECS::EntityID SceneSerializer::DeserializeEntity(ECS::World& world, const nlohmann::json& jsonObj, AppContext& ctx) {
    // 创建新实体
    ECS::EntityDescriptor desc;
    if (jsonObj.contains("name")) {
        desc.name = jsonObj["name"].get<std::string>();
    }
    
    ECS::EntityID entity = world.CreateEntity(desc);

    // 反序列化组件
    if (jsonObj.contains("components")) {
        const auto& components = jsonObj["components"];

        // 反序列化NameComponent
        if (components.contains("name")) {
            DeserializeNameComponent(world, entity, components["name"]);
        }

        // 反序列化TransformComponent
        if (components.contains("transform")) {
            DeserializeTransformComponent(world, entity, components["transform"]);
        }

        // 反序列化MeshRenderComponent
        if (components.contains("meshRender")) {
            DeserializeMeshRenderComponent(world, entity, components["meshRender"], ctx);
        }

        // 反序列化CameraComponent
        if (components.contains("camera")) {
            DeserializeCameraComponent(world, entity, components["camera"], ctx);
        }

        // 反序列化LightComponent
        if (components.contains("light")) {
            DeserializeLightComponent(world, entity, components["light"]);
        }
    }

    return entity;
}

void SceneSerializer::SerializeTransformComponent(const ECS::TransformComponent& comp, nlohmann::json& jsonObj) {
    nlohmann::json transformJson;
    
    if (comp.transform) {
        auto pos = comp.GetPosition();
        auto rot = comp.GetRotation();
        auto scale = comp.GetScale();

        transformJson["position"] = {pos.x(), pos.y(), pos.z()};
        transformJson["rotation"] = {rot.w(), rot.x(), rot.y(), rot.z()};
        transformJson["scale"] = {scale.x(), scale.y(), scale.z()};

        if (comp.parentEntity.IsValid()) {
            transformJson["parentEntity"] = comp.parentEntity.index;
        }
    }

    jsonObj["transform"] = transformJson;
}

void SceneSerializer::DeserializeTransformComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj) {
    ECS::TransformComponent transform;

    if (jsonObj.contains("position") && jsonObj["position"].is_array() && jsonObj["position"].size() >= 3) {
        auto pos = jsonObj["position"];
        transform.SetPosition(Vector3(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>()));
    }

    if (jsonObj.contains("rotation") && jsonObj["rotation"].is_array() && jsonObj["rotation"].size() >= 4) {
        auto rot = jsonObj["rotation"];
        transform.SetRotation(Quaternion(rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>(), rot[3].get<float>()));
    }

    if (jsonObj.contains("scale") && jsonObj["scale"].is_array() && jsonObj["scale"].size() >= 3) {
        auto scale = jsonObj["scale"];
        transform.SetScale(Vector3(scale[0].get<float>(), scale[1].get<float>(), scale[2].get<float>()));
    }

    if (jsonObj.contains("parentEntity")) {
        uint32_t parentIndex = jsonObj["parentEntity"].get<uint32_t>();
        // 注意：这里需要根据实际保存的实体ID来查找父实体
        // 简化实现：暂时不处理父子关系，需要改进
    }

    world.AddComponent(entity, transform);
}

void SceneSerializer::SerializeMeshRenderComponent(const ECS::MeshRenderComponent& comp, nlohmann::json& jsonObj) {
    nlohmann::json meshJson;
    
    meshJson["meshName"] = comp.meshName;
    meshJson["materialName"] = comp.materialName;
    if (!comp.shaderName.empty()) {
        meshJson["shaderName"] = comp.shaderName;
    }
    if (!comp.shaderVertPath.empty()) {
        meshJson["shaderVertPath"] = comp.shaderVertPath;
    }
    if (!comp.shaderFragPath.empty()) {
        meshJson["shaderFragPath"] = comp.shaderFragPath;
    }

    meshJson["visible"] = comp.visible;
    meshJson["castShadows"] = comp.castShadows;
    meshJson["receiveShadows"] = comp.receiveShadows;
    meshJson["layerID"] = comp.layerID;
    meshJson["renderPriority"] = comp.renderPriority;

    // 材质覆盖
    if (comp.materialOverride.diffuseColor.has_value()) {
        const auto& c = comp.materialOverride.diffuseColor.value();
        meshJson["materialOverride"]["diffuseColor"] = {c.r, c.g, c.b, c.a};
    }
    if (comp.materialOverride.specularColor.has_value()) {
        const auto& c = comp.materialOverride.specularColor.value();
        meshJson["materialOverride"]["specularColor"] = {c.r, c.g, c.b, c.a};
    }
    if (comp.materialOverride.shininess.has_value()) {
        meshJson["materialOverride"]["shininess"] = comp.materialOverride.shininess.value();
    }

    jsonObj["meshRender"] = meshJson;
}

void SceneSerializer::DeserializeMeshRenderComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj, AppContext& ctx) {
    ECS::MeshRenderComponent meshComp;

    if (jsonObj.contains("meshName")) {
        meshComp.meshName = jsonObj["meshName"].get<std::string>();
        // 从ResourceManager获取mesh
        if (ctx.resourceManager && ctx.resourceManager->HasMesh(meshComp.meshName)) {
            meshComp.mesh = ctx.resourceManager->GetMesh(meshComp.meshName);
        } else {
            Logger::GetInstance().WarningFormat(
                "[SceneSerializer] Mesh resource not found: %s", meshComp.meshName.c_str());
        }
    }

    if (jsonObj.contains("materialName")) {
        meshComp.materialName = jsonObj["materialName"].get<std::string>();
        // 从ResourceManager获取material
        if (ctx.resourceManager && ctx.resourceManager->HasMaterial(meshComp.materialName)) {
            meshComp.material = ctx.resourceManager->GetMaterial(meshComp.materialName);
        } else {
            Logger::GetInstance().WarningFormat(
                "[SceneSerializer] Material resource not found: %s", meshComp.materialName.c_str());
        }
    }
    
    // 设置资源加载状态
    meshComp.resourcesLoaded = (meshComp.mesh != nullptr && meshComp.material != nullptr);
    meshComp.asyncLoading = false;

    if (jsonObj.contains("shaderName")) {
        meshComp.shaderName = jsonObj["shaderName"].get<std::string>();
    }

    if (jsonObj.contains("shaderVertPath")) {
        meshComp.shaderVertPath = jsonObj["shaderVertPath"].get<std::string>();
    }

    if (jsonObj.contains("shaderFragPath")) {
        meshComp.shaderFragPath = jsonObj["shaderFragPath"].get<std::string>();
    }

    if (jsonObj.contains("visible")) {
        meshComp.visible = jsonObj["visible"].get<bool>();
    }

    if (jsonObj.contains("castShadows")) {
        meshComp.castShadows = jsonObj["castShadows"].get<bool>();
    }

    if (jsonObj.contains("receiveShadows")) {
        meshComp.receiveShadows = jsonObj["receiveShadows"].get<bool>();
    }

    if (jsonObj.contains("layerID")) {
        meshComp.layerID = jsonObj["layerID"].get<uint32_t>();
    }

    if (jsonObj.contains("renderPriority")) {
        meshComp.renderPriority = jsonObj["renderPriority"].get<int32_t>();
    }

    // 材质覆盖
    if (jsonObj.contains("materialOverride")) {
        const auto& override = jsonObj["materialOverride"];
        if (override.contains("diffuseColor") && override["diffuseColor"].is_array() && override["diffuseColor"].size() >= 4) {
            const auto& c = override["diffuseColor"];
            meshComp.materialOverride.diffuseColor = Color(
                c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
        }
        if (override.contains("specularColor") && override["specularColor"].is_array() && override["specularColor"].size() >= 4) {
            const auto& c = override["specularColor"];
            meshComp.materialOverride.specularColor = Color(
                c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
        }
        if (override.contains("shininess")) {
            meshComp.materialOverride.shininess = override["shininess"].get<float>();
        }
    }

    world.AddComponent(entity, meshComp);
}

void SceneSerializer::SerializeCameraComponent(const ECS::CameraComponent& comp, nlohmann::json& jsonObj) {
    nlohmann::json cameraJson;

    if (comp.camera) {
        cameraJson["active"] = comp.active;
        cameraJson["layerMask"] = comp.layerMask;
        cameraJson["depth"] = comp.depth;
        cameraJson["clearColor"] = {comp.clearColor.r, comp.clearColor.g, comp.clearColor.b, comp.clearColor.a};
        cameraJson["clearDepth"] = comp.clearDepth;
        cameraJson["clearStencil"] = comp.clearStencil;

        // 序列化相机参数
        auto projType = comp.camera->GetProjectionType();
        cameraJson["projectionType"] = (projType == ProjectionType::Perspective) ? "perspective" : "orthographic";

        if (projType == ProjectionType::Perspective) {
            cameraJson["fov"] = comp.camera->GetFieldOfView();
            cameraJson["aspect"] = comp.camera->GetAspectRatio();
            cameraJson["nearPlane"] = comp.camera->GetNearPlane();
            cameraJson["farPlane"] = comp.camera->GetFarPlane();
        } else {
            // 正交投影：使用宽高比计算左右上下边界
            float aspect = comp.camera->GetAspectRatio();
            float height = 10.0f; // 默认高度，可以从相机获取或使用默认值
            float width = height * aspect;
            cameraJson["orthoWidth"] = width;
            cameraJson["orthoHeight"] = height;
            cameraJson["nearPlane"] = comp.camera->GetNearPlane();
            cameraJson["farPlane"] = comp.camera->GetFarPlane();
        }
    }

    jsonObj["camera"] = cameraJson;
}

void SceneSerializer::DeserializeCameraComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj, AppContext& ctx) {
    ECS::CameraComponent cameraComp;

    auto camera = std::make_shared<Camera>();

    if (jsonObj.contains("projectionType")) {
        std::string projType = jsonObj["projectionType"].get<std::string>();
        
        if (projType == "perspective") {
            float fov = jsonObj.value("fov", 60.0f);
            float aspect = jsonObj.value("aspect", 16.0f / 9.0f);
            float nearPlane = jsonObj.value("nearPlane", 0.1f);
            float farPlane = jsonObj.value("farPlane", 1000.0f);
            camera->SetPerspective(fov, aspect, nearPlane, farPlane);
        } else {
            // 正交投影：使用宽高
            float width = jsonObj.value("orthoWidth", 10.0f);
            float height = jsonObj.value("orthoHeight", 10.0f);
            float nearPlane = jsonObj.value("nearPlane", 0.1f);
            float farPlane = jsonObj.value("farPlane", 1000.0f);
            camera->SetOrthographic(width, height, nearPlane, farPlane);
        }
    } else {
        // 默认透视投影
        camera->SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    }

    cameraComp.camera = camera;
    cameraComp.active = jsonObj.value("active", true);
    cameraComp.layerMask = jsonObj.value("layerMask", 0xFFFFFFFFu);
    cameraComp.depth = jsonObj.value("depth", 0);
    
    if (jsonObj.contains("clearColor") && jsonObj["clearColor"].is_array() && jsonObj["clearColor"].size() >= 4) {
        const auto& c = jsonObj["clearColor"];
        cameraComp.clearColor = Color(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>());
    }

    cameraComp.clearDepth = jsonObj.value("clearDepth", true);
    cameraComp.clearStencil = jsonObj.value("clearStencil", false);

    world.AddComponent(entity, cameraComp);
}

void SceneSerializer::SerializeLightComponent(const ECS::LightComponent& comp, nlohmann::json& jsonObj) {
    nlohmann::json lightJson;

    lightJson["type"] = static_cast<int>(static_cast<int>(comp.type));
    lightJson["color"] = {comp.color.r, comp.color.g, comp.color.b};
    lightJson["intensity"] = comp.intensity;
    lightJson["range"] = comp.range;
    lightJson["attenuation"] = comp.attenuation;
    lightJson["innerConeAngle"] = comp.innerConeAngle;
    lightJson["outerConeAngle"] = comp.outerConeAngle;
    lightJson["castShadows"] = comp.castShadows;
    lightJson["shadowMapSize"] = comp.shadowMapSize;
    lightJson["shadowBias"] = comp.shadowBias;
    lightJson["enabled"] = comp.enabled;

    jsonObj["light"] = lightJson;
}

void SceneSerializer::DeserializeLightComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj) {
    ECS::LightComponent lightComp;

    if (jsonObj.contains("type")) {
        lightComp.type = static_cast<ECS::LightType>(jsonObj["type"].get<int>());
    }

    if (jsonObj.contains("color") && jsonObj["color"].is_array() && jsonObj["color"].size() >= 3) {
        const auto& c = jsonObj["color"];
        lightComp.color = Color(c[0].get<float>(), c[1].get<float>(), c[2].get<float>());
    }

    lightComp.intensity = jsonObj.value("intensity", 1.0f);
    lightComp.range = jsonObj.value("range", 10.0f);
    lightComp.attenuation = jsonObj.value("attenuation", 1.0f);
    lightComp.innerConeAngle = jsonObj.value("innerConeAngle", 30.0f);
    lightComp.outerConeAngle = jsonObj.value("outerConeAngle", 45.0f);
    lightComp.castShadows = jsonObj.value("castShadows", false);
    lightComp.shadowMapSize = jsonObj.value("shadowMapSize", 1024u);
    lightComp.shadowBias = jsonObj.value("shadowBias", 0.001f);
    lightComp.enabled = jsonObj.value("enabled", true);

    world.AddComponent(entity, lightComp);
}

void SceneSerializer::SerializeNameComponent(const ECS::NameComponent& comp, nlohmann::json& jsonObj) {
    jsonObj["name"] = comp.name;
}

void SceneSerializer::DeserializeNameComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj) {
    ECS::NameComponent nameComp;
    
    if (jsonObj.is_string()) {
        nameComp.name = jsonObj.get<std::string>();
    } else if (jsonObj.is_object() && jsonObj.contains("name")) {
        nameComp.name = jsonObj["name"].get<std::string>();
    }

    world.AddComponent(entity, nameComp);
}

} // namespace Render::Application

