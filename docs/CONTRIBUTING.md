# 贡献指南

## 目录
[返回文档首页](README.md)

## 如何贡献

感谢您对 RenderEngine 项目的兴趣！我们欢迎各种形式的贡献。

## 开发环境设置

1. 克隆仓库
```bash
git clone https://github.com/Linductor-alkaid/render.git
cd render
```

2. 安装依赖
- OpenGL
- SDL3
- Eigen3
- CMake 3.15+

3. 编译项目
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 代码风格

### 命名约定
- **类名**: PascalCase (`class Renderer`)
- **函数名**: camelCase (`void updateCamera()`)
- **成员变量**: m_ 前缀 + camelCase (`m_isInitialized`)
- **常量**: UPPER_CASE (`const int MAX_LIGHTS = 8`)
- **命名空间**: lower_case (`namespace render`)

### 代码格式
```cpp
// 良好的代码风格
class MeshRenderer {
public:
    void Render(const Mesh* mesh, const Material* material) {
        if (!mesh || !material) {
            return;
        }
        
        BindMaterial(material);
        BindMesh(mesh);
        glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), 
                      GL_UNSIGNED_INT, nullptr);
    }
    
private:
    void BindMaterial(const Material* material);
    void BindMesh(const Mesh* mesh);
};
```

### 注释规范
```cpp
/**
 * @brief 渲染网格对象
 * 
 * @param mesh 要渲染的网格
 * @param material 要使用的材质
 * @param transform 世界变换矩阵
 * 
 * @note 此函数必须在渲染线程中调用
 */
void RenderMesh(const Mesh* mesh, 
                const Material* material,
                const Matrix4& transform);
```

## 提交 Pull Request

1. Fork 仓库
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 提交信息规范

使用清晰的提交信息：

```
feat: 添加阴影贴图支持

- 实现定向光阴影贴图
- 添加阴影采样器
- 更新着色器以支持阴影
```

### 提交类型
- `feat`: 新功能
- `fix`: 修复bug
- `docs`: 文档更新
- `style`: 代码格式
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试
- `chore`: 杂项

## 测试

运行测试套件：
```bash
cd build
ctest --output-on-failure
```

编写新功能时，请添加相应的测试。

## 文档更新

如果添加新功能，请更新相应文档：

- `docs/README.md` - 功能列表
- `docs/API_REFERENCE.md` - API 文档
- `docs/DEVELOPMENT_GUIDE.md` - 使用示例

## 问题报告

报告问题时，请包含：
- 操作系统
- OpenGL 版本
- 错误日志
- 复现步骤

## 功能请求

功能请求时，请说明：
- 使用场景
- 期望行为
- 可能的实现方案

---

[返回文档首页](README.md)

