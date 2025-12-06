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
#include "render/uniform_manager.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace Render {

/**
 * @brief 着色器类型
 */
enum class ShaderType {
    Vertex,
    Fragment,
    Geometry,
    Compute
};

/**
 * @brief 着色器程序类
 * 
 * 管理 OpenGL 着色器程序的编译、链接和使用
 */
class Shader {
public:
    Shader();
    virtual ~Shader();
    
    /**
     * @brief 从文件加载着色器
     * @param vertexPath 顶点着色器文件路径
     * @param fragmentPath 片段着色器文件路径
     * @param geometryPath 几何着色器文件路径（可选）
     * @return 成功返回 true，失败返回 false
     */
    bool LoadFromFile(const std::string& vertexPath,
                      const std::string& fragmentPath,
                      const std::string& geometryPath = "");
    
    /**
     * @brief 从文件加载Compute Shader
     * @param computePath Compute Shader文件路径
     * @return 成功返回 true，失败返回 false
     */
    bool LoadComputeShaderFromFile(const std::string& computePath);
    
    /**
     * @brief 从源码字符串加载着色器
     * @param vertexSource 顶点着色器源码
     * @param fragmentSource 片段着色器源码
     * @param geometrySource 几何着色器源码（可选）
     * @return 成功返回 true，失败返回 false
     */
    bool LoadFromSource(const std::string& vertexSource,
                        const std::string& fragmentSource,
                        const std::string& geometrySource = "");
    
    /**
     * @brief 从源码字符串加载Compute Shader
     * @param computeSource Compute Shader源码
     * @return 成功返回 true，失败返回 false
     */
    bool LoadComputeShaderFromSource(const std::string& computeSource);
    
    /**
     * @brief 使用此着色器程序
     */
    void Use() const;
    
    /**
     * @brief 解除使用
     */
    void Unuse() const;
    
    /**
     * @brief 检查着色器是否有效
     */
    bool IsValid() const { return m_programID != 0; }
    
    /**
     * @brief 获取程序 ID
     */
    uint32_t GetProgramID() const { return m_programID; }
    
    /**
     * @brief 获取 Uniform 管理器
     */
    UniformManager* GetUniformManager() { return m_uniformManager.get(); }
    const UniformManager* GetUniformManager() const { return m_uniformManager.get(); }
    
    /**
     * @brief 重新加载着色器（热重载）
     */
    bool Reload();
    
    /**
     * @brief 获取着色器名称
     */
    const std::string& GetName() const { return m_name; }
    
    /**
     * @brief 设置着色器名称
     */
    void SetName(const std::string& name) { m_name = name; }
    
protected:
    /**
     * @brief 编译单个着色器
     * @param source 着色器源码
     * @param type 着色器类型
     * @return 着色器对象 ID，失败返回 0
     */
    uint32_t CompileShader(const std::string& source, ShaderType type);
    
    /**
     * @brief 链接着色器程序
     * @param vertexShader 顶点着色器 ID
     * @param fragmentShader 片段着色器 ID
     * @param geometryShader 几何着色器 ID（可选，0 表示无）
     * @return 程序 ID，失败返回 0
     */
    uint32_t LinkProgram(uint32_t vertexShader, 
                         uint32_t fragmentShader,
                         uint32_t geometryShader = 0);
    
    /**
     * @brief 链接Compute Shader程序
     * @param computeShader Compute Shader ID
     * @return 程序 ID，失败返回 0
     */
    uint32_t LinkComputeProgram(uint32_t computeShader);
    
    /**
     * @brief 检查着色器编译错误
     */
    bool CheckCompileErrors(uint32_t shader, ShaderType type);
    
    /**
     * @brief 检查程序链接错误
     */
    bool CheckLinkErrors(uint32_t program);
    
    /**
     * @brief 删除着色器程序
     */
    void DeleteProgram();
    
    /**
     * @brief 从源码加载着色器（内部方法，不加锁）
     * @note 调用此方法前必须已持有 m_mutex 锁
     */
    bool LoadFromSource_Locked(const std::string& vertexSource,
                                const std::string& fragmentSource,
                                const std::string& geometrySource);
    
    /**
     * @brief 从源码加载Compute Shader（内部方法，不加锁）
     * @note 调用此方法前必须已持有 m_mutex 锁
     */
    bool LoadComputeShaderFromSource_Locked(const std::string& computeSource);
    
    /**
     * @brief 删除着色器程序（内部方法，不加锁）
     * @note 调用此方法前必须已持有 m_mutex 锁
     */
    void DeleteProgram_Locked();
    
private:
    uint32_t m_programID;
    std::string m_name;
    
    // 用于热重载
    std::string m_vertexPath;
    std::string m_fragmentPath;
    std::string m_geometryPath;
    std::string m_computePath;
    
    // Uniform 管理器
    std::unique_ptr<UniformManager> m_uniformManager;
    
    // 线程安全保护
    mutable std::mutex m_mutex;
};

} // namespace Render

