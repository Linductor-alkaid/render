#pragma once

#include "render/transform.h"
#include "render/types.h"
#include "entity.h"  // 需要 EntityID 定义
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <optional>  // C++17 std::optional
#include <unordered_map>
#include <functional>
#include <sstream>  // std::ostringstream
#include "render/sprite/sprite_nineslice.h"

namespace Render {

// 前向声明
class Camera;
class Mesh;
class Material;
class Texture;
class Framebuffer;

namespace ECS {

// 前向声明
class World;

// ============================================================
// Transform 组件（避免反复创建销毁）
// ============================================================

/**
 * @brief Transform 组件
 * 
 * 使用 shared_ptr 复用 Transform 对象，避免频繁创建销毁
 * 提供快捷访问接口，简化使用
 * 
 * **父子关系管理**（方案B - 使用实体ID）:
 * - 使用 parentEntity 存储父实体ID而非直接的Transform指针
 * - 由 TransformSystem 负责同步实体ID到Transform指针
 * - 确保生命周期安全，父实体销毁时自动清除关系
 */
struct TransformComponent {
    Ref<Transform> transform;    ///< 复用 Transform 对象（shared_ptr）
    EntityID parentEntity = EntityID::Invalid();  ///< 父实体ID（安全的父子关系管理）
    
    // 默认构造函数 - 自动创建 Transform 对象
    TransformComponent() : transform(std::make_shared<Transform>()) {}
    
    // 使用现有 Transform 对象构造
    explicit TransformComponent(const Ref<Transform>& t) : transform(t) {}
    
    // ==================== 快捷访问接口 ====================
    
    void SetPosition(const Vector3& pos) {
        if (transform) transform->SetPosition(pos);
    }
    
    void SetRotation(const Quaternion& rot) {
        if (transform) transform->SetRotation(rot);
    }
    
    void SetScale(const Vector3& scale) {
        if (transform) transform->SetScale(scale);
    }
    
    void SetScale(float uniformScale) {
        if (transform) transform->SetScale(uniformScale);
    }
    
    [[nodiscard]] Vector3 GetPosition() const {
        return transform ? transform->GetPosition() : Vector3::Zero();
    }
    
    [[nodiscard]] Quaternion GetRotation() const {
        return transform ? transform->GetRotation() : Quaternion::Identity();
    }
    
    [[nodiscard]] Vector3 GetScale() const {
        return transform ? transform->GetScale() : Vector3::Ones();
    }
    
    [[nodiscard]] Matrix4 GetLocalMatrix() const {
        return transform ? transform->GetLocalMatrix() : Matrix4::Identity();
    }
    
    [[nodiscard]] Matrix4 GetWorldMatrix() const {
        return transform ? transform->GetWorldMatrix() : Matrix4::Identity();
    }
    
    // 朝向目标
    void LookAt(const Vector3& target, const Vector3& up = Vector3::UnitY()) {
        if (transform) transform->LookAt(target, up);
    }
    
    // ==================== 父子关系（基于实体ID - 方案B）====================
    
    /**
     * @brief 设置父实体（通过实体ID）
     * @param world World 对象指针
     * @param parent 父实体ID
     * @return 成功返回 true，失败返回 false
     * 
     * @note 此方法会验证父实体的有效性
     * @note 实际的Transform指针同步由TransformSystem自动处理
     * @note 失败情况：父实体无效、父实体没有TransformComponent等
     */
    bool SetParentEntity(World* world, EntityID parent);
    
    /**
     * @brief 获取父实体ID
     * @return 父实体ID，如果没有父实体返回 Invalid
     */
    [[nodiscard]] EntityID GetParentEntity() const {
        return parentEntity;
    }
    
    /**
     * @brief 移除父对象
     * @return 总是返回 true
     */
    bool RemoveParent() {
        parentEntity = EntityID::Invalid();
        if (transform) transform->SetParent(nullptr);
        return true;
    }
    
    /**
     * @brief 验证父实体是否仍然有效
     * @param world World 对象指针
     * @return 如果父实体有效（或没有父实体）返回 true
     * 
     * @note 如果父实体已销毁，会自动清除父子关系
     */
    bool ValidateParentEntity(World* world);
    
