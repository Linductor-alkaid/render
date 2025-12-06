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
#pragma once

#include <nlohmann/json.hpp>
#include "render/types.h"
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <string>
#include <fstream>

/**
 * @file json_serializer.h
 * @brief 通用的JSON序列化/反序列化工具
 * 
 * 该工具类提供了通用的JSON序列化和反序列化功能，可以被UI主题、场景、配置等多个模块复用。
 * 
 * 设计原则：
 * 1. 使用nlohmann/json库作为底层实现
 * 2. 为常用的基础类型（Color、Vector2、Vector3等）提供序列化支持
 * 3. 提供统一的错误处理和日志记录
 * 4. 易于扩展，支持自定义类型的序列化
 * 
 * 使用示例：
 * @code
 * // 序列化
 * MyData data;
 * nlohmann::json j = data; // 使用自定义的to_json函数
 * JsonSerializer::SaveToFile(j, "data.json");
 * 
 * // 反序列化
 * nlohmann::json j;
 * if (JsonSerializer::LoadFromFile("data.json", j)) {
 *     MyData data = j.get<MyData>(); // 使用自定义的from_json函数
 * }
 * @endcode
 */

namespace Render {

/**
 * @brief JSON序列化工具类
 */
class JsonSerializer {
public:
    /**
     * @brief 从文件加载JSON
     * @param filepath JSON文件路径
     * @param outJson 输出的JSON对象
     * @return 是否成功加载
     */
    static bool LoadFromFile(const std::string& filepath, nlohmann::json& outJson);
    
    /**
     * @brief 保存JSON到文件
     * @param json JSON对象
     * @param filepath 文件路径
     * @param indent 缩进空格数（默认4，用于格式化输出）
     * @return 是否成功保存
     */
    static bool SaveToFile(const nlohmann::json& json, const std::string& filepath, int indent = 4);
    
    /**
     * @brief 从字符串解析JSON
     * @param jsonStr JSON字符串
     * @param outJson 输出的JSON对象
     * @return 是否成功解析
     */
    static bool ParseFromString(const std::string& jsonStr, nlohmann::json& outJson);
    
    /**
     * @brief 将JSON转换为字符串
     * @param json JSON对象
     * @param indent 缩进空格数
     * @return JSON字符串
     */
    static std::string ToString(const nlohmann::json& json, int indent = 4);
};

// ============================================================================
// 基础类型的JSON序列化支持
// ============================================================================

/**
 * @brief Color 转 JSON
 */
inline void to_json(nlohmann::json& j, const Color& color) {
    j = nlohmann::json::array({color.r, color.g, color.b, color.a});
}

/**
 * @brief JSON 转 Color
 */
inline void from_json(const nlohmann::json& j, Color& color) {
    if (j.is_array() && j.size() >= 3) {
        color.r = j[0].get<float>();
        color.g = j[1].get<float>();
        color.b = j[2].get<float>();
        color.a = (j.size() >= 4) ? j[3].get<float>() : 1.0f;
    } else if (j.is_object()) {
        // 也支持对象格式：{"r": 1.0, "g": 0.5, "b": 0.0, "a": 1.0}
        color.r = j.value("r", 1.0f);
        color.g = j.value("g", 1.0f);
        color.b = j.value("b", 1.0f);
        color.a = j.value("a", 1.0f);
    }
}

/**
 * @brief Vector2 转 JSON
 */
inline void to_json(nlohmann::json& j, const Vector2& vec) {
    j = nlohmann::json::array({vec.x(), vec.y()});
}

/**
 * @brief JSON 转 Vector2
 */
inline void from_json(const nlohmann::json& j, Vector2& vec) {
    if (j.is_array() && j.size() >= 2) {
        vec.x() = j[0].get<float>();
        vec.y() = j[1].get<float>();
    } else if (j.is_object()) {
        vec.x() = j.value("x", 0.0f);
        vec.y() = j.value("y", 0.0f);
    }
}

/**
 * @brief Vector3 转 JSON
 */
inline void to_json(nlohmann::json& j, const Vector3& vec) {
    j = nlohmann::json::array({vec.x(), vec.y(), vec.z()});
}

/**
 * @brief JSON 转 Vector3
 */
inline void from_json(const nlohmann::json& j, Vector3& vec) {
    if (j.is_array() && j.size() >= 3) {
        vec.x() = j[0].get<float>();
        vec.y() = j[1].get<float>();
        vec.z() = j[2].get<float>();
    } else if (j.is_object()) {
        vec.x() = j.value("x", 0.0f);
        vec.y() = j.value("y", 0.0f);
        vec.z() = j.value("z", 0.0f);
    }
}

/**
 * @brief Vector4 转 JSON
 */
inline void to_json(nlohmann::json& j, const Vector4& vec) {
    j = nlohmann::json::array({vec.x(), vec.y(), vec.z(), vec.w()});
}

/**
 * @brief JSON 转 Vector4
 */
inline void from_json(const nlohmann::json& j, Vector4& vec) {
    if (j.is_array() && j.size() >= 4) {
        vec.x() = j[0].get<float>();
        vec.y() = j[1].get<float>();
        vec.z() = j[2].get<float>();
        vec.w() = j[3].get<float>();
    } else if (j.is_object()) {
        vec.x() = j.value("x", 0.0f);
        vec.y() = j.value("y", 0.0f);
        vec.z() = j.value("z", 0.0f);
        vec.w() = j.value("w", 0.0f);
    }
}

/**
 * @brief Quaternion 转 JSON
 */
inline void to_json(nlohmann::json& j, const Quaternion& quat) {
    j = nlohmann::json::array({quat.x(), quat.y(), quat.z(), quat.w()});
}

/**
 * @brief JSON 转 Quaternion
 */
inline void from_json(const nlohmann::json& j, Quaternion& quat) {
    if (j.is_array() && j.size() >= 4) {
        quat.x() = j[0].get<float>();
        quat.y() = j[1].get<float>();
        quat.z() = j[2].get<float>();
        quat.w() = j[3].get<float>();
    } else if (j.is_object()) {
        quat.x() = j.value("x", 0.0f);
        quat.y() = j.value("y", 0.0f);
        quat.z() = j.value("z", 0.0f);
        quat.w() = j.value("w", 1.0f);
    }
}

/**
 * @brief Rect 转 JSON
 */
inline void to_json(nlohmann::json& j, const Rect& rect) {
    j = {
        {"x", rect.x},
        {"y", rect.y},
        {"width", rect.width},
        {"height", rect.height}
    };
}

/**
 * @brief JSON 转 Rect
 */
inline void from_json(const nlohmann::json& j, Rect& rect) {
    rect.x = j.value("x", 0.0f);
    rect.y = j.value("y", 0.0f);
    rect.width = j.value("width", 0.0f);
    rect.height = j.value("height", 0.0f);
}

} // namespace Render


