# 基于物理的渲染 (PBR) 系统开发 Todolist

## 概述

本文档详细规划了基于物理的渲染 (Physically Based Rendering, PBR) 系统的开发任务。PBR系统将显著提升渲染引擎的视觉真实感，是现代游戏引擎的核心功能之一。

### 目标

- ✅ 实现完整的PBR材质系统（金属度/粗糙度工作流）
- ✅ 支持基于图像的光照 (IBL) 系统
- ✅ 集成到现有ECS架构和渲染管线
- ✅ 保持与现有Phong光照模型的兼容性
- ✅ 提供灵活的材质工作流选择

### 技术参考

- **Cook-Torrance BRDF**: 标准PBR光照模型
- **金属度/粗糙度工作流**: 主流PBR工作流（glTF 2.0标准）
- **IBL**: 基于图像的光照，使用预计算的辐照度图和预过滤环境图

### 与现有系统的集成点

- **Material系统**: 扩展现有Material类，添加PBR参数和纹理
- **Shader系统**: 创建新的PBR着色器，支持着色器变体
- **纹理系统**: 需要添加立方体贴图支持（用于IBL）
- **光照系统**: 与现有LightManager集成，支持PBR光照计算
- **ECS系统**: 添加PBRMaterialComponent（可选，或直接使用Material）

---

## 阶段1: 基础PBR材质系统 (1-2周)

### 1.1 立方体贴图支持

**目标**: 为IBL系统提供基础支持，立方体贴图是IBL的核心数据结构。

#### 任务清单

- [x] **创建TextureCubemap类**
  - [x] 在 `include/render/texture_cubemap.h` 中定义类接口
  - [x] 支持从6个面加载立方体贴图
  - [ ] 支持从HDRI文件加载并转换为立方体贴图（待实现，需要stb_image库）
  - [x] 实现立方体贴图的绑定和参数设置
  - [x] 支持立方体贴图的Mipmap生成
  - [x] 线程安全设计（与现有Texture类保持一致）

- [x] **实现立方体贴图加载器**
  - [x] 在 `src/rendering/texture_cubemap.cpp` 中实现
  - [x] 支持从6个独立图像文件加载（+X, -X, +Y, -Y, +Z, -Z）
  - [ ] 支持从HDRI文件加载（需要stb_image或类似库，待实现）
  - [ ] 集成到TextureLoader中（可选，后续添加）

- [ ] **HDRI加载支持**
  - [ ] 集成stb_image库（或使用现有SDL_image扩展）
  - [ ] 实现HDRI到立方体贴图的转换
  - [ ] 支持HDR格式（.hdr文件）

- [ ] **测试立方体贴图**
  - [ ] 创建测试用例验证6面加载
  - [ ] 创建测试用例验证HDRI转换
  - [ ] 验证立方体贴图渲染（天空盒测试）

**依赖**: 无  
**预计时间**: 2-3天

---

### 1.2 PBR材质类扩展

**目标**: 扩展现有Material类，添加完整的PBR参数支持。

#### 任务清单

- [ ] **扩展Material类接口**
  - [ ] 添加PBR工作流枚举（MetallicRoughness, SpecularGlossiness）
  - [ ] 添加PBR参数设置方法：
    - [ ] `SetAlbedo(glm::vec3)` / `GetAlbedo()`
    - [ ] `SetMetallic(float)` / `GetMetallic()`
    - [ ] `SetRoughness(float)` / `GetRoughness()`
    - [ ] `SetAO(float)` / `GetAO()` (环境光遮蔽)
    - [ ] `SetEmissive(glm::vec3)` / `GetEmissive()`
    - [ ] `SetEmissiveStrength(float)` / `GetEmissiveStrength()`
    - [ ] `SetNormalStrength(float)` / `GetNormalStrength()`
  - [ ] 添加PBR纹理设置方法：
    - [ ] `SetAlbedoMap(TexturePtr)`
    - [ ] `SetMetallicMap(TexturePtr)`
    - [ ] `SetRoughnessMap(TexturePtr)`
    - [ ] `SetMetallicRoughnessMap(TexturePtr)` (合并贴图，G通道=粗糙度，B通道=金属度)
    - [ ] `SetNormalMap(TexturePtr)` (复用现有)
    - [ ] `SetAOMap(TexturePtr)`
    - [ ] `SetEmissiveMap(TexturePtr)`
  - [ ] 添加工作流切换方法：`SetWorkflow(Workflow)`