    // ==================== 兼容性接口（原始指针）====================
    
    /**
     * @brief 获取父对象（原始指针）
     * @return 父Transform指针，可能为 nullptr
     * 
     * @warning 返回的指针可能会失效，推荐使用 GetParentEntity()
     * @deprecated 建议使用基于实体ID的接口
     */
    [[nodiscard]] Transform* GetParent() const {
        return transform ? transform->GetParent() : nullptr;
    }
    
    // ==================== 验证和调试接口 ====================
    
    /**
     * @brief 验证 Transform 状态
     * @return 如果 Transform 有效返回 true
     */
    [[nodiscard]] bool Validate() const {
        return transform && transform->Validate();
    }
    
    /**
     * @brief 获取调试字符串
     */
    [[nodiscard]] std::string DebugString() const {
        if (!transform) {
            return "TransformComponent { transform: null }";
        }
        std::string result = "TransformComponent { ";
        result += transform->DebugString();
        if (parentEntity.IsValid()) {
            result += ", parentEntity: " + std::to_string(parentEntity.index);
        }
        result += " }";
        return result;
    }
    
    /**
     * @brief 获取层级深度
     */
    [[nodiscard]] int GetHierarchyDepth() const {
        return transform ? transform->GetHierarchyDepth() : 0;
    }
    
    /**
     * @brief 获取子对象数量
     */
    [[nodiscard]] int GetChildCount() const {
        return transform ? transform->GetChildCount() : 0;
    }
};

// ============================================================
// 名称组件
// ============================================================

/**
 * @brief 名称组件
 * 
 * 为实体提供可读的名称，用于调试和显示
 */
struct NameComponent {
    std::string name;
    
    NameComponent() = default;
    explicit NameComponent(const std::string& n) : name(n) {}
};

// ============================================================
// 标签组件
// ============================================================

/**
 * @brief 标签组件
 * 
 * 为实体添加多个标签，用于查询和分组
 */
struct TagComponent {
    std::unordered_set<std::string> tags;
    
    TagComponent() = default;
    explicit TagComponent(const std::vector<std::string>& tagList) {
        for (const auto& tag : tagList) {
            tags.insert(tag);
        }
    }
    
    [[nodiscard]] bool HasTag(const std::string& tag) const {
        return tags.find(tag) != tags.end();
    }
    
    void AddTag(const std::string& tag) {
        tags.insert(tag);
    }
    
    void RemoveTag(const std::string& tag) {
        tags.erase(tag);
    }
    
    void Clear() {
        tags.clear();
    }
    
    [[nodiscard]] std::vector<std::string> GetTagList() const {
        return std::vector<std::string>(tags.begin(), tags.end());
    }
};

// ============================================================
// 激活状态组件
// ============================================================

/**
 * @brief 激活状态组件
 * 
 * 控制实体是否激活（激活的实体才会被系统处理）
 */
struct ActiveComponent {
    bool active = true;
    
    ActiveComponent() = default;
    explicit ActiveComponent(bool a) : active(a) {}
};

// ============================================================
// Mesh 渲染组件
// ============================================================

/**
 * @brief Mesh 渲染组件
 * 
 * 用于 3D 网格渲染
 * 支持异步资源加载和 LOD
 */
struct MeshRenderComponent {
    // ==================== 资源引用（通过 ResourceManager 管理）====================
    std::string meshName;          ///< 网格资源名称
    std::string materialName;      ///< 材质资源名称
    std::string shaderName;        ///< 着色器名称（可选，覆盖材质的着色器）
    
    // 着色器路径（可选，用于动态加载）
    // 如果提供了路径，ResourceLoadingSystem会自动加载着色器到ShaderCache
    // 如果未提供路径，则仅从ShaderCache中获取已预加载的着色器
    std::string shaderVertPath;    ///< 顶点着色器路径（可选）
    std::string shaderFragPath;    ///< 片段着色器路径（可选）
    std::string shaderGeomPath;    ///< 几何着色器路径（可选）
    
    Ref<Mesh> mesh;                ///< 网格对象（延迟加载）
    Ref<Material> material;        ///< 材质对象（延迟加载）
    
