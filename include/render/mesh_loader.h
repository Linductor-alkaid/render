#pragma once

#include "mesh.h"
#include "material.h"
#include "types.h"
#include <memory>
#include <vector>

namespace Render {

// 前向声明
class Material;

/**
 * @brief 网格与材质数据结构
 * 
 * 用于从文件加载时返回网格和关联的材质
 */
struct MeshWithMaterial {
    Ref<Mesh> mesh;           ///< 网格数据
    Ref<Material> material;   ///< 材质数据（可能为 nullptr）
    std::string name;         ///< 网格名称
    
    MeshWithMaterial() = default;
    MeshWithMaterial(Ref<Mesh> m, Ref<Material> mat = nullptr, const std::string& n = "")
        : mesh(m), material(mat), name(n) {}
};

/**
 * @brief 网格加载器
 * 
 * 提供创建基本几何形状的工具函数和外部模型文件加载功能
 * 支持的格式：OBJ, FBX, GLTF/GLB, Collada, Blender, PMX/PMD (MMD), 3DS, PLY, STL 等
 * 
 * 材质加载：
 * - 使用 LoadFromFileWithMaterials() 可以同时加载网格和材质
 * - 自动加载漫反射、镜面反射、法线等纹理贴图
 * - 支持 Phong 和 PBR 材质参数
 */
class MeshLoader {
public:
    // ========================================================================
    // 文件加载功能
    // ========================================================================
    
    /**
     * @brief 从文件加载模型（可能包含多个网格）
     * @param filepath 模型文件路径
     * @param flipUVs 是否翻转 UV 坐标（默认 true，适用于 OpenGL）
     * @return 网格列表（如果加载失败返回空列表）
     * 
     * 支持的格式：
     * - .obj - Wavefront OBJ
     * - .fbx - Autodesk FBX
     * - .gltf, .glb - GL Transmission Format
     * - .dae - Collada
     * - .blend - Blender
     * - .pmx, .pmd - MikuMikuDance (MMD)
     * - .3ds - 3D Studio
     * - .ply - Polygon File Format
     * - .stl - Stereolithography
     * 
     * 注意：
     * - 此方法必须在 OpenGL 上下文的线程中调用（因为会调用 Upload()）
     * - 返回的网格已自动上传到 GPU
     * - 模型会自动三角化
     * - 法线会自动生成（如果文件中不包含）
     */
    static std::vector<Ref<Mesh>> LoadFromFile(
        const std::string& filepath,
        bool flipUVs = true
    );
    
    /**
     * @brief 从文件加载单个网格
     * @param filepath 模型文件路径
     * @param meshIndex 网格索引（默认 0，第一个网格）
     * @param flipUVs 是否翻转 UV 坐标（默认 true）
     * @return 网格对象（如果加载失败返回 nullptr）
     * 
     * 如果模型包含多个网格，只返回指定索引的网格
     */
    static Ref<Mesh> LoadMeshFromFile(
        const std::string& filepath,
        uint32_t meshIndex = 0,
        bool flipUVs = true
    );
    
    /**
     * @brief 从文件加载模型（包含网格和材质）
     * @param filepath 模型文件路径
     * @param basePath 纹理文件搜索基础路径（默认为模型文件所在目录）
     * @param flipUVs 是否翻转 UV 坐标（默认 true）
     * @param shader 材质使用的着色器（如果为 nullptr，材质将不包含着色器）
     * @return 网格与材质列表
     * 
     * 此方法会：
     * 1. 加载所有网格数据
     * 2. 解析材质属性（颜色、光泽度等）
     * 3. 加载纹理贴图（漫反射、镜面反射、法线等）
     * 4. 创建 Material 对象并关联网格
     * 
     * 注意：
     * - 必须在 OpenGL 上下文的线程中调用
     * - 纹理路径相对于 basePath
     */
    static std::vector<MeshWithMaterial> LoadFromFileWithMaterials(
        const std::string& filepath,
        const std::string& basePath = "",
        bool flipUVs = true,
        Ref<Shader> shader = nullptr
    );
    
    // ========================================================================
    // 批量资源管理
    // ========================================================================
    
    /**
     * @brief 批量上传网格到GPU（性能优化）
     * @param meshes 要上传的网格列表
     * @param maxConcurrent 最大并发上传数（默认5，避免OpenGL驱动过载）
     * @param progressCallback 进度回调函数（可选）
     * @return 成功上传的网格数量
     * 
     * 功能说明：
     * - 分批上传网格，避免OpenGL驱动资源竞争
     * - 自动跳过已上传的网格
     * - 支持进度回调，便于显示加载进度
     * - 异常安全：单个网格上传失败不影响其他网格
     * 
     * 使用场景：
     * - 初始化场景时批量上传所有网格
     * - 加载大型模型（如MMD模型）时避免卡顿
     * - 需要显示加载进度的场景
     * 
     * 示例：
     * @code
     * std::vector<Ref<Mesh>> meshes = { mesh1, mesh2, mesh3, ... };
     * 
     * // 方式1: 简单批量上传
     * size_t uploaded = MeshLoader::BatchUpload(meshes);
     * 
     * // 方式2: 带进度回调
     * size_t total = meshes.size();
     * auto callback = [total](size_t current, size_t total, const Ref<Mesh>& mesh) {
     *     float progress = (float)current / total * 100.0f;
     *     std::cout << "上传进度: " << progress << "%" << std::endl;
     * };
     * size_t uploaded = MeshLoader::BatchUpload(meshes, 5, callback);
     * @endcode
     */
    static size_t BatchUpload(
        const std::vector<Ref<Mesh>>& meshes,
        size_t maxConcurrent = 5,
        std::function<void(size_t current, size_t total, const Ref<Mesh>& mesh)> progressCallback = nullptr
    );
    
    // ========================================================================
    // 基本几何形状生成
    // ========================================================================
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

