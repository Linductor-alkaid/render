#pragma once

#include "render/types.h"
#include "render/render_state.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace Render {

// 前向声明
class Shader;
class Texture;

/**
 * @brief 材质类 - 管理渲染材质属性、纹理和着色器
 * 
 * Material 封装了渲染所需的所有属性，包括：
 * - 物理材质属性（环境色、漫反射、镜面反射等）
 * - 纹理贴图（漫反射贴图、法线贴图、镜面贴图等）
 * - 着色器程序
 * - 渲染状态（混合模式、深度测试等）
 * 
 * 线程安全：
 * - ✅ 所有公共方法都是线程安全的
 * - ✅ 使用互斥锁保护内部状态
 * - ✅ Getter 方法返回副本以保证线程安全
 * - ✅ 移动操作使用 scoped_lock 避免死锁
 * - ⚠️ 注意：OpenGL 调用需要在创建上下文的线程中执行（通常是主线程）
 * 
 * 使用示例：
 * @code
 * auto material = std::make_shared<Material>();
 * material->SetName("Wood");
 * material->SetShader(shader);
 * material->SetDiffuseColor(Color(0.8f, 0.6f, 0.4f, 1.0f));
 * material->SetTexture("diffuseMap", diffuseTexture);
 * material->SetFloat("roughness", 0.7f);
 * 
 * // 应用材质（线程安全）
 * material->Bind();
 * // 渲染...
 * material->Unbind();
 * @endcode
 */
class Material {
    // 确保正确的内存对齐以使用 Eigen 的 SIMD 优化
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
public:
    /**
     * @brief 构造函数
     */
    Material();
    
    /**
     * @brief 析构函数
     */
    ~Material();
    
    // 禁止拷贝
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    
    // 允许移动
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;
    
    // ========================================================================
    // 名称管理
    // ========================================================================
    
    /**
     * @brief 设置材质名称（线程安全）
     * @param name 材质名称
     */
    void SetName(const std::string& name);
    
    /**
     * @brief 获取材质名称（线程安全，返回副本）
     * @return 材质名称
     */
    std::string GetName() const;
    
    // ========================================================================
    // 着色器管理
    // ========================================================================
    
    /**
     * @brief 设置着色器
     * @param shader 着色器指针
     */
    void SetShader(std::shared_ptr<Shader> shader);
    
    /**
     * @brief 获取着色器（线程安全）
     * @return 着色器指针
     */
    std::shared_ptr<Shader> GetShader() const;
    
    // ========================================================================
    // 材质属性 - 颜色
    // ========================================================================
    
    /**
     * @brief 设置环境光颜色（线程安全）
     * @param color 环境光颜色
     */
    void SetAmbientColor(const Color& color);
    
    /**
     * @brief 获取环境光颜色（线程安全，返回副本）
     * @return 环境光颜色
     */
    Color GetAmbientColor() const;
    
    /**
     * @brief 设置漫反射颜色（线程安全）
     * @param color 漫反射颜色
     */
    void SetDiffuseColor(const Color& color);
    
    /**
     * @brief 获取漫反射颜色（线程安全，返回副本）
     * @return 漫反射颜色
     */
    Color GetDiffuseColor() const;
    
    /**
     * @brief 设置镜面反射颜色（线程安全）
     * @param color 镜面反射颜色
     */
    void SetSpecularColor(const Color& color);
    
    /**
     * @brief 获取镜面反射颜色（线程安全，返回副本）
     * @return 镜面反射颜色
     */
    Color GetSpecularColor() const;
    
    /**
     * @brief 设置自发光颜色（线程安全）
     * @param color 自发光颜色
     */
    void SetEmissiveColor(const Color& color);
    
    /**
     * @brief 获取自发光颜色（线程安全，返回副本）
     * @return 自发光颜色
     */
    Color GetEmissiveColor() const;
    
    // ========================================================================
    // 材质属性 - 物理参数
    // ========================================================================
    
    /**
     * @brief 设置镜面反射强度（线程安全）
     * @param shininess 镜面反射强度（0.0 - 128.0）
     */
    void SetShininess(float shininess);
    