    // ==================== 渲染属性 ====================
    bool visible = true;           ///< 是否可见
    bool castShadows = true;       ///< 是否投射阴影
    bool receiveShadows = true;    ///< 是否接收阴影
    uint32_t layerID = 300;        ///< 渲染层级（默认 WORLD_GEOMETRY）
    int32_t renderPriority = 0;    ///< 渲染优先级
    
    // ==================== 材质属性覆盖 ====================
    // 这些属性会在渲染时覆盖材质的默认值
    
    struct MaterialOverride {
        std::optional<Color> diffuseColor;      ///< 漫反射颜色覆盖
        std::optional<Color> specularColor;     ///< 镜面反射颜色覆盖
        std::optional<Color> emissiveColor;     ///< 自发光颜色覆盖
        std::optional<float> shininess;         ///< 镜面反射强度覆盖
        std::optional<float> metallic;          ///< 金属度覆盖
        std::optional<float> roughness;         ///< 粗糙度覆盖
        std::optional<float> opacity;           ///< 不透明度覆盖
    };
    
    MaterialOverride materialOverride;  ///< 材质属性覆盖
    
    // ==================== 纹理设置 ====================
    
    struct TextureSettings {
        bool generateMipmaps = true;
        // 可以在这里扩展纹理参数（过滤模式、包裹模式等）
    };
    
    std::unordered_map<std::string, TextureSettings> textureSettings;  ///< 纹理设置（按名称）
    std::unordered_map<std::string, std::string> textureOverrides;      ///< 纹理覆盖（纹理名 -> 资源路径）
    
    // ==================== LOD 支持 ====================
    std::vector<float> lodDistances;  ///< LOD 距离阈值
    
    // ==================== 实例化渲染支持 ====================
    bool useInstancing = false;                      ///< 是否使用实例化渲染
    uint32_t instanceCount = 1;                      ///< 实例数量
    std::vector<Matrix4> instanceTransforms;         ///< 实例变换矩阵（可选）
    
    // ==================== 异步加载状态 ====================
    bool resourcesLoaded = false;     ///< 资源是否已加载
    bool asyncLoading = false;        ///< 是否正在异步加载
    
    MeshRenderComponent() = default;
    
    // ==================== 便捷方法 ====================
    
    /// 设置漫反射颜色覆盖
    void SetDiffuseColor(const Color& color) { materialOverride.diffuseColor = color; }
    
    /// 设置镜面反射颜色覆盖
    void SetSpecularColor(const Color& color) { materialOverride.specularColor = color; }
    
    /// 设置自发光颜色覆盖
    void SetEmissiveColor(const Color& color) { materialOverride.emissiveColor = color; }
    
    /// 设置镜面反射强度覆盖
    void SetShininess(float value) { materialOverride.shininess = value; }
    
    /// 设置金属度覆盖
    void SetMetallic(float value) { materialOverride.metallic = value; }
    
    /// 设置粗糙度覆盖
    void SetRoughness(float value) { materialOverride.roughness = value; }
    
    /// 设置不透明度覆盖
    void SetOpacity(float value) { materialOverride.opacity = value; }
    
    /// 清除所有材质覆盖
    void ClearMaterialOverrides() { materialOverride = MaterialOverride{}; }
};

// ============================================================
// Sprite 渲染组件（2D）
// ============================================================

/**
 * @brief Sprite 渲染组件
 * 
 * 用于 2D 精灵渲染
 */
struct SpriteRenderComponent {
    std::string textureName;       ///< 纹理资源名称
    Ref<Texture> texture;          ///< 纹理对象（延迟加载）
    
    Rect sourceRect{0, 0, 1, 1};   ///< 源矩形（UV 坐标）
    Vector2 size{1.0f, 1.0f};      ///< 显示大小
    Color tintColor{1, 1, 1, 1};   ///< 着色
    
    bool visible = true;
    uint32_t layerID = 800;        ///< UI_LAYER
    int32_t sortOrder = 0;         ///< 渲染优先级（同层内排序，可为负）
    bool screenSpace = true;       ///< 是否使用屏幕空间坐标（正交投影）
    SpriteUI::NineSliceSettings nineSlice{}; ///< 九宫格配置
    bool snapToPixel = false;       ///< 是否对齐像素网格
    Vector2 subPixelOffset{0.0f, 0.0f}; ///< 子像素偏移
    SpriteUI::SpriteFlipFlags flipFlags = SpriteUI::SpriteFlipFlags::None;
    
