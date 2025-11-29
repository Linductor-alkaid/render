# RenderEngine 项目结构图

## 项目整体架构图

```mermaid
graph TB
    %% 用户应用层
    UserApp[用户应用程序] --> AppFramework[应用框架层]

    %% 应用框架层
    subgraph "Application Framework"
        AppFramework --> SceneManager[场景管理器]
        AppFramework --> ModuleRegistry[模块注册表]
        AppFramework --> EventBus[事件总线]

        SceneManager --> Scene[场景接口]
        ModuleRegistry --> CoreRenderModule[核心渲染模块]
        ModuleRegistry --> InputModule[输入模块]
        ModuleRegistry --> UIModule[UI模块]
        ModuleRegistry --> DebugHUDModule[调试HUD模块]
    end

    %% ECS架构层
    subgraph "ECS Architecture"
        ECSWorld[ECS World]
        ECSWorld --> EntityManager[实体管理器]
        ECSWorld --> ComponentRegistry[组件注册表]
        ECSWorld --> Systems[系统集合]

        Systems --> RenderSystem[渲染系统]
        Systems --> AnimationSystem[动画系统]
        Systems --> TransformSystem[变换系统]
        Systems --> UISystem[UI系统]
    end

    %% 核心渲染层
    subgraph "Core Rendering Layer"
        Renderer[主渲染器] --> OpenGLContext[OpenGL上下文]
        Renderer --> RenderState[渲染状态]
        Renderer --> RenderLayer[渲染层级]
        Renderer --> BatchManager[批处理管理器]
        Renderer --> LightManager[光照管理器]
        Renderer --> LODSystem[LOD系统]

        RenderState --> MaterialStateCache[材质状态缓存]
        RenderLayer --> MaterialSortKey[材质排序键]
        BatchManager --> RenderBatch[渲染批次]
        LODSystem --> LODSelector[LOD选择器]
        LODSystem --> LODGenerator[LOD生成器]
        LODSystem --> LODInstancedRenderer[LOD实例化渲染器]
    end

    %% 资源管理层
    subgraph "Resource Management"
        ResourceManager[资源管理器] --> AsyncResourceLoader[异步资源加载器]
        ResourceManager --> ResourceHandle[资源句柄]
        ResourceManager --> ResourceDependency[资源依赖]

        ResourceManager --> ShaderCache[着色器缓存]
        ResourceManager --> TextureLoader[纹理加载器]
        ResourceManager --> MeshLoader[网格加载器]
        ResourceManager --> ModelLoader[模型加载器]
        ResourceManager --> LODGenerator[LOD生成器]
    end

    %% 渲染对象层
    subgraph "Renderable Objects"
        Renderable[可渲染对象基类]

        MeshRenderable[网格渲染对象] --> Renderable
        SpriteRenderable[精灵渲染对象] --> Renderable
        TextRenderable[文本渲染对象] --> Renderable
        ModelRenderable[模型渲染对象] --> Renderable

        MeshRenderable --> Mesh[网格]
        MeshRenderable --> Material[材质]

        SpriteRenderable --> Texture[纹理]
        SpriteRenderable --> SpriteAtlas[精灵图集]

        TextRenderable --> Font[字体]
        TextRenderable --> Texture

        ModelRenderable --> Model[模型]
    end

    %% UI系统
    subgraph "UI System"
        UICanvas[UI画布] --> UIWidgetTree[UI控件树]
        UIWidgetTree --> UIWidget[UI控件基类]

        UIWidget --> UIButton[按钮]
        UIWidget --> UITextField[文本框]
        UIWidget --> UISlider[滑块]
        UIWidget --> UIMenu[菜单]

        UICanvas --> UILayout[UI布局]
        UILayout --> UIRenderCommands[UI渲染命令]
        UIInputRouter[UI输入路由器] --> UIWidget
    end

    %% 精灵系统
    subgraph "Sprite System"
        Sprite[精灵] --> SpriteSheet[精灵表]
        Sprite --> SpriteAtlas
        SpriteAnimator[精灵动画器] --> Sprite
        SpriteBatcher[精灵批处理器] --> Sprite
        SpriteRenderer[精灵渲染器] --> SpriteBatcher
    end

    %% 光照系统
    subgraph "Lighting System"
        Light[光源基类]
        DirectionalLight[定向光] --> Light
        PointLight[点光源] --> Light
        SpotLight[聚光灯] --> Light
        LightManager --> Light
    end

    %% 数学库
    subgraph "Math Library"
        Types[基础类型] --> Vector2[Vector2]
        Types --> Vector3[Vector3]
        Types --> Matrix4[Matrix4]
        Types --> Quaternion[Quaternion]
        Types --> Color[颜色]

        MathUtils[数学工具] --> Transform[变换]
        Transform --> Position[位置]
        Transform --> Rotation[旋转]
        Transform --> Scale[缩放]
    end

    %% 第三方库
    subgraph "Third Party Libraries"
        Eigen[Eigen3 数学库] --> Types
        SDL3[SDL3 窗口/输入] --> OpenGLContext
        SDL3_Image[SDL3_Image] --> TextureLoader
        SDL3_TTF[SDL3_TTF] --> Font
        Assimp[Assimp 模型加载] --> ModelLoader
        GLAD[GLAD OpenGL加载器] --> OpenGLContext
        nlohmann_json[nlohmann JSON] --> SceneSerializer[场景序列化]
    end

    %% 工具类
    subgraph "Utilities"
        Logger[日志系统]
        FileUtils[文件工具]
        Error[错误处理]
        JSONSerializer[JSON序列化]
        GLThreadChecker[OpenGL线程检查器]
    end

    %% 连接关系
    SceneManager --> ECSWorld
    CoreRenderModule --> Renderer
    RenderSystem --> Renderer
    RenderSystem --> LODSystem
    Renderer --> Renderable
    ResourceManager --> Renderable
    ResourceManager --> LODGenerator
    UIModule --> UICanvas
    AnimationSystem --> SpriteAnimator
    CameraSystem --> LODSelector

    style UserApp fill:#e1f5fe
    style Renderer fill:#f3e5f5
    style ECSWorld fill:#e8f5e8
    style ResourceManager fill:#fff3e0
    style UICanvas fill:#fce4ec
```

