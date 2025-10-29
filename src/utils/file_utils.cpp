#include "render/file_utils.h"
#include "render/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Render {

std::string FileUtils::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file: " + filepath);
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

std::vector<uint8_t> FileUtils::ReadBinaryFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open binary file: " + filepath);
        return {};
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOG_ERROR("Failed to read binary file: " + filepath);
        return {};
    }
    
    file.close();
    return buffer;
}

bool FileUtils::FileExists(const std::string& filepath) {
    return std::filesystem::exists(filepath);
}

std::string FileUtils::GetFileExtension(const std::string& filepath) {
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    return ext;
}

std::string FileUtils::GetFileName(const std::string& filepath) {
    std::filesystem::path path(filepath);
    return path.stem().string();
}

std::string FileUtils::GetDirectory(const std::string& filepath) {
    std::filesystem::path path(filepath);
    return path.parent_path().string();
}

std::string FileUtils::CombinePaths(const std::string& base, const std::string& relative) {
    std::filesystem::path basePath(base);
    std::filesystem::path relativePath(relative);
    return (basePath / relativePath).string();
}

} // namespace Render

