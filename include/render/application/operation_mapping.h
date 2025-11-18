#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "render/application/events/input_events.h"

namespace Render::Application {

// 快捷键组合
struct KeyCombo {
    int scancode = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    
    bool operator==(const KeyCombo& other) const {
        return scancode == other.scancode && 
               ctrl == other.ctrl && 
               shift == other.shift && 
               alt == other.alt;
    }
};

// 快捷键哈希函数
struct KeyComboHash {
    size_t operator()(const KeyCombo& combo) const {
        return std::hash<int>()(combo.scancode) ^
               (std::hash<bool>()(combo.ctrl) << 1) ^
               (std::hash<bool>()(combo.shift) << 2) ^
               (std::hash<bool>()(combo.alt) << 3);
    }
};

// 快捷键上下文
class ShortcutContext {
public:
    explicit ShortcutContext(std::string_view name) : m_name(name) {}
    
    std::string_view GetName() const { return m_name; }
    
    // 注册快捷键到操作
    void RegisterShortcut(const KeyCombo& combo, Events::OperationType operation);
    
    // 检查快捷键是否已注册
    bool HasShortcut(const KeyCombo& combo) const;
    
    // 获取快捷键对应的操作
    Events::OperationType GetOperation(const KeyCombo& combo) const;
    
    // 获取操作的所有快捷键
    std::vector<KeyCombo> GetShortcuts(Events::OperationType operation) const;
    
    // 检查快捷键冲突
    bool CheckConflict(const KeyCombo& combo, Events::OperationType operation) const;
    
private:
    std::string m_name;
    std::unordered_map<KeyCombo, Events::OperationType, KeyComboHash> m_shortcuts;
    std::unordered_map<Events::OperationType, std::vector<KeyCombo>> m_operations;
};

// 操作映射管理器
class OperationMappingManager {
public:
    OperationMappingManager();
    
    // 添加快捷键上下文
    void AddContext(std::shared_ptr<ShortcutContext> context);
    
    // 设置当前上下文
    void SetCurrentContext(std::string_view contextName);
    
    // 获取当前上下文
    std::shared_ptr<ShortcutContext> GetCurrentContext() const;
    
    // 获取上下文
    std::shared_ptr<ShortcutContext> GetContext(std::string_view contextName) const;
    
    // 根据快捷键获取操作
    Events::OperationType GetOperationFromKey(const KeyCombo& combo) const;
    
    // 初始化Blender风格默认映射
    void InitializeBlenderDefaults();
    
private:
    std::unordered_map<std::string, std::shared_ptr<ShortcutContext>> m_contexts;
    std::string m_currentContext;
    
    // 辅助函数：创建Blender风格快捷键
    KeyCombo MakeCombo(int scancode, bool ctrl = false, bool shift = false, bool alt = false);
};

} // namespace Render::Application