    /**
     * @brief 获取镜面反射强度（线程安全）
     * @return 镜面反射强度
     */
    float GetShininess() const;
    
    /**
     * @brief 设置不透明度（线程安全）
     * @param opacity 不透明度（0.0 - 1.0）
     */
    void SetOpacity(float opacity);
    
    /**
     * @brief 获取不透明度（线程安全）
     * @return 不透明度
     */
    float GetOpacity() const;
    
    /**
     * @brief 设置金属度（PBR，线程安全）
     * @param metallic 金属度（0.0 - 1.0）
     */
    void SetMetallic(float metallic);
    
    /**
     * @brief 获取金属度（线程安全）
     * @return 金属度
     */
    float GetMetallic() const;
    
    /**
     * @brief 设置粗糙度（PBR，线程安全）
     * @param roughness 粗糙度（0.0 - 1.0）
     */
    void SetRoughness(float roughness);
    
    /**
     * @brief 获取粗糙度（线程安全）
     * @return 粗糙度
     */
    float GetRoughness() const;
    
    // ========================================================================
    // 纹理管理
    // ========================================================================
    
    /**
     * @brief 设置纹理
     * @param name 纹理名称（如 "diffuseMap", "normalMap" 等）
     * @param texture 纹理指针
     */
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);
    
    /**
     * @brief 获取纹理
     * @param name 纹理名称
     * @return 纹理指针，如果不存在返回 nullptr
     */
    std::shared_ptr<Texture> GetTexture(const std::string& name) const;
    
    /**
     * @brief 检查是否有指定纹理
     * @param name 纹理名称
     * @return 存在返回 true
     */
    bool HasTexture(const std::string& name) const;
    
    /**
     * @brief 移除纹理
     * @param name 纹理名称
     */
    void RemoveTexture(const std::string& name);
    
    /**
     * @brief 清空所有纹理
     */
    void ClearTextures();
    
    /**
     * @brief 获取所有纹理名称
     * @return 纹理名称列表
     * 
     * @note 性能说明：此方法返回副本，频繁调用可能有性能开销。
     *       对于高性能场景，考虑使用 ForEachTexture() 方法。
     */
    std::vector<std::string> GetTextureNames() const;
    
    /**
     * @brief 通过回调遍历所有纹理（高性能版本）
     * @param callback 回调函数 (name, texture)
     * 
     * 避免拷贝纹理名称，直接在锁保护下遍历。
     * 适用于频繁访问纹理的场景。
     * 
     * @example
     * @code
     * material->ForEachTexture([](const std::string& name, const Ref<Texture>& tex) {
     *     std::cout << "Texture: " << name << std::endl;
     * });
     * @endcode
     */
    void ForEachTexture(std::function<void(const std::string&, const Ref<Texture>&)> callback) const;
    
    // ========================================================================
    // 自定义参数（Uniform）
    // ========================================================================
    
    /**
     * @brief 设置整型参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetInt(const std::string& name, int value);
    
    /**
     * @brief 设置浮点参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetFloat(const std::string& name, float value);
    
    /**
     * @brief 设置向量2参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetVector2(const std::string& name, const Vector2& value);
    
    /**
     * @brief 设置向量3参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetVector3(const std::string& name, const Vector3& value);
    
    /**
     * @brief 设置向量4参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetVector4(const std::string& name, const Vector4& value);
    
    /**
     * @brief 设置颜色参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetColor(const std::string& name, const Color& value);
    
    /**
     * @brief 设置向量2数组参数（传入 uniform 数组）
     * @param name uniform 名称（如 "uExtraUVSetScales[0]"）
     * @param values 参数值集合
     */
    void SetVector2Array(const std::string& name, const std::vector<Vector2>& values);
    
    /**
     * @brief 设置颜色数组参数（传入 uniform 数组）
     * @param name uniform 名称（如 "uExtraColorSets[0]"）
     * @param values 参数值集合
     */
    void SetColorArray(const std::string& name, const std::vector<Color>& values);
    
    /**
     * @brief 设置矩阵4参数
     * @param name 参数名称
     * @param value 参数值
     */
    void SetMatrix4(const std::string& name, const Matrix4& value);
    
    // ========================================================================
    // 渲染状态
    // ========================================================================
    
    /**
     * @brief 设置混合模式（线程安全）
     * @param mode 混合模式
     */
    void SetBlendMode(BlendMode mode);
    
    /**
     * @brief 获取混合模式（线程安全）
     * @return 混合模式
     */
    BlendMode GetBlendMode() const;
    
    /**
     * @brief 设置面剔除模式（线程安全）
     * @param mode 面剔除模式
     */
    void SetCullFace(CullFace mode);
    
    /**
     * @brief 获取面剔除模式（线程安全）
     * @return 面剔除模式
     */
    CullFace GetCullFace() const;
    
    /**
     * @brief 设置深度测试（线程安全）
     * @param enable 是否启用
     */
    void SetDepthTest(bool enable);
    
    /**
     * @brief 获取深度测试状态（线程安全）
     * @return 深度测试状态
     */
    bool GetDepthTest() const;
    
    /**
     * @brief 设置深度写入（线程安全）
     * @param enable 是否启用
     */
    void SetDepthWrite(bool enable);
    
    /**
     * @brief 获取深度写入状态（线程安全）
     * @return 深度写入状态
     */
    bool GetDepthWrite() const;
    
    // ========================================================================
    // 应用和绑定
    // ========================================================================
    
    /**
     * @brief 绑定材质（应用所有设置到渲染管线）
     * @param renderState 渲染状态管理器（可选）
     * 
     * 此方法会：
     * 1. 激活着色器
     * 2. 绑定所有纹理
     * 3. 设置所有 uniform 参数
     * 4. 应用渲染状态
     */
    void Bind(RenderState* renderState = nullptr);
    
    /**
     * @brief 解绑材质
     */
    void Unbind();
    
    /**
     * @brief 应用渲染状态到 RenderState
     * @param renderState 渲染状态管理器
     */
    void ApplyRenderState(RenderState* renderState);
    
    /**
     * @brief 检查材质是否有效（有着色器且着色器有效）
     * @return 有效返回 true
     */
    bool IsValid() const;

    /**
     * @brief 获取材质的稳定唯一 ID
     *
     * 该 ID 在材质生命周期内保持不变，可用于排序和批处理键。
     */
    uint32_t GetStableID() const noexcept { return m_stableID; }
    