## ECS架构详细图

```mermaid
graph TB
    subgraph "ECS World"
        World[ECS World]

        subgraph "Entity Management"
            EntityManager[实体管理器]
            EntityID[实体ID]
            Entity[实体]
        end

        subgraph "Component Management"
            ComponentRegistry[组件注册表]
            ComponentPool[组件池]
            ComponentType[组件类型]
        end

        subgraph "System Management"
            SystemRegistry[系统注册表]
            System[系统基类]
            SystemPriority[系统优先级]
        end

        World --> EntityManager
        World --> ComponentRegistry
        World --> SystemRegistry

        EntityManager --> EntityID
        EntityManager --> Entity

        ComponentRegistry --> ComponentPool
        ComponentRegistry --> ComponentType

        SystemRegistry --> System
        SystemRegistry --> SystemPriority
    end

    subgraph "Core Components"
        TransformComponent[变换组件]
        MeshRenderComponent[网格渲染组件]
        SpriteRenderComponent[精灵渲染组件]
        CameraComponent[相机组件]
        LightComponent[光源组件]
        UIComponent[UI组件]
        AnimationComponent[动画组件]
        LODComponent[LOD组件]

        TransformComponent --> ComponentType
        MeshRenderComponent --> ComponentType
        SpriteRenderComponent --> ComponentType
        CameraComponent --> ComponentType
        LightComponent --> ComponentType
        UIComponent --> ComponentType
        AnimationComponent --> ComponentType
        LODComponent --> ComponentType
    end

    subgraph "Core Systems"
        RenderSystem[渲染系统]
        AnimationSystem[动画系统]
        TransformSystem[变换系统]
        CameraSystem[相机系统]
        LightSystem[光照系统]
        UISystem[UI系统]
        ScriptSystem[脚本系统]

        RenderSystem --> System
        AnimationSystem --> System
        TransformSystem --> System
        CameraSystem --> System
        LightSystem --> System
        UISystem --> System
        ScriptSystem --> System
    end

    Entity --> ComponentPool
    System --> Entity
    System --> ComponentPool

    style World fill:#e8f5e8
    style TransformComponent fill:#e1f5fe
    style RenderSystem fill:#fff3e0
```

