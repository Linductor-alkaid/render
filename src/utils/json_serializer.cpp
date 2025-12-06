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
#include "render/json_serializer.h"
#include "render/logger.h"
#include <fstream>
#include <sstream>

namespace Render {

bool JsonSerializer::LoadFromFile(const std::string& filepath, nlohmann::json& outJson) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::GetInstance().ErrorFormat("[JsonSerializer] Failed to open file: %s", filepath.c_str());
            return false;
        }
        
        file >> outJson;
        file.close();
        
        Logger::GetInstance().InfoFormat("[JsonSerializer] Successfully loaded JSON from: %s", filepath.c_str());
        return true;
        
    } catch (const nlohmann::json::parse_error& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] JSON parse error in file %s: %s", 
                                         filepath.c_str(), e.what());
        return false;
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] Error loading file %s: %s", 
                                         filepath.c_str(), e.what());
        return false;
    }
}

bool JsonSerializer::SaveToFile(const nlohmann::json& json, const std::string& filepath, int indent) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger::GetInstance().ErrorFormat("[JsonSerializer] Failed to create file: %s", filepath.c_str());
            return false;
        }
        
        file << json.dump(indent);
        file.close();
        
        Logger::GetInstance().InfoFormat("[JsonSerializer] Successfully saved JSON to: %s", filepath.c_str());
        return true;
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] Error saving file %s: %s", 
                                         filepath.c_str(), e.what());
        return false;
    }
}

bool JsonSerializer::ParseFromString(const std::string& jsonStr, nlohmann::json& outJson) {
    try {
        outJson = nlohmann::json::parse(jsonStr);
        return true;
        
    } catch (const nlohmann::json::parse_error& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] JSON parse error: %s", e.what());
        return false;
        
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] Error parsing JSON string: %s", e.what());
        return false;
    }
}

std::string JsonSerializer::ToString(const nlohmann::json& json, int indent) {
    try {
        return json.dump(indent);
    } catch (const std::exception& e) {
        Logger::GetInstance().ErrorFormat("[JsonSerializer] Error converting JSON to string: %s", e.what());
        return "{}";
    }
}

} // namespace Render


