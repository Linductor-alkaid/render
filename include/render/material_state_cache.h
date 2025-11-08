#pragma once

#include <cstdint>

namespace Render {

class Material;
class RenderState;

/**
 * @brief 材质绑定状态缓存
 *
 * 线程局部缓存，用于跳过连续的重复 Material::Bind 调用，减少 uniform 与状态设置开销。
 */
class MaterialStateCache {
public:
    /// 获取线程局部缓存实例
    static MaterialStateCache& Get();

    /// 重置缓存（通常在每帧开始时调用）
    void Reset();

    /// 判断当前材质是否需要重新绑定
    bool ShouldBind(const Material* material, RenderState* renderState) const;

    /// 在绑定后更新缓存
    void OnBind(const Material* material, RenderState* renderState);

private:
    const Material* m_lastMaterial = nullptr;
    RenderState* m_lastRenderState = nullptr;
};

} // namespace Render


