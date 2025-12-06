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

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace Render {
namespace ECS {

/**
 * @brief 实体 ID 类型（64位：32位索引 + 32位版本号）
 * 
 * 使用版本号机制防止悬空引用（当实体被删除后，版本号递增）
 */
struct EntityID {
    uint32_t index;      ///< 实体索引
    uint32_t version;    ///< 版本号（用于检测悬空引用）
    
    /// 检查实体 ID 是否有效
    [[nodiscard]] bool IsValid() const { 
        return index != INVALID_INDEX; 
    }
    
    /// 比较运算符
    bool operator==(const EntityID& other) const {
        return index == other.index && version == other.version;
    }
    
    bool operator!=(const EntityID& other) const {
        return !(*this == other);
    }
    
    bool operator<(const EntityID& other) const {
        if (index != other.index) {
            return index < other.index;
        }
        return version < other.version;
    }
    
    /// 哈希支持（用于 std::unordered_map）
    struct Hash {
        size_t operator()(const EntityID& id) const {
            return std::hash<uint64_t>()(
                (static_cast<uint64_t>(id.index) << 32) | id.version
            );
        }
    };
    
    /// 无效索引常量
    static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;
    
    /// 创建无效实体 ID
    static EntityID Invalid() {
        return EntityID{ INVALID_INDEX, 0 };
    }
};

/**
 * @brief 实体描述符（用于创建实体）
 */
struct EntityDescriptor {
    std::string name;                    ///< 实体名称
    bool active = true;                  ///< 是否激活
    std::vector<std::string> tags;       ///< 标签列表
};

} // namespace ECS
} // namespace Render