## 渲染管线流程图

```mermaid
graph LR
    subgraph "Application Update"
        AppUpdate[应用更新] --> SceneUpdate[场景更新]
        SceneUpdate --> ECSUpdate[ECS系统更新]
    end

    subgraph "Culling & Batching"
        FrustumCulling[视锥剔除] --> DistanceCulling[距离剔除]
        DistanceCulling --> LODCalculation[LOD计算]
        LODCalculation --> MaterialSorting[材质排序]
        MaterialSorting --> LayerSorting[层级排序]
        LayerSorting --> Batching[批处理]
    end

    subgraph "Rendering Pipeline"
        BeginFrame[开始帧] --> ClearBuffers[清空缓冲区]
        ClearBuffers --> ShadowPass[阴影渲染通道]
        ShadowPass --> GeometryPass[几何渲染通道]
        GeometryPass --> LightingPass[光照渲染通道]
        LightingPass --> TransparentPass[透明渲染通道]
        TransparentPass --> UIPass[UI渲染通道]
        UIPass --> PostProcessing[后处理]
        PostProcessing --> Present[呈现]
    end

    subgraph "Batching Strategies"
        Batching --> BatchMode{批处理模式}
        BatchMode --> Disabled[禁用批处理]
        BatchMode --> CPUMerge[CPU合批]
        BatchMode --> GPUInstancing[GPU实例化]
    end

    ECSUpdate --> FrustumCulling
    LODCalculation --> MaterialSorting
    Batching --> BeginFrame

    Disabled --> GeometryPass
    CPUMerge --> GeometryPass
    GPUInstancing --> GeometryPass

    style BeginFrame fill:#f3e5f5
    style ShadowPass fill:#e8f5e8
    style GeometryPass fill:#fff3e0
    style LightingPass fill:#e1f5fe
    style UIPass fill:#fce4ec
```

## 资源管理系统图

```mermaid
graph TB
    subgraph "Resource Management"
        ResourceManager[资源管理器]
        ResourceCache[资源缓存]
        ResourceFactory[资源工厂]
        ResourceLoader[资源加载器]
        ResourceRef[资源引用]
    end

    subgraph "Resource Types"
        TextureResource[纹理资源]
        MeshResource[网格资源]
        ShaderResource[着色器资源]
        MaterialResource[材质资源]
        ModelResource[模型资源]
        FontResource[字体资源]
        AudioResource[音频资源]
    end

    subgraph "Async Loading"
        AsyncLoader[异步加载器]
        LoadQueue[加载队列]
        WorkerThread[工作线程]
        LoadCallback[加载回调]
        LoadProgress[加载进度]
    end

    subgraph "Resource Dependencies"
        DependencyGraph[依赖图]
        DependencyResolver[依赖解析器]
        CircularReferenceDetector[循环引用检测]
        LoadOrder[加载顺序]
    end

    ResourceManager --> ResourceCache
    ResourceManager --> ResourceFactory
    ResourceManager --> AsyncLoader

    ResourceFactory --> ResourceLoader
    ResourceFactory --> ResourceRef

    ResourceLoader --> TextureResource
    ResourceLoader --> MeshResource
    ResourceLoader --> ShaderResource
    ResourceLoader --> MaterialResource
    ResourceLoader --> ModelResource
    ResourceLoader --> FontResource
    ResourceLoader --> AudioResource

    AsyncLoader --> LoadQueue
    AsyncLoader --> WorkerThread
    AsyncLoader --> LoadCallback
    AsyncLoader --> LoadProgress

    ResourceManager --> DependencyGraph
    DependencyGraph --> DependencyResolver
    DependencyResolver --> CircularReferenceDetector
    DependencyResolver --> LoadOrder

    style ResourceManager fill:#fff3e0
    style AsyncLoader fill:#e1f5fe
    style DependencyGraph fill:#f3e5f5
```

