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

#include "render/ecs/physics/physics_material.h"
#include "render/json_serializer.h"
#include "render/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace Render {
namespace ECS {

bool PhysicsMaterialManager::LoadMaterialsFromFile(const std::string& filePath) {
    nlohmann::json json;
    if (!JsonSerializer::LoadFromFile(filePath, json)) {
        Logger::GetInstance().ErrorFormat("[PhysicsMaterialManager] Failed to load materials from file: %s", filePath.c_str());
        return false;
    }
    
    return LoadMaterialsFromJson(json.dump());
}

bool PhysicsMaterialManager::LoadMaterialsFromJson(const std::string& jsonStr) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        
        // 支持两种格式：
        // 1. { "materials": [{ "name": "...", "friction": ..., ... }, ...] }
        // 2. { "materialName": { "friction": ..., ... }, ... }
        
        if (json.contains("materials") && json["materials"].is_array()) {
            // 格式1：数组格式
            for (const auto& materialJson : json["materials"]) {
                PhysicsMaterial material;
                
                if (materialJson.contains("name")) {
                    material.name = materialJson["name"].get<std::string>();
                } else {
                    Logger::GetInstance().Warning("[PhysicsMaterialManager] Material missing 'name' field, skipping");
                    continue;
                }
                
                if (materialJson.contains("friction")) {
                    material.friction = materialJson["friction"].get<float>();
                }
                
                if (materialJson.contains("restitution")) {
                    material.restitution = materialJson["restitution"].get<float>();
                }
                
                if (materialJson.contains("density")) {
                    material.density = materialJson["density"].get<float>();
                }
                
                RegisterMaterial(material);
            }
        } else {
            // 格式2：对象格式，键为材质名称
            for (auto it = json.begin(); it != json.end(); ++it) {
                const std::string& materialName = it.key();
                const auto& materialJson = it.value();
                
                PhysicsMaterial material;
                material.name = materialName;
                
                if (materialJson.contains("friction")) {
                    material.friction = materialJson["friction"].get<float>();
                }
                
                if (materialJson.contains("restitution")) {
                    material.restitution = materialJson["restitution"].get<float>();
                }
                
                if (materialJson.contains("density")) {
                    material.density = materialJson["density"].get<float>();
                }
                
                RegisterMaterial(material);
            }
        }
        
        Logger::GetInstance().InfoFormat("[PhysicsMaterialManager] Loaded %zu materials", m_materials.size());
        return true;
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[PhysicsMaterialManager] Failed to parse JSON: %s", e.what());
        return false;
    }
}

bool PhysicsMaterialManager::RegisterMaterial(const PhysicsMaterial& material) {
    if (material.name.empty()) {
        Logger::GetInstance().Warning("[PhysicsMaterialManager] Cannot register material with empty name");
        return false;
    }
    
    if (m_materials.find(material.name) != m_materials.end()) {
        Logger::GetInstance().WarningFormat(
            "[PhysicsMaterialManager] Material '%s' already exists, overwriting", material.name.c_str()
        );
    }
    
    m_materials[material.name] = material;
    return true;
}

const PhysicsMaterial* PhysicsMaterialManager::GetMaterial(const std::string& name) const {
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        return &it->second;
    }
    return nullptr;
}

bool PhysicsMaterialManager::HasMaterial(const std::string& name) const {
    return m_materials.find(name) != m_materials.end();
}

void PhysicsMaterialManager::Clear() {
    m_materials.clear();
}

std::vector<std::string> PhysicsMaterialManager::GetAllMaterialNames() const {
    std::vector<std::string> names;
    names.reserve(m_materials.size());
    
    for (const auto& pair : m_materials) {
        names.push_back(pair.first);
    }
    
    return names;
}

} // namespace ECS
} // namespace Render