    bool resourcesLoaded = false;
    bool asyncLoading = false;
    
    SpriteRenderComponent() = default;
};

// ============================================================
// Sprite 动画组件
// ============================================================

struct SpriteAnimationClip {
    std::vector<Rect> frames;             ///< 帧列表（UV 数据）
    float frameDuration = 0.1f;           ///< 单帧持续时间（秒）
    bool loop = true;                     ///< 是否循环（兼容老字段）
    SpritePlaybackMode playbackMode = SpritePlaybackMode::Loop; ///< 播放模式
};

struct SpriteAnimationEvent {
    enum class Type {
        ClipStarted,
        ClipCompleted,
        FrameChanged
    };

    Type type = Type::FrameChanged;
    std::string clip;
    int frameIndex = 0;
};

struct SpriteAnimationTransitionCondition {
    enum class Type {
        Always,
        StateTimeGreater,
        Trigger,
        BoolEquals,
        FloatGreater,
        FloatLess,
        OnEvent
    };

    Type type = Type::Always;
    std::string parameter;
    float threshold = 0.0f;
    bool boolValue = true;
    SpriteAnimationEvent::Type eventType = SpriteAnimationEvent::Type::FrameChanged;
    std::string eventClip;
    int eventFrame = -1;
};

struct SpriteAnimationStateTransition {
    std::string fromState;
    std::string toState;
    std::vector<SpriteAnimationTransitionCondition> conditions;
    bool once = false;
    bool consumed = false;
};

struct SpriteAnimationState {
    std::string name;
    std::string clip;
    float playbackSpeed = 1.0f;
    std::optional<SpritePlaybackMode> playbackMode;
    bool resetOnEnter = true;
    std::vector<std::string> onEnterScripts;
    std::vector<std::string> onExitScripts;
};

struct SpriteAnimationScriptBinding {
    SpriteAnimationEvent::Type eventType = SpriteAnimationEvent::Type::FrameChanged;
    std::string clip;
    int frameIndex = -1;
    std::string scriptName;
};

struct SpriteAnimationStateMachineDebug {
    std::string defaultState;
    std::string currentState;
    std::string currentClip;
    int currentFrame = 0;
    float stateTime = 0.0f;
    float playbackSpeed = 1.0f;
    bool playing = false;
    std::unordered_map<std::string, bool> boolParameters;
    std::unordered_map<std::string, float> floatParameters;
    std::vector<std::string> activeTriggers;
    std::vector<SpriteAnimationState> states;
    std::vector<SpriteAnimationStateTransition> transitions;
    std::vector<SpriteAnimationScriptBinding> scriptBindings;
    std::vector<SpriteAnimationEvent> queuedEvents;
};

struct SpriteAnimationComponent {
    std::unordered_map<std::string, SpriteAnimationClip> clips; ///< 动画剪辑集合
    std::string currentClip;        ///< 当前播放的剪辑名称
    int currentFrame = 0;           ///< 当前帧索引
    float timeInFrame = 0.0f;       ///< 当前帧已播放时间
    float playbackSpeed = 1.0f;     ///< 播放速度倍率
    bool playing = false;           ///< 是否正在播放
    bool dirty = false;             ///< 是否需要立即刷新显示帧
    int playbackDirection = 1;      ///< 当前播放方向（1 正向，-1 反向）
    bool clipJustChanged = false;   ///< 本帧是否刚刚切换动画
    std::vector<SpriteAnimationEvent> events; ///< 帧事件（本帧生成）
    using EventListener = std::function<void(EntityID, const SpriteAnimationEvent&)>;
    std::vector<EventListener> eventListeners; ///< 事件监听器