- [ ] **更新Material内部状态**
  - [ ] 在 `CachedState` 中添加PBR参数
  - [ ] 更新 `Bind()` 方法，传递PBR参数到着色器
  - [ ] 保持向后兼容（Phong参数仍然可用）

- [ ] **材质序列化支持**
  - [ ] 更新JSON序列化，支持PBR参数
  - [ ] 更新反序列化，兼容旧版本材质文件

**依赖**: 无  
**预计时间**: 1-2天

---

### 1.3 PBR着色器实现

**目标**: 实现Cook-Torrance BRDF着色器，支持金属度/粗糙度工作流。

#### 任务清单

- [ ] **创建PBR顶点着色器**
  - [ ] 在 `shaders/pbr.vert` 中实现
  - [ ] 输出世界空间位置、法线、切线、纹理坐标
  - [ ] 支持法线贴图（切线空间计算）
  - [ ] 与现有顶点着色器结构保持一致

- [ ] **实现Cook-Torrance BRDF函数**
  - [ ] 在 `shaders/pbr.frag` 中实现
  - [ ] **分布函数 (D)**: GGX/Trowbridge-Reitz分布
    ```glsl
    float DistributionGGX(vec3 N, vec3 H, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = max(dot(N, H), 0.0);
        float NdotH2 = NdotH * NdotH;
        float num = a2;
        float denom = (NdotH2 * (a2 - 1.0) + 1.0);
        denom = PI * denom * denom;
        return num / denom;
    }
    ```
  - [ ] **几何函数 (G)**: Smith's method with Schlick-GGX
    ```glsl
    float GeometrySchlickGGX(float NdotV, float roughness) {
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;
        float num = NdotV;
        float denom = NdotV * (1.0 - k) + k;
        return num / denom;
    }
    
    float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float ggx2 = GeometrySchlickGGX(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX(NdotL, roughness);
        return ggx1 * ggx2;
    }
    ```
  - [ ] **菲涅尔函数 (F)**: Fresnel-Schlick近似
    ```glsl
    vec3 FresnelSchlick(float cosTheta, vec3 F0) {
        return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    }
    ```

- [ ] **实现PBR光照计算**
  - [ ] 直接光照计算（方向光、点光源、聚光灯）
  - [ ] 金属度工作流：F0从金属度计算
    ```glsl
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    ```
  - [ ] 环境光项（暂时使用常量，IBL阶段会替换）
  - [ ] 与现有LightManager集成，使用相同的LightingData结构

- [ ] **纹理采样和混合**
  - [ ] 支持albedo贴图
  - [ ] 支持metallic/roughness合并贴图或分离贴图
  - [ ] 支持法线贴图（复用现有逻辑）
  - [ ] 支持AO贴图
  - [ ] 支持自发光贴图
  - [ ] 纹理缺失时的回退值

- [ ] **着色器变体支持**
  - [ ] 使用预处理器宏控制功能：
    - `USE_ALBEDO_MAP`
    - `USE_METALLIC_MAP`
    - `USE_ROUGHNESS_MAP`
    - `USE_METALLIC_ROUGHNESS_MAP`
    - `USE_NORMAL_MAP`
    - `USE_AO_MAP`
    - `USE_EMISSIVE_MAP`
  - [ ] 在ShaderCache中支持变体缓存

**依赖**: 1.2 (PBR材质类扩展)  
**预计时间**: 3-4天

---

### 1.4 集成到渲染管线

**目标**: 将PBR材质系统集成到现有渲染管线中。

#### 任务清单

- [ ] **更新Renderer类**
  - [ ] 在渲染循环中检测PBR材质
  - [ ] 自动选择PBR着色器（如果材质使用PBR参数）
  - [ ] 保持Phong材质向后兼容

