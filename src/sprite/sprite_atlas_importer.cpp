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
#include "render/sprite/sprite_atlas_importer.h"
#include "render/resource_manager.h"
#include "render/texture_loader.h"
#include "render/logger.h"
#include "render/error.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Render {

namespace {

Vector2 ParseVector2(const nlohmann::json& j, const Vector2& fallback = Vector2(0.0f, 0.0f)) {
    if (j.is_array() && j.size() >= 2) {
        return Vector2(j[0].get<float>(), j[1].get<float>());
    }
    if (j.is_object()) {
        return Vector2(j.value("x", fallback.x()), j.value("y", fallback.y()));
    }
    return fallback;
}

Rect ParseRect(const nlohmann::json& j) {
    if (j.is_array() && j.size() >= 4) {
        return Rect(j[0].get<float>(), j[1].get<float>(),
                    j[2].get<float>(), j[3].get<float>());
    }
    if (j.is_object()) {
        return Rect(j.value("x", 0.0f), j.value("y", 0.0f),
                    j.value("w", j.value("width", 0.0f)),
                    j.value("h", j.value("height", 0.0f)));
    }
    return Rect(0, 0, 0, 0);
}

SpritePlaybackMode ParsePlaybackMode(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "loop") {
        return SpritePlaybackMode::Loop;
    }
    if (lower == "once" || lower == "one" || lower == "single") {
        return SpritePlaybackMode::Once;
    }
    if (lower == "pingpong" || lower == "ping_pong" || lower == "pong") {
        return SpritePlaybackMode::PingPong;
    }
    HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                 "SpriteAtlasImporter: 未知的播放模式 '" + value + "', 使用 Loop"));
    return SpritePlaybackMode::Loop;
}

} // namespace

