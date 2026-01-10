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

#include "render/types.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace Render {
namespace ECS {

/**
 * @brief 物理材质
 * 
 * 定义物理体的材质属性，如摩擦系数、弹性系数等
 */
struct PhysicsMaterial {
    std::string name;              ///< 材质名称
    
    float friction = 0.5f;         ///< 摩擦系数 [0, 1]
    float restitution = 0.0f;      ///< 弹性系数（反弹系数）[0, 1]
    float density = 1000.0f;       ///< 密度（kg/m³），用于计算质量
    
    PhysicsMaterial() = default;
    PhysicsMaterial(const std::string& n, float f, float r, float d)
        : name(n), friction(f), restitution(r), density(d) {}
};

/**
 * @brief 物理材质管理器
 * 
 * 管理物理材质的加载和查找
 */
class PhysicsMaterialManager {
public:
    PhysicsMaterialManager() = default;
    ~PhysicsMaterialManager() = default;
    
    /**
     * @brief 加载材质定义文件
     * @param filePath JSON 文件路径
     * @return 成功返回 true
     */
    bool LoadMaterialsFromFile(const std::string& filePath);
    
    /**
     * @brief 加载材质定义（从 JSON 对象）
     * @param json JSON 对象
     * @return 成功返回 true
     */
    bool LoadMaterialsFromJson(const std::string& json);
    
    /**
     * @brief 注册材质
     * @param material 材质对象
     * @return 成功返回 true，如果同名材质已存在返回 false
     */
    bool RegisterMaterial(const PhysicsMaterial& material);
    
    /**
     * @brief 获取材质
     * @param name 材质名称
     * @return 材质指针，如果不存在返回 nullptr
     */
    const PhysicsMaterial* GetMaterial(const std::string& name) const;
    
    /**
     * @brief 检查材质是否存在
     * @param name 材质名称
     * @return 存在返回 true
     */
    bool HasMaterial(const std::string& name) const;
    
    /**
     * @brief 清除所有材质
     */
    void Clear();
    
    /**
     * @brief 获取所有材质名称
     * @return 材质名称列表
     */
    std::vector<std::string> GetAllMaterialNames() const;
    
private:
    std::unordered_map<std::string, PhysicsMaterial> m_materials;  ///< 材质映射表（名称 -> 材质）
};

} // namespace ECS
} // namespace Render
