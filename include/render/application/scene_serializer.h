#pragma once

#include <string>
#include <optional>
#include <fstream>

#include <nlohmann/json.hpp>

#include "render/application/app_context.h"
#include "render/application/scene_types.h"
#include "render/ecs/world.h"
#include "render/ecs/components.h"

namespace Render {
namespace Application {

/**
 * @brief 场景序列化器
 * 
 * 负责将场景序列化为JSON格式，以及从JSON反序列化场景
 * 支持ECS实体、组件和SceneGraph节点的序列化
 */
class SceneSerializer {
public:
    SceneSerializer() = default;
    ~SceneSerializer() = default;

    /**
     * @brief 保存场景到JSON文件
     * @param world ECS World对象
     * @param sceneName 场景名称
     * @param filePath 保存路径
     * @return 成功返回true，失败返回false
     */
    bool SaveScene(ECS::World& world, const std::string& sceneName, const std::string& filePath);

    /**
     * @brief 从JSON文件加载场景
     * @param world ECS World对象
     * @param filePath 文件路径
     * @param ctx AppContext（用于资源路径校验）
     * @return 成功返回场景名称，失败返回空
     */
    std::optional<std::string> LoadScene(ECS::World& world, const std::string& filePath, AppContext& ctx);

    /**
     * @brief 验证资源路径
     * @param resourcePath 资源路径
     * @param resourceType 资源类型（mesh, material, texture, shader等）
     * @param ctx AppContext
     * @return 如果资源路径有效返回true
     */
    static bool ValidateResourcePath(const std::string& resourcePath, const std::string& resourceType, AppContext& ctx);

private:
    /**
     * @brief 序列化ECS实体
     */
    void SerializeEntity(ECS::World& world, ECS::EntityID entity, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化ECS实体
     */
    ECS::EntityID DeserializeEntity(ECS::World& world, const nlohmann::json& jsonObj, AppContext& ctx);

    /**
     * @brief 序列化TransformComponent
     */
    void SerializeTransformComponent(const ECS::TransformComponent& comp, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化TransformComponent
     */
    void DeserializeTransformComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj);

    /**
     * @brief 序列化MeshRenderComponent
     */
    void SerializeMeshRenderComponent(const ECS::MeshRenderComponent& comp, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化MeshRenderComponent
     */
    void DeserializeMeshRenderComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj, AppContext& ctx);

    /**
     * @brief 序列化CameraComponent
     */
    void SerializeCameraComponent(const ECS::CameraComponent& comp, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化CameraComponent
     */
    void DeserializeCameraComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj, AppContext& ctx);

    /**
     * @brief 序列化LightComponent
     */
    void SerializeLightComponent(const ECS::LightComponent& comp, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化LightComponent
     */
    void DeserializeLightComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj);

    /**
     * @brief 序列化NameComponent
     */
    void SerializeNameComponent(const ECS::NameComponent& comp, nlohmann::json& jsonObj);

    /**
     * @brief 反序列化NameComponent
     */
    void DeserializeNameComponent(ECS::World& world, ECS::EntityID entity, const nlohmann::json& jsonObj);
};

} // namespace Application
} // namespace Render

