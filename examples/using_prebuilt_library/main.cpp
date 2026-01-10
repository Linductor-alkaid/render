// 示例：使用RenderEngine预编译库
// 
// 这个示例展示了如何在其他项目中使用预编译的RenderEngine库
// 编译此示例需要先构建并打包RenderEngine预编译库

#include <render/renderer.h>
#include <render/opengl_context.h>
#include <iostream>

int main() {
    std::cout << "RenderEngine Prebuilt Library Example" << std::endl;
    std::cout << "=====================================" << std::endl;
    
    // 这里可以添加使用RenderEngine的代码
    // 例如创建窗口、初始化渲染器等
    
    std::cout << "预编译库使用成功！" << std::endl;
    std::cout << std::endl;
    std::cout << "提示：" << std::endl;
    std::cout << "1. 确保已正确设置RenderEngine_DIR路径" << std::endl;
    std::cout << "2. 确保已链接OpenGL库" << std::endl;
    std::cout << "3. 查看 docs/PREBUILT_LIBRARY_USAGE.md 获取详细使用说明" << std::endl;
    
    return 0;
}
