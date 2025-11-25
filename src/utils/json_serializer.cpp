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


