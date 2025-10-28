# 模型文件目录

此目录用于存放 3D 模型文件，供 `MeshLoader` 加载测试使用。

## 支持的格式

渲染引擎通过 **Assimp** 库支持以下 3D 模型格式：

### 常用格式
- **`.obj`** - Wavefront OBJ（最常用，推荐用于测试）
- **`.fbx`** - Autodesk FBX
- **`.gltf` / `.glb`** - GL Transmission Format（现代标准，推荐）
- **`.dae`** - Collada
- **`.blend`** - Blender 原生格式
- **`.pmx` / `.pmd`** - MikuMikuDance (MMD) 模型格式

### 其他支持格式
- **`.3ds`** - 3D Studio
- **`.ply`** - Polygon File Format
- **`.stl`** - Stereolithography（3D打印常用）

## 使用方法

1. 将您的 3D 模型文件放入此目录
2. 运行示例程序 `11_model_loader_test`
3. 程序会自动尝试加载以下路径的模型：
   - `models/test.obj`
   - `models/cube.obj`
   - 其他可能的相对路径

## 代码示例

```cpp
#include <render/mesh_loader.h>

// 加载单个模型文件（可能包含多个网格）
auto meshes = MeshLoader::LoadFromFile("models/my_model.obj");

// 加载模型的第一个网格
auto mesh = MeshLoader::LoadMeshFromFile("models/my_model.obj", 0);

// 渲染
shader->Bind();
for (auto& mesh : meshes) {
    // 设置 uniforms...
    mesh->Draw();
}
```

## 获取测试模型

您可以从以下来源获取免费的测试模型：

### 通用 3D 模型
1. **Blender** - 自带简单模型（立方体、球体等）
2. **Sketchfab** - https://sketchfab.com/
3. **Free3D** - https://free3d.com/
4. **TurboSquid** - https://www.turbosquid.com/Search/3D-Models/free
5. **glTF Sample Models** - https://github.com/KhronosGroup/glTF-Sample-Models

### MMD (PMX/PMD) 模型
6. **BowlRoll** - https://bowlroll.net/ （MMD 模型资源站）
7. **ニコニ立体** - https://3d.nicovideo.jp/
8. **DeviantArt** - 搜索 "MMD model download"
9. **LearnMMD** - https://learnmmd.com/ （教程和资源）

⚠️ **PMX/PMD 使用注意**：
- MMD 模型通常包含材质和纹理信息（当前版本仅提取网格几何）
- 某些模型可能需要配套的纹理文件
- 请遵守模型作者的使用条款和版权

## 注意事项

- 建议使用小型模型进行测试（顶点数 < 10,000）
- OBJ 格式简单易用，推荐初次测试使用
- GLTF/GLB 是现代标准，支持完整的场景层次和材质
- 模型会自动三角化，无需手动处理
- UV 坐标会自动翻转以适应 OpenGL 约定
- 如果模型没有法线，会自动生成平滑法线

## 示例模型结构

推荐的目录结构：
```
models/
  ├── test.obj          # 简单测试模型
  ├── cube.obj          # 立方体
  ├── sphere.obj        # 球体
  ├── character.fbx     # 角色模型
  └── scene.gltf        # 完整场景
```