    std::unordered_map<std::string, SpriteAnimationState> states; ///< 状态机配置
    std::vector<SpriteAnimationStateTransition> transitions; ///< 状态过渡
    std::vector<SpriteAnimationScriptBinding> scriptBindings; ///< 事件脚本绑定
    std::string defaultState;      ///< 默认状态
    std::string currentState;      ///< 当前状态
    float stateTime = 0.0f;        ///< 当前状态已运行时间
    std::unordered_map<std::string, bool> boolParameters; ///< 状态参数（布尔）
    std::unordered_map<std::string, float> floatParameters; ///< 状态参数（浮点）
    std::unordered_set<std::string> triggers; ///< 单次触发参数

    std::vector<SpriteAnimationEvent> debugEventQueue; ///< 调试注入的事件

    SpriteAnimationComponent() = default;

    void Play(const std::string& clipName, bool restart = true) {
        if (!restart && playing && clipName == currentClip) {
            return;
        }
        currentClip = clipName;
        currentFrame = 0;
        timeInFrame = 0.0f;
        playing = true;
        dirty = true;
        clipJustChanged = true;
        playbackDirection = playbackSpeed < 0.0f ? -1 : 1;
    }

    void Stop(bool resetFrame = false) {
        playing = false;
        if (resetFrame) {
            currentFrame = 0;
            timeInFrame = 0.0f;
            dirty = true;
        }
    }

    void SetPlaybackSpeed(float speed) {
        playbackSpeed = speed;
        if (speed < 0.0f) {
            playbackDirection = -1;
        } else if (playbackDirection == 0) {
            playbackDirection = 1;
        }
    }

    [[nodiscard]] bool HasClip(const std::string& clipName) const {
        return clips.find(clipName) != clips.end();
    }

    void ClearEvents() {
        events.clear();
    }

    void AddEventListener(const EventListener& listener) {
        if (listener) {
            eventListeners.push_back(listener);
        }
    }

    void ClearEventListeners() {
        eventListeners.clear();
    }

    void SetBoolParameter(const std::string& name, bool value) {
        boolParameters[name] = value;
    }

    [[nodiscard]] bool GetBoolParameter(const std::string& name, bool defaultValue = false) const {
        if (auto it = boolParameters.find(name); it != boolParameters.end()) {
            return it->second;
        }
        return defaultValue;
    }

    void SetFloatParameter(const std::string& name, float value) {
        floatParameters[name] = value;
    }

    [[nodiscard]] float GetFloatParameter(const std::string& name, float defaultValue = 0.0f) const {
        if (auto it = floatParameters.find(name); it != floatParameters.end()) {
            return it->second;
        }
        return defaultValue;
    }

    void SetTrigger(const std::string& name) {
        triggers.insert(name);
    }

    bool ConsumeTrigger(const std::string& name) {
        auto it = triggers.find(name);
        if (it != triggers.end()) {
            triggers.erase(it);
            return true;
        }
        return false;
    }

    void ResetTrigger(const std::string& name) {
        triggers.erase(name);
    }

    void ClearTriggers() {
        triggers.clear();
    }

    void AddState(const SpriteAnimationState& state) {
        states[state.name] = state;
    }

    void AddTransition(const SpriteAnimationStateTransition& transition) {
        transitions.push_back(transition);
    }

    void AddScriptBinding(const SpriteAnimationScriptBinding& binding) {
        if (!binding.scriptName.empty()) {
            scriptBindings.push_back(binding);
        }
    }

    void SetDefaultState(const std::string& stateName) {
        defaultState = stateName;
    }

    [[nodiscard]] bool HasState(const std::string& stateName) const {
        return states.find(stateName) != states.end();
    }

    [[nodiscard]] std::vector<std::string> GetActiveTriggers() const {
        return std::vector<std::string>(triggers.begin(), triggers.end());
    }

    void QueueDebugEvent(const SpriteAnimationEvent& evt) {
        debugEventQueue.push_back(evt);
    }

    void FlushDebugEvents(std::vector<SpriteAnimationEvent>& target) {
        if (debugEventQueue.empty()) {
            return;
        }
        target.insert(target.end(), debugEventQueue.begin(), debugEventQueue.end());
        debugEventQueue.clear();
    }