## UI系统架构图

```mermaid
graph TB
    subgraph "UI System Core"
        UICanvas[UI画布] --> UIInputRouter[UI输入路由器]
        UICanvas --> UIWidgetTree[UI控件树]
        UICanvas --> UIRenderer[UI渲染器]
    end

    subgraph "Layout System"
        UILayoutEngine[布局引擎]
        FlexLayout[Flex布局]
        GridLayout[Grid布局]
        AbsoluteLayout[绝对布局]
        ConstraintsLayout[约束布局]

        UILayoutEngine --> FlexLayout
        UILayoutEngine --> GridLayout
        UILayoutEngine --> AbsoluteLayout
        UILayoutEngine --> ConstraintsLayout
    end

    subgraph "Widget Hierarchy"
        UIWidget[UI控件基类]
        UIContainer[UI容器控件]
        UIButton[按钮控件]
        UITextField[文本框控件]
        UISlider[滑块控件]
        UICheckbox[复选框控件]
        UIMenu[菜单控件]
        UIList[列表控件]
        UIPanel[面板控件]
    end

    subgraph "Rendering System"
        UIRenderCommands[UI渲染命令]
        UIBatchRenderer[UI批处理器]
        UIAtlas[UI图集]
        UIFontRenderer[字体渲染器]
        UIThemeSystem[主题系统]
    end

    subgraph "Event System"
        UIEvent[UI事件]
        UIEventHandler[事件处理器]
        UIEventDispatcher[事件分发器]
        UIGestureRecognizer[手势识别器]
    end

    UIWidgetTree --> UILayoutEngine
    UIWidgetTree --> UIWidget
    UIWidget --> UIContainer
    UIWidget --> UIButton
    UIWidget --> UITextField
    UIWidget --> UISlider
    UIWidget --> UICheckbox
    UIWidget --> UIMenu
    UIWidget --> UIList
    UIWidget --> UIPanel

    UIRenderer --> UIRenderCommands
    UIRenderer --> UIBatchRenderer
    UIRenderer --> UIAtlas
    UIRenderer --> UIFontRenderer
    UIRenderer --> UIThemeSystem

    UIInputRouter --> UIEventDispatcher
    UIEventDispatcher --> UIEventHandler
    UIEventDispatcher --> UIGestureRecognizer
    UIEventHandler --> UIEvent

    style UICanvas fill:#fce4ec
    style UILayoutEngine fill:#e1f5fe
    style UIWidget fill:#f3e5f5
    style UIRenderer fill:#fff3e0
```

## 精灵系统架构图