- [ ] **更新Material::Bind()方法**
  - [ ] 检测材质类型（PBR vs Phong）
  - [ ] 根据材质类型绑定相应着色器
  - [ ] 传递PBR uniform参数：
    - `uAlbedo`
    - `uMetallic`
    - `uRoughness`
    - `uAO`
    - `uEmissive`
    - `uEmissiveStrength`
    - `uNormalStrength`
    - 各种纹理贴图

- [ ] **ECS组件集成（可选）**
  - [ ] 考虑添加 `PBRMaterialComponent`（或直接使用Material）
  - [ ] 更新MeshRenderComponent，支持PBR材质
  - [ ] 确保与现有组件系统兼容

- [ ] **材质排序优化**
  - [ ] 更新MaterialSortKey，考虑PBR材质
  - [ ] PBR材质可以单独分组，减少状态切换

**依赖**: 1.2, 1.3  
**预计时间**: 1-2天

---

### 1.5 测试和验证

**目标**: 创建测试场景，验证PBR材质系统的正确性。

#### 任务清单

- [ ] **创建PBR测试场景**
  - [ ] 金属球阵列测试（金属度0-1，粗糙度0-1）
  - [ ] 不同材质类型对比（金属、塑料、木头、布料、陶瓷）
  - [ ] 纹理贴图测试（各种PBR贴图组合）

- [ ] **视觉验证**
  - [ ] 对比标准PBR参考图（如glTF示例模型）
  - [ ] 验证金属度效果（金属vs非金属）
  - [ ] 验证粗糙度效果（镜面反射vs漫反射）
  - [ ] 验证法线贴图效果

- [ ] **性能测试**
  - [ ] 对比PBR vs Phong的性能开销
  - [ ] 测试不同纹理组合的性能
  - [ ] 验证批处理是否正常工作

- [ ] **边界情况测试**
  - [ ] 缺失纹理时的回退行为
  - [ ] 极端参数值（金属度=0/1，粗糙度=0/1）
  - [ ] 空材质/默认材质

**依赖**: 1.1, 1.2, 1.3, 1.4  
**预计时间**: 1-2天

---

## 阶段2: IBL系统 (2-3周)

### 2.1 IBL环境类设计

**目标**: 设计并实现IBL环境类，管理HDRI和预计算贴图。

#### 任务清单

- [ ] **设计IBLEnvironment类接口**
  - [ ] 在 `include/render/pbr/ibl_environment.h` 中定义
  - [ ] 核心方法：
    - [ ] `LoadHDRI(const std::string& path)` - 加载HDRI文件
    - [ ] `GenerateIrradianceMap()` - 生成辐照度图
    - [ ] `GeneratePrefilteredMap()` - 生成预过滤环境图
    - [ ] `GenerateBRDFLookup()` - 生成BRDF查找表
    - [ ] `GetIrradianceMap()` - 获取辐照度图
    - [ ] `GetPrefilteredMap()` - 获取预过滤环境图
    - [ ] `GetBRDFLookup()` - 获取BRDF查找表
  - [ ] 配置参数：
    - [ ] 辐照度图分辨率（默认32x32）
    - [ ] 预过滤环境图分辨率（默认128x128）
    - [ ] 预过滤环境图Mipmap级别（默认5级）
    - [ ] BRDF查找表分辨率（默认512x512）

- [ ] **实现IBLEnvironment类**
  - [ ] 在 `src/rendering/pbr/ibl_environment.cpp` 中实现
  - [ ] 使用帧缓冲对象(FBO)进行离屏渲染
  - [ ] 使用计算着色器或渲染到纹理进行预计算

**依赖**: 1.1 (立方体贴图支持)  
**预计时间**: 1天

---

### 2.2 HDRI加载和立方体贴图转换

**目标**: 实现HDRI到立方体贴图的转换。

#### 任务清单

- [ ] **HDRI文件加载**
  - [ ] 使用stb_image加载HDR文件
  - [ ] 解析HDR格式（RGBE或RGB float）
  - [ ] 转换为浮点纹理数据