std::optional<SpriteAtlasImportResult> SpriteAtlasImporter::LoadFromFile(
    const std::string& filePath,
    const std::string& atlasName,
    std::string& error) {

    std::ifstream file(filePath);
    if (!file.is_open()) {
        error = "无法打开图集文件: " + filePath;
        return std::nullopt;
    }

    nlohmann::json jsonData;
    try {
        file >> jsonData;
    } catch (const std::exception& e) {
        error = "解析图集 JSON 失败: " + std::string(e.what());
        return std::nullopt;
    }

    auto result = SpriteAtlasImportResult{};
    result.atlas = CreateRef<SpriteAtlas>();

    // 解析 meta 信息
    const auto& meta = jsonData.contains("meta") ? jsonData["meta"] : nlohmann::json::object();
    const std::string metaName = meta.value("name", std::string{});
    const std::string resolvedName = !atlasName.empty() ? atlasName :
        (!metaName.empty() ? metaName : std::filesystem::path(filePath).stem().string());

    result.atlas->SetName(resolvedName);

    const std::string texturePath = meta.value("texture", std::string{});
    std::string textureName = meta.value("textureName", texturePath);
    if (textureName.empty()) {
        textureName = resolvedName + "_texture";
    }

    result.atlas->SetTextureName(textureName);

    auto& resourceManager = ResourceManager::GetInstance();
    Ref<Texture> texture = resourceManager.GetTexture(textureName);

    if (!texture && !texturePath.empty()) {
        texture = TextureLoader::GetInstance().LoadTexture(textureName, texturePath, true);
        if (texture) {
            resourceManager.RegisterTexture(textureName, texture);
            Logger::GetInstance().InfoFormat("[SpriteAtlasImporter] 纹理 '%s' 已加载并注册 (路径: %s)",
                                             textureName.c_str(), texturePath.c_str());
        }
    }

    if (!texture) {
        error = "无法获取图集所需的纹理: " + textureName;
        return std::nullopt;
    }

    result.atlas->SetTexture(texture);
    result.spriteSheet.SetTexture(texture);

    Vector2 textureSize(static_cast<float>(texture->GetWidth()),
                        static_cast<float>(texture->GetHeight()));
    if (meta.contains("size")) {
        const auto sizeObj = meta["size"];
        const float width = sizeObj.value("w", sizeObj.value("width", textureSize.x()));
        const float height = sizeObj.value("h", sizeObj.value("height", textureSize.y()));
        if (width > 0.0f && height > 0.0f) {
            textureSize = Vector2(width, height);
        }
    }
    result.atlas->SetTextureSize(static_cast<int>(textureSize.x()), static_cast<int>(textureSize.y()));

    const float defaultFrameDuration = meta.value("defaultFrameDuration", 0.1f);
    result.defaultAnimation = meta.value("defaultAnimation", std::string{});
    result.autoPlay = meta.value("autoPlay", false);

    // 解析帧
    if (!jsonData.contains("frames") || !jsonData["frames"].is_object()) {
        error = "图集 JSON 缺少 'frames' 定义";
        return std::nullopt;
    }

    const auto& framesJson = jsonData["frames"];
    for (const auto& [frameName, frameValue] : framesJson.items()) {
        SpriteAtlasFrame atlasFrame;

        if (frameValue.contains("frame")) {
            atlasFrame.uv = ParseRect(frameValue["frame"]);
        } else if (frameValue.contains("uv")) {
            atlasFrame.uv = ParseRect(frameValue["uv"]);
        } else {
            atlasFrame.uv = Rect(
                frameValue.value("x", 0.0f),
                frameValue.value("y", 0.0f),
                frameValue.value("w", frameValue.value("width", 0.0f)),
                frameValue.value("h", frameValue.value("height", 0.0f)));
        }

        const Vector2 sizeFallback(atlasFrame.uv.width, atlasFrame.uv.height);
        if (frameValue.contains("sourceSize")) {
            atlasFrame.size = ParseVector2(frameValue["sourceSize"], sizeFallback);
            atlasFrame.originalSize = ParseVector2(frameValue["sourceSize"], sizeFallback);
        } else if (frameValue.contains("size")) {
            atlasFrame.size = ParseVector2(frameValue["size"], sizeFallback);
            atlasFrame.originalSize = ParseVector2(frameValue["size"], sizeFallback);
        } else {
            atlasFrame.size = sizeFallback;
            atlasFrame.originalSize = sizeFallback;
        }

        if (frameValue.contains("spriteSourceSize")) {
            atlasFrame.offset = ParseVector2(frameValue["spriteSourceSize"], Vector2(0.0f, 0.0f));
        } else {
            atlasFrame.offset = Vector2(0.0f, 0.0f);
        }

        if (frameValue.contains("pivot")) {
            atlasFrame.pivot = ParseVector2(frameValue["pivot"], Vector2(0.5f, 0.5f));
        } else {
            atlasFrame.pivot = Vector2(0.5f, 0.5f);
        }
        atlasFrame.duration = frameValue.value("duration", 0.0f);

        // 将像素转换为 SpriteFrame
        SpriteFrame spriteFrame;
        spriteFrame.uv = atlasFrame.uv;
        spriteFrame.size = atlasFrame.size;
        spriteFrame.pivot = atlasFrame.pivot;

        result.atlas->AddFrame(frameName, atlasFrame);
        result.spriteSheet.AddFrame(frameName, spriteFrame);
    }

    // 解析动画
    if (jsonData.contains("animations") && jsonData["animations"].is_object()) {
        const auto& animationsJson = jsonData["animations"];
        for (const auto& [animName, animValue] : animationsJson.items()) {
            if (!animValue.contains("frames") || !animValue["frames"].is_array()) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                    "SpriteAtlasImporter: 动画 '" + animName + "' 缺少 frames 数组"));
                continue;
            }

            SpriteAtlasAnimation animation;
            animation.frameDuration = animValue.value("frameDuration", defaultFrameDuration);
            animation.playbackSpeed = animValue.value("playbackSpeed", 1.0f);

            const std::string playback = animValue.value("playback", std::string("loop"));
            animation.playbackMode = ParsePlaybackMode(playback);

            for (const auto& frameEntry : animValue["frames"]) {
                if (frameEntry.is_string()) {
                    animation.frames.push_back(frameEntry.get<std::string>());
                } else if (frameEntry.is_object() && frameEntry.contains("name")) {
                    animation.frames.push_back(frameEntry["name"].get<std::string>());
                }
            }

            if (animation.frames.empty()) {
                HANDLE_ERROR(RENDER_WARNING(ErrorCode::InvalidArgument,
                    "SpriteAtlasImporter: 动画 '" + animName + "' 没有有效的帧"));
                continue;
            }

            result.atlas->AddAnimation(animName, animation);
        }
    }

    result.atlas->PopulateAnimationComponent(result.animationComponent,
                                             result.defaultAnimation,
                                             result.autoPlay);

    return result;
}

bool SpriteAtlasImporter::LoadAndRegister(
    const std::string& filePath,
    const std::string& atlasName,
    std::string& error) {

    auto resultOpt = LoadFromFile(filePath, atlasName, error);
    if (!resultOpt.has_value()) {
        return false;
    }

    auto& resourceManager = ResourceManager::GetInstance();
    const auto& result = resultOpt.value();

    if (!resourceManager.RegisterSpriteAtlas(result.atlas->GetName(), result.atlas)) {
        error = "SpriteAtlasImporter: 注册图集失败（可能名称重复）: " + result.atlas->GetName();
        return false;
    }

    Logger::GetInstance().InfoFormat("[SpriteAtlasImporter] 图集 '%s' 已导入并注册 (文件: %s)",
                                     result.atlas->GetName().c_str(), filePath.c_str());
    return true;
}

} // namespace Render