```mermaid
graph TB
    subgraph "Sprite Core"
        Sprite[精灵对象] --> SpriteSheet[精灵表]
        Sprite --> SpriteAtlas[精灵图集]
        Sprite --> SpriteAnimator[精灵动画器]
    end

    subgraph "Animation System"
        AnimationClip[动画剪辑]
        AnimationState[动画状态]
        AnimationStateMachine[状态机]
        AnimationTransition[状态转换]
        AnimationEvent[动画事件]

        SpriteAnimator --> AnimationClip
        SpriteAnimator --> AnimationStateMachine
        AnimationStateMachine --> AnimationState
        AnimationStateMachine --> AnimationTransition
        AnimationStateMachine --> AnimationEvent
    end

    subgraph "Batching System"
        SpriteBatcher[精灵批处理器]
        SpriteBatch[精灵批次]
        BatchVertex[批处理顶点]
        BatchIndex[批处理索引]
        InstanceData[实例数据]
    end

    subgraph "Rendering Pipeline"
        SpriteRenderer[精灵渲染器]
        SpriteShader[精灵着色器]
        NineSlice[九宫格渲染]
        TilingSprite[平铺精灵]
        ParticleSprite[粒子精灵]
    end

    subgraph "Import System"
        SpriteAtlasImporter[精灵图集导入器]
        TexturePacker[纹理打包器]
        SpriteSheetGenerator[精灵表生成器]
        MetadataParser[元数据解析器]
    end

    Sprite --> SpriteBatcher
    SpriteBatcher --> SpriteBatch
    SpriteBatch --> BatchVertex
    SpriteBatch --> BatchIndex
    SpriteBatch --> InstanceData

    SpriteRenderer --> SpriteBatch
    SpriteRenderer --> SpriteShader
    SpriteRenderer --> NineSlice
    SpriteRenderer --> TilingSprite
    SpriteRenderer --> ParticleSprite

    SpriteAtlasImporter --> TexturePacker
    SpriteAtlasImporter --> SpriteSheetGenerator
    SpriteAtlasImporter --> MetadataParser

    style Sprite fill:#fff3e0
    style SpriteAnimator fill:#e1f5fe
    style SpriteBatcher fill:#f3e5f5
    style SpriteRenderer fill:#e8f5e8
```

## 场景管理系统图

```mermaid
graph TB
    subgraph "Scene Management Core"
        SceneManager[场景管理器]
        SceneStack[场景栈]
        SceneFactory[场景工厂]
        SceneTransition[场景转换]
    end

    subgraph "Scene Types"
        Scene[场景基类]
        BootScene[启动场景]
        MenuScene[菜单场景]
        GameScene[游戏场景]
        LoadingScene[加载场景]
        CutsceneScene[过场动画场景]
    end

    subgraph "Scene Graph"
        SceneNode[场景节点]
        SceneNodeComponent[场景节点组件]
        SpatialHash[空间哈希]
        Octree[八叉树]
        BspTree[BSP树]
    end

    subgraph "Scene Serialization"
        SceneSerializer[场景序列化器]
        SceneDeserializer[场景反序列化器]
        SceneSnapshot[场景快照]
        SceneManifest[场景清单]
        JSONFormat[JSON格式]
        BinaryFormat[二进制格式]
    end

    subgraph "Resource Preloading"
        ResourcePreloader[资源预加载器]
        LoadProgress[加载进度]
        RequiredResources[必需资源]
        OptionalResources[可选资源]
        StreamingAssets[流式资源]
    end

    SceneManager --> SceneStack
    SceneManager --> SceneFactory
    SceneManager --> SceneTransition

    SceneFactory --> Scene
    Scene --> BootScene
    Scene --> MenuScene
    Scene --> GameScene
    Scene --> LoadingScene
    Scene --> CutsceneScene

    Scene --> SceneNode
    SceneNode --> SceneNodeComponent
    SceneNode --> SpatialHash
    SceneNode --> Octree
    SceneNode --> BspTree

    SceneManager --> SceneSerializer
    SceneSerializer --> SceneDeserializer
    SceneSerializer --> SceneSnapshot
    SceneSerializer --> SceneManifest
    SceneSerializer --> JSONFormat
    SceneSerializer --> BinaryFormat

    SceneManager --> ResourcePreloader
    ResourcePreloader --> LoadProgress
    ResourcePreloader --> RequiredResources
    ResourcePreloader --> OptionalResources
    ResourcePreloader --> StreamingAssets

    style SceneManager fill:#f3e5f5
    style Scene fill:#e1f5fe
    style SceneNode fill:#e8f5e8
    style SceneSerializer fill:#fff3e0
    style ResourcePreloader fill:#fce4ec
```