- [ ] **等距柱状投影到立方体贴图**
  - [ ] 实现等距柱状投影(Equirectangular)到立方体贴图的转换
  - [ ] 创建转换着色器：
    - [ ] 顶点着色器：渲染立方体
    - [ ] 片段着色器：从等距柱状投影采样
  - [ ] 渲染6个面到立方体贴图

- [ ] **立方体贴图生成**
  - [ ] 创建6个面的FBO
  - [ ] 使用转换着色器渲染每个面
  - [ ] 生成完整的立方体贴图

**依赖**: 2.1, 1.1  
**预计时间**: 2-3天

---

### 2.3 辐照度图生成

**目标**: 生成用于漫反射IBL的辐照度图。

#### 任务清单

- [ ] **实现辐照度卷积着色器**
  - [ ] 创建 `shaders/ibl/irradiance_convolution.frag`
  - [ ] 对立方体贴图进行卷积计算
  - [ ] 使用重要性采样或均匀采样
  - [ ] 计算每个方向的平均辐照度

- [ ] **辐照度图生成流程**
  - [ ] 创建低分辨率立方体贴图（32x32每面）
  - [ ] 对每个方向进行卷积计算
  - [ ] 渲染6个面到辐照度立方体贴图
  - [ ] 生成Mipmap（可选）

- [ ] **优化**
  - [ ] 使用计算着色器加速（如果支持）
  - [ ] 多线程生成（如果可能）

**依赖**: 2.2  
**预计时间**: 2-3天

---

### 2.4 预过滤环境图生成

**目标**: 生成用于镜面反射IBL的预过滤环境图。

#### 任务清单

- [ ] **实现预过滤着色器**
  - [ ] 创建 `shaders/ibl/prefilter_environment.frag`
  - [ ] 实现重要性采样（Importance Sampling）
  - [ ] 使用GGX分布进行采样
  - [ ] 支持粗糙度级别（Mipmap级别对应不同粗糙度）

- [ ] **预过滤环境图生成流程**
  - [ ] 创建多级Mipmap立方体贴图（128x128基础，5级Mipmap）
  - [ ] 对每个Mipmap级别（粗糙度级别）进行预过滤
  - [ ] 使用不同的粗糙度值（0.0到1.0）
  - [ ] 渲染6个面到每个Mipmap级别

- [ ] **重要性采样实现**
  - [ ] 实现GGX重要性采样算法
  - [ ] 使用Hammersley序列生成样本
  - [ ] 样本数量可配置（默认1024）

**依赖**: 2.2  
**预计时间**: 3-4天

---

### 2.5 BRDF查找表生成

**目标**: 生成BRDF积分查找表，用于镜面反射IBL计算。

#### 任务清单

- [ ] **实现BRDF积分着色器**
  - [ ] 创建 `shaders/ibl/brdf_integration.frag`
  - [ ] 计算BRDF的2D积分（NdotV vs 粗糙度）
  - [ ] 使用数值积分方法

- [ ] **BRDF查找表生成**
  - [ ] 创建2D纹理（512x512）
  - [ ] R通道：缩放因子
  - [ ] G通道：偏移因子
  - [ ] 使用渲染到纹理生成

- [ ] **验证查找表**
  - [ ] 对比参考实现（如LearnOpenGL）
  - [ ] 确保数值精度正确

**依赖**: 2.1  
**预计时间**: 1-2天

---

### 2.6 IBL集成到PBR着色器

**目标**: 在PBR着色器中集成IBL计算。

#### 任务清单

- [ ] **更新PBR片段着色器**
  - [ ] 添加IBL uniform参数：
    - [ ] `samplerCube irradianceMap`
    - [ ] `samplerCube prefilterMap`
    - [ ] `sampler2D brdfLUT`
    - [ ] `float iblStrength` (IBL强度控制)
  - [ ] 实现漫反射IBL计算：
    ```glsl
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    vec3 ambient = (kD * diffuse) * ao;
    ```
  - [ ] 实现镜面反射IBL计算：
    ```glsl
    vec3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
    ```

- [ ] **更新Material::Bind()方法**
  - [ ] 传递IBL贴图到着色器
  - [ ] 设置IBL强度参数