private:
    std::string m_name;                     ///< 材质名称
    std::shared_ptr<Shader> m_shader;       ///< 着色器
    
    // 材质颜色属性
    Color m_ambientColor;                   ///< 环境光颜色
    Color m_diffuseColor;                   ///< 漫反射颜色
    Color m_specularColor;                  ///< 镜面反射颜色
    Color m_emissiveColor;                  ///< 自发光颜色
    
    // 材质物理属性
    float m_shininess;                      ///< 镜面反射强度
    float m_opacity;                        ///< 不透明度
    float m_metallic;                       ///< 金属度（PBR）
    float m_roughness;                      ///< 粗糙度（PBR）
    
    // 纹理
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    
    // 自定义参数
    std::unordered_map<std::string, int> m_intParams;
    std::unordered_map<std::string, float> m_floatParams;
    std::unordered_map<std::string, Vector2> m_vector2Params;
    std::unordered_map<std::string, Vector3> m_vector3Params;
    std::unordered_map<std::string, Vector4> m_vector4Params;
    std::unordered_map<std::string, Matrix4> m_matrix4Params;
    std::unordered_map<std::string, std::vector<Vector2>> m_vector2ArrayParams;
    std::unordered_map<std::string, std::vector<Color>> m_colorArrayParams;
    
    // 渲染状态
    BlendMode m_blendMode;                  ///< 混合模式
    CullFace m_cullFace;                    ///< 面剔除模式
    bool m_depthTest;                       ///< 深度测试
    bool m_depthWrite;                      ///< 深度写入

    uint32_t m_stableID;                    ///< 稳定唯一 ID（用于排序/批处理）

    static std::atomic<uint32_t> s_nextStableID;
    
    mutable std::mutex m_mutex;             ///< 互斥锁，保护所有成员变量
};

// 类型别名
using MaterialPtr = std::shared_ptr<Material>;

} // namespace Render