## 模块依赖关系图

```mermaid
graph TB
    subgraph "Application Layer"
        AppModule[应用模块基类]
        CoreRenderModule[核心渲染模块]
        InputModule[输入模块]
        UIModule[UI模块]
        DebugHUDModule[调试HUD模块]
    end

    subgraph "Core Systems"
        Renderer[渲染器]
        ECSWorld[ECS世界]
        ResourceManager[资源管理器]
        SceneManager[场景管理器]
        EventBus[事件总线]
    end

    subgraph "Rendering Subsystems"
        OpenGLContext[OpenGL上下文]
        RenderState[渲染状态]
        ShaderSystem[着色器系统]
        TextureSystem[纹理系统]
        MeshSystem[网格系统]
        LightSystem[光照系统]
    end

    subgraph "Utility Systems"
        Logger[日志系统]
        FileSystem[文件系统]
        MemoryPool[内存池]
        ThreadPool[线程池]
        ConfigSystem[配置系统]
    end

    AppModule --> Renderer
    AppModule --> ECSWorld
    AppModule --> ResourceManager
    AppModule --> SceneManager
    AppModule --> EventBus

    CoreRenderModule --> Renderer
    InputModule --> EventBus
    UIModule --> Renderer
    UIModule --> EventBus
    DebugHUDModule --> Renderer

    Renderer --> OpenGLContext
    Renderer --> RenderState
    Renderer --> ShaderSystem
    Renderer --> TextureSystem
    Renderer --> MeshSystem
    Renderer --> LightSystem

    ECSWorld --> ResourceManager
    SceneManager --> ECSWorld
    SceneManager --> ResourceManager
    ResourceManager --> FileSystem
    ResourceManager --> MemoryPool

    Renderer --> ThreadPool
    ResourceManager --> ThreadPool
    EventBus --> Logger

    style AppModule fill:#e1f5fe
    style Renderer fill:#f3e5f5
    style ECSWorld fill:#e8f5e8
    style ResourceManager fill:#fff3e0
    style EventBus fill:#fce4ec
```

## 数据流向图

```mermaid
flowchart TD
    %% 初始化阶段
    InitStart[应用启动] --> CreateRenderer[创建渲染器]
    CreateRenderer --> InitOpenGL[初始化OpenGL]
    InitOpenGL --> CreateResourceManager[创建资源管理器]
    CreateResourceManager --> CreateECSWorld[创建ECS世界]
    CreateECSWorld --> LoadInitialScene[加载初始场景]

    %% 游戏循环
    GameLoop[游戏循环] --> HandleInput[处理输入]
    HandleInput --> UpdateScene[更新场景]
    UpdateScene --> UpdateECS[更新ECS系统]
    UpdateECS --> CullObjects[剔除对象]
    CullObjects --> CalculateLOD[计算LOD级别]
    CalculateLOD --> SortRenderables[排序渲染对象]
    SortRenderables --> BatchRenderables[批处理渲染对象]
    BatchRenderables --> RenderFrame[渲染帧]
    RenderFrame --> PresentFrame[呈现帧]
    PresentFrame --> GameLoop

    %% 资源加载流程
    LoadResource[请求加载资源] --> CheckCache{检查缓存}
    CheckCache -->|命中| ReturnResource[返回资源]
    CheckCache -->|未命中| LoadFromFile[从文件加载]
    LoadFromFile --> ProcessResource[处理资源]
    ProcessResource --> CacheResource[缓存资源]
    CacheResource --> ReturnResource

    %% 场景切换流程
    SceneTransition[场景切换] --> UnloadCurrentScene[卸载当前场景]
    UnloadCurrentScene --> PreloadNewScene[预加载新场景]
    PreloadNewScene --> CheckResourcesReady{资源准备完成?}
    CheckResourcesReady -->|否| WaitLoading[等待加载]
    WaitLoading --> CheckResourcesReady
    CheckResourcesReady -->|是| InitializeNewScene[初始化新场景]
    InitializeNewScene --> ActivateScene[激活场景]

    %% 渲染流程
    RenderStart[开始渲染] --> ShadowPass[阴影通道]
    ShadowPass --> GeometryPass[几何通道]
    GeometryPass --> LightingPass[光照通道]
    LightingPass --> TransparentPass[透明通道]
    TransparentPass --> UIPass[UI通道]
    UIPass --> PostProcessing[后处理]
    PostProcessing --> RenderEnd[结束渲染]

    style InitStart fill:#e1f5fe
    style GameLoop fill:#e8f5e8
    style LoadResource fill:#fff3e0
    style SceneTransition fill:#f3e5f5
    style RenderStart fill:#fce4ec
```