- [ ] **IBL管理器集成**
  - [ ] 在Renderer或Scene中管理IBL环境
  - [ ] 支持场景级别的IBL设置
  - [ ] 支持每个材质覆盖IBL设置（可选）

**依赖**: 2.3, 2.4, 2.5  
**预计时间**: 2-3天

---

### 2.7 IBL测试和优化

**目标**: 测试IBL系统，优化性能。

#### 任务清单

- [ ] **创建IBL测试场景**
  - [ ] 加载不同HDRI环境（室内、室外、夜晚）
  - [ ] 测试不同材质的IBL反射效果
  - [ ] 对比有无IBL的视觉效果

- [ ] **性能优化**
  - [ ] 预计算贴图分辨率优化
  - [ ] 采样数量优化（辐照度图、预过滤图）
  - [ ] 纹理压缩（如果支持）
  - [ ] 延迟加载IBL资源

- [ ] **内存管理**
  - [ ] IBL资源缓存机制
  - [ ] 资源释放策略
  - [ ] 多场景IBL切换

- [ ] **视觉质量验证**
  - [ ] 对比标准PBR参考（如glTF示例）
  - [ ] 验证不同粗糙度的反射效果
  - [ ] 验证金属材质的反射效果

**依赖**: 2.6  
**预计时间**: 2-3天

---

## 阶段3: 高级特性 (1-2周)

### 3.1 自发光材质增强

**目标**: 增强自发光材质功能，支持HDR自发光和动态强度。

#### 任务清单

- [ ] **HDR自发光支持**
  - [ ] 支持超过1.0的自发光强度
  - [ ] 添加色调映射前的自发光贡献
  - [ ] 支持自发光影响周围物体（可选，需要全局光照）

- [ ] **自发光动画**
  - [ ] 支持时间控制的强度变化
  - [ ] 支持闪烁效果
  - [ ] 与动画系统集成

**依赖**: 阶段1, 阶段2  
**预计时间**: 1天

---

### 3.2 清漆层效果（Clear Coat）

**目标**: 实现清漆层效果，用于汽车漆、塑料等材质。

#### 任务清单

- [ ] **扩展PBR材质参数**
  - [ ] 添加清漆层参数：
    - [ ] `clearCoat` (0-1)
    - [ ] `clearCoatRoughness` (0-1)
    - [ ] `clearCoatNormalMap` (可选)

- [ ] **实现清漆层着色器**
  - [ ] 双层BRDF计算（基础层+清漆层）
  - [ ] 清漆层菲涅尔计算
  - [ ] 清漆层法线贴图支持

- [ ] **纹理支持**
  - [ ] 清漆层法线贴图
  - [ ] 清漆层参数贴图（可选）

**依赖**: 阶段1, 阶段2  
**预计时间**: 2-3天

---

### 3.3 次表面散射近似（Subsurface Scattering）

**目标**: 实现次表面散射的近似效果，用于皮肤、蜡等材质。

#### 任务清单

- [ ] **扩展PBR材质参数**
  - [ ] 添加次表面散射参数：
    - [ ] `subsurfaceColor` (散射颜色)
    - [ ] `subsurfaceStrength` (散射强度)
    - [ ] `subsurfaceRadius` (散射半径)

- [ ] **实现次表面散射着色器**
  - [ ] 使用Wrap Lighting近似
  - [ ] 或使用屏幕空间次表面散射（SSSS）
  - [ ] 与现有光照系统集成

- [ ] **性能优化**
  - [ ] 可选的次表面散射（性能开销较大）
  - [ ] 质量级别设置

**依赖**: 阶段1, 阶段2  
**预计时间**: 3-4天（如果实现SSSS）

---

### 3.4 各向异性反射（Anisotropic Reflection）

**目标**: 实现各向异性反射，用于拉丝金属、头发等材质。

#### 任务清单

- [ ] **扩展PBR材质参数**
  - [ ] 添加各向异性参数：
    - [ ] `anisotropic` (0-1)
    - [ ] `anisotropicDirection` (方向向量或切线)
    - [ ] `anisotropicMap` (方向贴图，可选)

