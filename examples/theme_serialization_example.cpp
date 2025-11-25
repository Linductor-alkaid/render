/**
 * @file theme_serialization_example.cpp
 * @brief UI主题序列化示例
 * 
 * 演示如何使用JSON序列化工具保存和加载UI主题
 */

#include "render/ui/ui_theme.h"
#include "render/ui/ui_theme_serialization.h"
#include "render/json_serializer.h"
#include "render/logger.h"
#include <iostream>

using namespace Render;
using namespace Render::UI;

void Example1_SaveDefaultTheme() {
    std::cout << "\n=== 示例1: 保存默认主题 ===" << std::endl;
    
    // 创建默认主题
    UITheme theme = UITheme::CreateDefault();
    
    // 保存到文件
    if (UITheme::SaveToJSON(theme, "themes/default.json")) {
        std::cout << "✓ 默认主题已保存到 themes/default.json" << std::endl;
    } else {
        std::cout << "✗ 保存失败！" << std::endl;
    }
}

void Example2_SaveDarkTheme() {
    std::cout << "\n=== 示例2: 保存暗色主题 ===" << std::endl;
    
    // 创建暗色主题
    UITheme theme = UITheme::CreateDark();
    
    // 保存到文件
    if (UITheme::SaveToJSON(theme, "themes/dark.json")) {
        std::cout << "✓ 暗色主题已保存到 themes/dark.json" << std::endl;
    } else {
        std::cout << "✗ 保存失败！" << std::endl;
    }
}

void Example3_LoadTheme() {
    std::cout << "\n=== 示例3: 加载主题 ===" << std::endl;
    
    // 从文件加载主题
    UITheme theme;
    if (UITheme::LoadFromJSON("themes/default.json", theme)) {
        std::cout << "✓ 主题加载成功！" << std::endl;
        std::cout << "  按钮高度: " << theme.sizes.buttonHeight << std::endl;
        std::cout << "  控件单位: " << theme.sizes.widgetUnit << std::endl;
        std::cout << "  字体大小: " << theme.widget.size << std::endl;
        std::cout << "  字体族: " << theme.widget.family << std::endl;
        
        // 显示按钮正常状态的颜色
        const auto& btnNormal = theme.button.normal;
        std::cout << "  按钮正常状态内部颜色: (" 
                  << btnNormal.inner.r << ", "
                  << btnNormal.inner.g << ", "
                  << btnNormal.inner.b << ", "
                  << btnNormal.inner.a << ")" << std::endl;
    } else {
        std::cout << "✗ 加载失败！" << std::endl;
    }
}

void Example4_CreateCustomTheme() {
    std::cout << "\n=== 示例4: 创建并保存自定义主题 ===" << std::endl;
    
    // 基于默认主题创建自定义主题
    UITheme theme = UITheme::CreateDefault();
    
    // 修改颜色
    theme.button.normal.inner = Color(0.8f, 0.9f, 1.0f, 1.0f);  // 淡蓝色
    theme.button.hover.inner = Color(0.7f, 0.85f, 1.0f, 1.0f);   // 稍深的蓝色
    theme.backgroundColor = Color(0.9f, 0.95f, 1.0f, 1.0f);      // 淡蓝色背景
    
    // 修改字体大小
    theme.widget.size = 16.0f;
    theme.widgetLabel.size = 16.0f;
    
    // 修改尺寸
    theme.sizes.buttonHeight = 45.0f;
    theme.sizes.padding = 10.0f;
    
    // 保存自定义主题
    if (UITheme::SaveToJSON(theme, "themes/custom_blue.json")) {
        std::cout << "✓ 自定义蓝色主题已保存到 themes/custom_blue.json" << std::endl;
    } else {
        std::cout << "✗ 保存失败！" << std::endl;
    }
}

void Example5_UseWithThemeManager() {
    std::cout << "\n=== 示例5: 与主题管理器结合使用 ===" << std::endl;
    
    // 初始化默认主题
    UIThemeManager::GetInstance().InitializeDefaults();
    
    // 加载自定义主题
    UITheme customTheme;
    if (UITheme::LoadFromJSON("themes/dark.json", customTheme)) {
        // 注册到主题管理器
        UIThemeManager::GetInstance().RegisterBuiltinTheme("dark", customTheme);
        std::cout << "✓ 暗色主题已注册到主题管理器" << std::endl;
        
        // 切换到暗色主题
        UIThemeManager::GetInstance().SetCurrentTheme("dark");
        std::cout << "✓ 已切换到暗色主题" << std::endl;
        
        // 获取当前主题
        const UITheme& current = UIThemeManager::GetInstance().GetCurrentTheme();
        std::cout << "  当前主题背景色: (" 
                  << current.backgroundColor.r << ", "
                  << current.backgroundColor.g << ", "
                  << current.backgroundColor.b << ")" << std::endl;
    } else {
        std::cout << "✗ 加载主题失败！" << std::endl;
    }
}

void Example6_BasicTypesSerialization() {
    std::cout << "\n=== 示例6: 基础类型序列化 ===" << std::endl;
    
    // 演示Color、Vector等基础类型的序列化
    nlohmann::json j;
    
    // Color
    Color red = Color::Red();
    j["color"] = red;
    std::cout << "Color序列化: " << j["color"].dump() << std::endl;
    
    // Vector3
    Vector3 position(1.0f, 2.0f, 3.0f);
    j["position"] = position;
    std::cout << "Vector3序列化: " << j["position"].dump() << std::endl;
    
    // Rect
    Rect rect(10.0f, 20.0f, 100.0f, 50.0f);
    j["rect"] = rect;
    std::cout << "Rect序列化: " << j["rect"].dump() << std::endl;
    
    // 反序列化
    Color loadedColor = j["color"].get<Color>();
    Vector3 loadedPos = j["position"].get<Vector3>();
    Rect loadedRect = j["rect"].get<Rect>();
    
    std::cout << "✓ 所有基础类型序列化和反序列化成功" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  UI主题序列化示例程序" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 运行所有示例
    Example1_SaveDefaultTheme();
    Example2_SaveDarkTheme();
    Example3_LoadTheme();
    Example4_CreateCustomTheme();
    Example5_UseWithThemeManager();
    Example6_BasicTypesSerialization();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  所有示例执行完成！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}