## LOD系统架构图

```mermaid
graph TB
    subgraph "LOD System Core"
        LODComponent[LOD组件] --> LODConfig[LOD配置]
        LODConfig --> DistanceThresholds[距离阈值]
        LODConfig --> LODResources[LOD资源]
        LODResources --> LODMeshes[LOD网格]
        LODResources --> LODMaterials[LOD材质]
        LODResources --> LODTextures[LOD纹理]
        
        LODSelector[LOD选择器] --> BatchCalculateLOD[批量计算LOD]
        LODSelector --> CalculateDistance[计算距离]
        BatchCalculateLOD --> LODComponent
    end

    subgraph "LOD Generation"
        LODGenerator[LOD生成器] --> MeshSimplifier[网格简化器]
        MeshSimplifier --> MeshOptimizer[meshoptimizer库]
        
        LODGenerator --> GenerateLODLevels[生成LOD级别]
        LODGenerator --> GenerateModelLOD[生成模型LOD]
        LODGenerator --> BatchGenerateLOD[批量生成LOD]
        
        LODGenerator --> SaveLODFiles[保存LOD文件]
        LODGenerator --> LoadLODFiles[加载LOD文件]
    end

    subgraph "Texture LOD"
        TextureMipmap[纹理Mipmap]
        AutoConfigureTexture[自动配置纹理]
        TextureLODStrategy[纹理LOD策略]
        
        TextureMipmap --> AutoConfigureTexture
        AutoConfigureTexture --> TextureLODStrategy
        TextureLODStrategy --> UseMipmap[使用Mipmap]
        TextureLODStrategy --> UseLODTextures[使用LOD纹理]
    end

    subgraph "LOD Rendering"
        LODInstancedRenderer[LOD实例化渲染器] --> GroupByLOD[按LOD分组]
        GroupByLOD --> InstanceBatching[实例批处理]
        InstanceBatching --> GPURendering[GPU渲染]
        
        MeshRenderSystem[网格渲染系统] --> BatchCalculateLOD
        MeshRenderSystem --> LODInstancedRenderer
        MeshRenderSystem --> SelectLODResource[选择LOD资源]
        SelectLODResource --> LODMeshes
        SelectLODResource --> LODMaterials
    end

    subgraph "Integration"
        ECSWorld[ECS世界] --> LODComponent
        CameraSystem[相机系统] --> CameraPosition[相机位置]
        CameraPosition --> BatchCalculateLOD
        TransformComponent[变换组件] --> EntityPosition[实体位置]
        EntityPosition --> CalculateDistance
    end

    MeshSimplifier --> GenerateLODLevels
    GenerateLODLevels --> LODMeshes
    AutoConfigureTexture --> LODTextures
    BatchCalculateLOD --> SelectLODResource

    style LODComponent fill:#e1f5fe
    style LODGenerator fill:#fff3e0
    style LODSelector fill:#f3e5f5
    style LODInstancedRenderer fill:#e8f5e8
```

## LOD工作流程图