    bool ForceState(const std::string& stateName, bool resetTime = true) {
        auto it = states.find(stateName);
        if (it == states.end()) {
            return false;
        }
        const auto& state = it->second;
        currentState = state.name;
        if (resetTime) {
            stateTime = 0.0f;
        }
        playbackSpeed = state.playbackSpeed;

        if (!HasClip(state.clip)) {
            return true;
        }

        auto& clip = clips[state.clip];
        if (state.playbackMode.has_value()) {
            clip.playbackMode = *state.playbackMode;
            clip.loop = (clip.playbackMode == SpritePlaybackMode::Loop);
        }

        Play(state.clip, state.resetOnEnter);
        SetPlaybackSpeed(state.playbackSpeed);
        return true;
    }

    [[nodiscard]] SpriteAnimationStateMachineDebug GetStateMachineDebug() const {
        SpriteAnimationStateMachineDebug debugInfo;
        debugInfo.defaultState = defaultState;
        debugInfo.currentState = currentState;
        debugInfo.currentClip = currentClip;
        debugInfo.currentFrame = currentFrame;
        debugInfo.stateTime = stateTime;
        debugInfo.playbackSpeed = playbackSpeed;
        debugInfo.playing = playing;
        debugInfo.boolParameters = boolParameters;
        debugInfo.floatParameters = floatParameters;
        debugInfo.activeTriggers = GetActiveTriggers();
        debugInfo.transitions = transitions;
        debugInfo.scriptBindings = scriptBindings;
        debugInfo.queuedEvents = debugEventQueue;

        debugInfo.states.reserve(states.size());
        for (const auto& [name, state] : states) {
            debugInfo.states.push_back(state);
        }

        return debugInfo;
    }
};

// ============================================================
// Camera 组件
// ============================================================

/**
 * @brief Camera 组件
 * 
 * 使用 shared_ptr 复用 Camera 对象
 * 
 * **离屏渲染支持**：
 * 通过设置 renderTarget，相机可以渲染到 Framebuffer 而不是屏幕。
 * 这可以用于后处理、阴影贴图、反射等效果。
 * 
 * **使用示例**：
 * ```cpp
 * // 创建离屏渲染目标
 * auto fbo = std::make_shared<Framebuffer>(1024, 1024);
 * fbo->AttachColorTexture();
 * fbo->AttachDepthTexture();
 * 
 * // 设置相机使用该渲染目标
 * cameraComp.renderTarget = fbo;
 * cameraComp.renderTargetName = "shadowMap";  // 可选的名称
 * 
 * // 渲染到纹理后，可以在材质中使用
 * material->SetTexture("shadowMap", fbo->GetColorTexture(0));
 * ```
 */
struct CameraComponent {
    Ref<Camera> camera;            ///< 相机对象（复用）
    
    bool active = true;            ///< 是否激活
    uint32_t layerMask = 0xFFFFFFFF;  ///< 可见层级遮罩
    int32_t depth = 0;             ///< 渲染深度（深度越低越先渲染）
    Color clearColor{0.1f, 0.1f, 0.1f, 1.0f};  ///< 清屏颜色
    bool clearDepth = true;        ///< 是否清除深度缓冲
    bool clearStencil = false;     ///< 是否清除模板缓冲
    
    // ==================== 渲染目标（离屏渲染）====================
    std::string renderTargetName;  ///< 渲染目标名称（可选，用于调试）
    Ref<Framebuffer> renderTarget; ///< 渲染目标（nullptr = 渲染到屏幕）
    
    // ==================== 构造函数 ====================
    
    /// 默认构造函数 - 显式初始化camera为nullptr
    CameraComponent() : camera(nullptr) {}
    
    /// 使用现有Camera对象构造
    explicit CameraComponent(const Ref<Camera>& cam) : camera(cam) {}
    
    // ==================== 便捷方法 ====================
    
    /// 判断是否渲染到离屏目标
    [[nodiscard]] bool IsOffscreen() const { return renderTarget != nullptr; }
    
    /**
     * @brief 检查相机组件是否有效且可用
     * @return 如果相机对象存在且组件激活返回true
     */
    [[nodiscard]] bool IsValid() const {
        return camera != nullptr && active;
    }
    
