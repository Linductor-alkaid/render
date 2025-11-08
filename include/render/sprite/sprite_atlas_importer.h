#pragma once

#include "render/sprite/sprite_atlas.h"
#include "render/ecs/components.h"
#include <optional>

namespace Render {

struct SpriteAtlasImportResult {
    SpriteAtlasPtr atlas;
    SpriteSheet spriteSheet;
    ECS::SpriteAnimationComponent animationComponent;
    std::string defaultAnimation;
    bool autoPlay = false;
};

/**
 * @brief 精灵图集导入器，解析 JSON 图集描述并生成渲染所需的数据
 */
class SpriteAtlasImporter {
public:
    /**
     * @brief 从 JSON 文件加载图集
     * @param filePath 图集描述文件路径
     * @param atlasName 可选的图集名称（为空则使用文件或 meta 中的名称）
     * @param error 输出错误信息
     * @return 成功时返回导入结果，失败返回 std::nullopt
     */
    static std::optional<SpriteAtlasImportResult> LoadFromFile(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);

    /**
     * @brief 加载图集并注册到 ResourceManager
     * @param filePath 图集描述文件路径
     * @param atlasName 图集名称
     * @param error 输出错误信息
     * @return 注册成功返回 true
     */
    static bool LoadAndRegister(
        const std::string& filePath,
        const std::string& atlasName,
        std::string& error);
};

} // namespace Render


