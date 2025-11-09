# Skeleton API 参考

[返回 API 首页](README.md)

---

## 概述

`Skeleton` 提供对 `MeshSkinningData` 的高层封装，负责：

- 将 `MeshLoader::LoadDetailedFromFile()` 解析得到的骨骼结构转换为层级索引。
- 根据局部姿势矩阵计算世界矩阵、生成 GPU 蒙皮调色板。
- 提供按名称查询骨骼索引、访问父子层级等便捷接口。

**头文件**: `render/skinning.h`  
**命名空间**: `Render`

---

## 相关类型

### SkeletonBone

```cpp
struct SkeletonBone {
    std::string name;
    int32_t parentIndex = -1;
    Matrix4 offsetMatrix = Matrix4::Identity();
};
```

- `name`：骨骼名称。
- `parentIndex`：父骨骼索引，`-1` 表示根骨骼。
- `offsetMatrix`：绑定姿势逆矩阵（`mesh` 顶点 → 骨骼空间）。

### SkeletonPose

```cpp
struct SkeletonPose {
    std::vector<Matrix4> localTransforms;
    void Resize(size_t count);
    size_t Size() const;
};
```

- `localTransforms`：局部姿势矩阵数组，与骨骼顺序一致。
- `Resize()`：扩展并使用单位矩阵初始化缺失元素。

### Skeleton

```cpp
class Skeleton {
public:
    static Skeleton FromSkinningData(const MeshSkinningData& data);
    size_t GetBoneCount() const;
    const std::vector<SkeletonBone>& GetBones() const;
    const SkeletonBone& GetBone(size_t index) const;
    std::optional<size_t> FindBoneIndex(const std::string& name) const;
    const std::vector<std::vector<size_t>>& GetChildren() const;
    void EvaluateWorldTransforms(const SkeletonPose& pose,
                                 std::vector<Matrix4>& outWorld) const;
    void BuildSkinningPalette(const SkeletonPose& pose,
                              std::vector<Matrix4>& outPalette) const;
};
```

- `FromSkinningData()`：读取 `MeshSkinningData`，建立骨骼层级与名称映射。
- `GetChildren()`：返回每个骨骼的子骨骼列表，便于遍历或调试。
- `EvaluateWorldTransforms()`：计算所有骨骼的世界矩阵。
- `BuildSkinningPalette()`：生成 GPU 蒙皮调色板（世界矩阵 × offset）。

---

## 使用示例

```cpp
auto& part = modelParts[0];
if (part.extraData && part.extraData->skinning.HasBones()) {
    Skeleton skeleton = Skeleton::FromSkinningData(part.extraData->skinning);

    SkeletonPose pose;
    pose.Resize(skeleton.GetBoneCount());
    // TODO: 填充 pose.localTransforms（来自动画系统或默认姿势）

    std::vector<Matrix4> skinningPalette;
    skeleton.BuildSkinningPalette(pose, skinningPalette);

    // 将 skinningPalette 上传到 GPU 作为统一缓冲或纹理
}
```

---

[返回 API 首页](README.md)