    /**
     * @brief 验证组件状态（更严格的检查）
     * @return 如果组件完全有效返回true
     * 
     * @note 会检查相机对象、离屏渲染目标等
     */
    [[nodiscard]] bool Validate() const {
        if (!camera) {
            return false;
        }
        // 如果使用离屏渲染，检查framebuffer是否有效
        if (renderTarget) {
            // 注意：Framebuffer可能没有IsValid()方法，这里只检查非空
            // 如果有IsValid()方法，可以取消注释：
            // if (!renderTarget->IsValid()) return false;
        }
        return true;
    }
    
    /**
     * @brief 获取调试信息
     * @return 组件状态的字符串表示
     */
    [[nodiscard]] std::string DebugString() const {
        std::ostringstream oss;
        oss << "CameraComponent{";
        oss << "active=" << active;
        oss << ", camera=" << (camera ? "valid" : "null");
        oss << ", depth=" << depth;
        oss << ", layerMask=0x" << std::hex << layerMask << std::dec;
        if (renderTarget) {
            oss << ", offscreen='" << renderTargetName << "'";
        }
        oss << "}";
        return oss.str();
    }
};

// ============================================================
// Light 组件
// ============================================================

/**
 * @brief 光源类型
 */
enum class LightType {
    Directional,   ///< 定向光
    Point,         ///< 点光源
    Spot,          ///< 聚光灯
    Area           ///< 区域光（未来支持）
};

/**
 * @brief Light 组件
 * 
 * 用于场景光照
 */
struct LightComponent {
    LightType type = LightType::Point;
    
    Color color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    
    // Point/Spot 光源
    float range = 10.0f;
    float attenuation = 1.0f;
    
    // Spot 光源
    float innerConeAngle = 30.0f;  ///< 内角（度）
    float outerConeAngle = 45.0f;  ///< 外角（度）
    
    // 阴影
    bool castShadows = false;
    uint32_t shadowMapSize = 1024;
    float shadowBias = 0.001f;
    
    bool enabled = true;
    
    LightComponent() = default;
};

// ============================================================
// Geometry 几何形状组件
// ============================================================

/**
 * @brief 几何形状类型
 */
enum class GeometryType {
    Cube,       ///< 立方体
    Sphere,     ///< 球体
    Cylinder,   ///< 圆柱体
    Cone,       ///< 圆锥体
    Plane,      ///< 平面
    Quad,       ///< 四边形（2D）
    Torus,      ///< 圆环
    Capsule,    ///< 胶囊体
    Triangle,   ///< 三角形
    Circle      ///< 圆形（2D）
};

/**
 * @brief Geometry 几何形状组件
 * 
 * 用于程序化生成基本几何形状
 * 与 MeshRenderComponent 配合使用
 * 
 * **使用示例**：
 * ```cpp
 * auto entity = world.CreateEntity();
 * 
 * // 添加几何形状组件
 * auto& geom = world.AddComponent<GeometryComponent>(entity);
 * geom.type = GeometryType::Sphere;
 * geom.size = 2.0f;
 * geom.segments = 32;
 * 
 * // 添加网格渲染组件
 * auto& meshRender = world.AddComponent<MeshRenderComponent>(entity);
 * meshRender.materialName = "default";
 * 
 * // GeometrySystem 会自动生成网格并赋值给 meshRender.mesh
 * ```
 */
struct GeometryComponent {
    GeometryType type = GeometryType::Cube;  ///< 几何形状类型
    
    // 通用参数
    float size = 1.0f;                       ///< 大小（缩放因子）
    int segments = 16;                       ///< 分段数（影响精度）
    
    // Sphere/Cylinder/Cone 专用
    int rings = 16;                          ///< 环数（仅用于球体、圆柱等）
    
    // Cylinder/Cone 专用
    float height = 1.0f;                     ///< 高度
    
    // Torus 专用
    float innerRadius = 0.25f;               ///< 内半径
    float outerRadius = 0.5f;                ///< 外半径
    
    // Capsule 专用
    float radius = 0.5f;                     ///< 半径
    float cylinderHeight = 1.0f;             ///< 中间圆柱部分的高度
    
    bool generated = false;                  ///< 是否已生成网格
    
    GeometryComponent() = default;
    explicit GeometryComponent(GeometryType t) : type(t) {}
};

} // namespace ECS
} // namespace Render