- [ ] **实现各向异性BRDF**
  - [ ] 修改GGX分布函数支持各向异性
  - [ ] 修改几何函数支持各向异性
  - [ ] 切线空间计算

- [ ] **纹理支持**
  - [ ] 各向异性方向贴图
  - [ ] 与法线贴图结合

**依赖**: 阶段1, 阶段2  
**预计时间**: 2-3天

---

### 3.5 高级特性测试

**目标**: 测试所有高级特性，确保正确性和性能。

#### 任务清单

- [ ] **创建高级特性测试场景**
  - [ ] 清漆层材质测试（汽车模型）
  - [ ] 次表面散射测试（角色模型）
  - [ ] 各向异性反射测试（拉丝金属）

- [ ] **性能测试**
  - [ ] 对比有无高级特性的性能
  - [ ] 优化建议和文档

- [ ] **视觉质量验证**
  - [ ] 对比参考实现
  - [ ] 确保视觉效果正确

**依赖**: 3.1, 3.2, 3.3, 3.4  
**预计时间**: 1-2天

---

## 集成指南

### 与现有系统集成

#### Material系统集成

```cpp
// 创建PBR材质
auto pbrMaterial = std::make_shared<Material>();
pbrMaterial->SetWorkflow(Material::Workflow::MetallicRoughness);
pbrMaterial->SetAlbedo(glm::vec3(0.8f, 0.1f, 0.1f));
pbrMaterial->SetMetallic(0.0f);
pbrMaterial->SetRoughness(0.5f);

// 加载PBR贴图
auto albedoMap = TextureLoader::GetInstance().LoadTexture("albedo", "textures/metal_albedo.png");
pbrMaterial->SetAlbedoMap(albedoMap);

auto normalMap = TextureLoader::GetInstance().LoadTexture("normal", "textures/metal_normal.png");
pbrMaterial->SetNormalMap(normalMap);

// 应用到实体
entity.addComponent<MeshRenderComponent>(mesh, pbrMaterial);
```

#### IBL系统集成

```cpp
// 创建IBL环境
auto iblEnv = std::make_shared<IBLEnvironment>();
iblEnv->LoadHDRI("environments/sunset.hdr");
iblEnv->GenerateIrradianceMap();
iblEnv->GeneratePrefilteredMap();
iblEnv->GenerateBRDFLookup();

// 在渲染器中设置
renderer.SetIBLEnvironment(iblEnv);
```

#### 着色器变体管理

```cpp
// 自动选择着色器变体
ShaderVariantManager::Defines defines;
defines.USE_ALBEDO_MAP = material->HasTexture("albedoMap");
defines.USE_NORMAL_MAP = material->HasTexture("normalMap");
defines.USE_METALLIC_ROUGHNESS_MAP = material->HasTexture("metallicRoughnessMap");

auto shader = ShaderVariantManager::GetInstance().GetOrCreateVariant("pbr", defines);
material->SetShader(shader);
```

---

## 测试计划

### 单元测试

- [ ] Material类PBR参数设置和获取
- [ ] 立方体贴图加载和绑定
- [ ] IBL环境生成（辐照度图、预过滤图、BRDF查找表）
- [ ] BRDF函数正确性（对比参考实现）

### 集成测试

- [ ] PBR材质渲染测试
- [ ] IBL光照测试
- [ ] 与现有Phong材质共存测试
- [ ] 材质切换性能测试

### 视觉测试

- [ ] PBR材质球测试（金属度/粗糙度矩阵）
- [ ] 不同HDRI环境测试
- [ ] 与glTF参考模型对比
- [ ] 高级特性视觉验证

### 性能测试

- [ ] PBR vs Phong性能对比
- [ ] IBL性能开销测试
- [ ] 不同纹理组合的性能
- [ ] 批处理性能验证

---

## 文档需求

### API文档

- [ ] PBRMaterial API文档
- [ ] IBLEnvironment API文档
- [ ] TextureCubemap API文档
- [ ] PBR着色器接口文档

### 教程文档

- [ ] PBR材质创建教程
- [ ] IBL环境设置教程
- [ ] HDRI转换教程
- [ ] 高级特性使用指南