```mermaid
flowchart TD
    %% LOD资源准备阶段
    Start[开始] --> LoadSourceMesh[加载源网格]
    LoadSourceMesh --> ChooseMethod{选择LOD生成方式}
    
    ChooseMethod -->|自动生成| AutoGenerate[LODGenerator自动生成]
    ChooseMethod -->|手动加载| ManualLoad[手动加载LOD文件]
    
    AutoGenerate --> GenerateLOD1[生成LOD1网格]
    GenerateLOD1 --> GenerateLOD2[生成LOD2网格]
    GenerateLOD2 --> GenerateLOD3[生成LOD3网格]
    GenerateLOD3 --> SaveLOD[保存LOD文件可选]
    
    ManualLoad --> LoadLOD0[加载LOD0网格]
    LoadLOD0 --> LoadLOD1[加载LOD1网格]
    LoadLOD1 --> LoadLOD2[加载LOD2网格]
    LoadLOD2 --> LoadLOD3[加载LOD3网格]
    
    SaveLOD --> ConfigureLOD
    LoadLOD3 --> ConfigureLOD[配置LODComponent]
    
    %% LOD配置阶段
    ConfigureLOD --> SetThresholds[设置距离阈值]
    SetThresholds --> SetResources[设置LOD资源]
    SetResources --> ConfigTexture[配置纹理LOD]
    ConfigTexture --> AddToEntity[添加到实体]
    
    %% 运行时阶段
    AddToEntity --> RuntimeLoop[运行时循环]
    RuntimeLoop --> GetCameraPos[获取相机位置]
    GetCameraPos --> BatchCalculate[批量计算LOD级别]
    
    BatchCalculate --> CalcDistance[计算实体到相机距离]
    CalcDistance --> CompareThresholds[比较距离阈值]
    CompareThresholds --> SelectLOD{选择LOD级别}
    
    SelectLOD -->|距离 < 阈值1| LOD0[使用LOD0]
    SelectLOD -->|阈值1 <= 距离 < 阈值2| LOD1[使用LOD1]
    SelectLOD -->|阈值2 <= 距离 < 阈值3| LOD2[使用LOD2]
    SelectLOD -->|阈值3 <= 距离 < 阈值4| LOD3[使用LOD3]
    SelectLOD -->|距离 >= 阈值4| Culled[剔除]
    
    LOD0 --> UpdateComponent[更新LODComponent.currentLOD]
    LOD1 --> UpdateComponent
    LOD2 --> UpdateComponent
    LOD3 --> UpdateComponent
    Culled --> SkipRender[跳过渲染]
    
    UpdateComponent --> SelectResource[选择对应LOD资源]
    SelectResource --> Render[渲染]
    Render --> RuntimeLoop
    
    style Start fill:#e1f5fe
    style AutoGenerate fill:#fff3e0
    style ConfigureLOD fill:#f3e5f5
    style BatchCalculate fill:#e8f5e8
    style Render fill:#fce4ec
```

## 总结

RenderEngine 项目采用了现代软件工程的最佳实践，具有以下特点：

### 架构优势
1. **分层设计**: 清晰的层次结构，职责分离明确
2. **模块化**: 高内聚低耦合的模块设计，便于维护和扩展
3. **ECS架构**: 灵活的实体组件系统，支持复杂游戏逻辑
4. **批处理优化**: 多种批处理策略，提升渲染性能
5. **异步加载**: 非阻塞的资源加载系统，改善用户体验
6. **LOD系统**: 完整的细节层次系统，支持自动网格简化和纹理LOD，显著优化渲染性能

### 技术特色
1. **现代C++**: 充分利用C++20特性，类型安全且性能优异
2. **跨平台**: 基于SDL3的跨平台支持
3. **内存安全**: 智能指针和RAII模式，避免内存泄漏
4. **线程安全**: 多线程环境下的安全设计
5. **可扩展性**: 插件式架构，易于添加新功能

这个项目结构为实时渲染应用提供了坚实的基础，适合作为学习现代渲染引擎架构的参考，也可以直接用于实际项目开发。