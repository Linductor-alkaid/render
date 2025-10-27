#pragma once

#include "mesh.h"
#include "types.h"
#include <memory>

namespace Render {

/**
 * @brief 网格加载器
 * 
 * 提供创建基本几何形状的工具函数
 * 后续可扩展文件加载功能（如 OBJ, FBX 等）
 */
class MeshLoader {
public:
    /**
     * @brief 创建平面（Plane）
     * @param width 宽度
     * @param height 高度
     * @param widthSegments 宽度分段数（默认 1）
     * @param heightSegments 高度分段数（默认 1）
     * @param color 顶点颜色（默认白色）
     * @return 平面网格
     * 
     * 平面位于 XZ 平面，法线向上（+Y）
     */
    static Ref<Mesh> CreatePlane(
        float width = 1.0f, 
        float height = 1.0f,
        uint32_t widthSegments = 1,
        uint32_t heightSegments = 1,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建立方体（Cube）
     * @param width 宽度（X轴）
     * @param height 高度（Y轴）
     * @param depth 深度（Z轴）
     * @param color 顶点颜色（默认白色）
     * @return 立方体网格
     * 
     * 立方体中心位于原点
     */
    static Ref<Mesh> CreateCube(
        float width = 1.0f,
        float height = 1.0f,
        float depth = 1.0f,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建球体（Sphere）
     * @param radius 半径
     * @param segments 水平分段数（经度）
     * @param rings 垂直分段数（纬度）
     * @param color 顶点颜色（默认白色）
     * @return 球体网格
     * 
     * 球体中心位于原点
     */
    static Ref<Mesh> CreateSphere(
        float radius = 0.5f,
        uint32_t segments = 32,
        uint32_t rings = 16,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建圆柱体（Cylinder）
     * @param radiusTop 顶部半径
     * @param radiusBottom 底部半径
     * @param height 高度
     * @param segments 圆周分段数
     * @param color 顶点颜色（默认白色）
     * @return 圆柱体网格
     * 
     * 圆柱体中心位于原点，沿 Y 轴延伸
     */
    static Ref<Mesh> CreateCylinder(
        float radiusTop = 0.5f,
        float radiusBottom = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建圆锥体（Cone）
     * @param radius 底部半径
     * @param height 高度
     * @param segments 圆周分段数
     * @param color 顶点颜色（默认白色）
     * @return 圆锥体网格
     * 
     * 圆锥体底部中心位于原点，顶点沿 +Y 方向
     */
    static Ref<Mesh> CreateCone(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建圆环（Torus）
     * @param majorRadius 大圆半径（环的中心到管中心的距离）
     * @param minorRadius 小圆半径（管的半径）
     * @param majorSegments 大圆分段数
     * @param minorSegments 小圆分段数
     * @param color 顶点颜色（默认白色）
     * @return 圆环网格
     * 
     * 圆环位于 XZ 平面，中心在原点
     */
    static Ref<Mesh> CreateTorus(
        float majorRadius = 1.0f,
        float minorRadius = 0.3f,
        uint32_t majorSegments = 32,
        uint32_t minorSegments = 16,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建胶囊体（Capsule）
     * @param radius 半径
     * @param height 中间圆柱部分的高度（不含两端半球）
     * @param segments 圆周分段数
     * @param rings 半球纬度分段数
     * @param color 顶点颜色（默认白色）
     * @return 胶囊体网格
     * 
     * 胶囊体沿 Y 轴，中心在原点
     */
    static Ref<Mesh> CreateCapsule(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32,
        uint32_t rings = 8,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建四边形（Quad）
     * @param width 宽度
     * @param height 高度
     * @param color 顶点颜色（默认白色）
     * @return 四边形网格
     * 
     * 四边形位于 XY 平面，法线朝向 +Z，中心在原点
     */
    static Ref<Mesh> CreateQuad(
        float width = 1.0f,
        float height = 1.0f,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建三角形
     * @param size 大小（边长）
     * @param color 顶点颜色（默认白色）
     * @return 三角形网格
     * 
     * 等边三角形位于 XY 平面，法线朝向 +Z，中心在原点
     */
    static Ref<Mesh> CreateTriangle(
        float size = 1.0f,
        const Color& color = Color::White()
    );
    
    /**
     * @brief 创建圆形（Circle）
     * @param radius 半径
     * @param segments 分段数
     * @param color 顶点颜色（默认白色）
     * @return 圆形网格
     * 
     * 圆形位于 XY 平面，法线朝向 +Z，中心在原点
     */
    static Ref<Mesh> CreateCircle(
        float radius = 0.5f,
        uint32_t segments = 32,
        const Color& color = Color::White()
    );

private:
    // 禁止实例化
    MeshLoader() = delete;
    ~MeshLoader() = delete;
};

} // namespace Render