### 技术文档

- [ ] Cook-Torrance BRDF实现细节
- [ ] IBL预计算算法说明
- [ ] 性能优化指南
- [ ] 与glTF 2.0兼容性说明

---

## 时间估算

### 阶段1: 基础PBR材质系统
- **总计**: 8-12天
- 立方体贴图支持: 2-3天
- PBR材质类扩展: 1-2天
- PBR着色器实现: 3-4天
- 集成到渲染管线: 1-2天
- 测试和验证: 1-2天

### 阶段2: IBL系统
- **总计**: 12-18天
- IBL环境类设计: 1天
- HDRI加载和转换: 2-3天
- 辐照度图生成: 2-3天
- 预过滤环境图生成: 3-4天
- BRDF查找表生成: 1-2天
- IBL集成到着色器: 2-3天
- 测试和优化: 2-3天

### 阶段3: 高级特性
- **总计**: 8-12天
- 自发光增强: 1天
- 清漆层效果: 2-3天
- 次表面散射: 3-4天（如果实现SSSS）
- 各向异性反射: 2-3天
- 测试: 1-2天

### 总计
- **完整PBR系统**: 28-42天（约4-6周）
- **基础PBR + IBL**: 20-30天（约3-4周）
- **仅基础PBR**: 8-12天（约1-2周）

---

## 依赖关系图

```
阶段1.1 (立方体贴图)
    ↓
阶段1.2 (PBR材质类) → 阶段1.3 (PBR着色器) → 阶段1.4 (集成) → 阶段1.5 (测试)
    ↓                                                              ↓
阶段2.1 (IBL环境类) → 阶段2.2 (HDRI转换) → 阶段2.3 (辐照度图)
    ↓                                                              ↓
阶段2.4 (预过滤图) → 阶段2.5 (BRDF查找表) → 阶段2.6 (IBL集成) → 阶段2.7 (测试)
    ↓
阶段3.1 (自发光) → 阶段3.5 (测试)
阶段3.2 (清漆层) → 阶段3.5 (测试)
阶段3.3 (次表面散射) → 阶段3.5 (测试)
阶段3.4 (各向异性) → 阶段3.5 (测试)
```

---

## 风险和注意事项

### 技术风险

1. **立方体贴图支持**: 需要确保OpenGL上下文支持立方体贴图
2. **HDRI加载**: 可能需要集成额外的库（stb_image）
3. **性能开销**: IBL预计算可能耗时，需要优化
4. **内存占用**: IBL贴图占用较多显存，需要管理

### 兼容性考虑

1. **向后兼容**: 确保现有Phong材质仍然可用
2. **着色器变体**: 需要管理大量着色器变体
3. **资源管理**: IBL资源需要正确的生命周期管理

### 优化建议

1. **延迟加载**: IBL资源可以延迟加载
2. **纹理压缩**: 使用压缩纹理格式减少内存
3. **LOD系统**: IBL贴图可以使用不同分辨率
4. **缓存机制**: 预计算的IBL资源应该缓存

---

## 后续扩展方向

### 短期扩展（完成基础PBR后）

- [ ] 支持Specular/Glossiness工作流
- [ ] PBR材质编辑器（工具链集成）
- [ ] 材质预设库
- [ ] 实时IBL更新（动态光源）

### 长期扩展

- [ ] 光线追踪反射（如果支持RTX）
- [ ] 屏幕空间反射（SSR）增强
- [ ] 体积光照集成
- [ ] 材质LOD系统

---

## 总结

本Todolist详细规划了PBR系统的完整开发流程，分为三个阶段：

1. **阶段1**: 建立PBR基础，实现核心材质和着色器系统
2. **阶段2**: 实现IBL系统，提供真实的环境光照
3. **阶段3**: 添加高级特性，支持更复杂的材质效果

每个阶段都有明确的任务清单、依赖关系和时间估算。建议按阶段逐步实现，每个阶段完成后进行充分测试，确保系统稳定可靠。

---

**文档版本**: 1.0  
**最后更新**: 2025-01-XX  
**维护者**: RenderEngine开发团队
