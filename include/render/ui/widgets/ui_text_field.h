#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_scancode.h>

#include "render/ui/ui_widget.h"

namespace Render::UI {

class UITextField : public UIWidget {
public:
    using TextChangedHandler = std::function<void(UITextField&, const std::string&)>;

    static constexpr float kPaddingLeft = 12.0f;
    static constexpr float kPaddingRight = 12.0f;
    static constexpr float kPaddingTop = 10.0f;
    static constexpr float kPaddingBottom = 10.0f;
    static constexpr float kCaretWidth = 2.0f;

    explicit UITextField(std::string id);

    void SetText(std::string text);
    [[nodiscard]] const std::string& GetText() const noexcept { return m_text; }

    void SetPlaceholder(std::string placeholder);
    [[nodiscard]] const std::string& GetPlaceholder() const noexcept { return m_placeholder; }

    void SetReadOnly(bool readOnly) noexcept;
    [[nodiscard]] bool IsReadOnly() const noexcept { return m_readOnly; }

    void SetSelectAllOnFocus(bool enabled) noexcept { m_selectAllOnFocus = enabled; }
    [[nodiscard]] bool GetSelectAllOnFocus() const noexcept { return m_selectAllOnFocus; }

    void SetMaxLength(size_t bytes) noexcept { m_maxLength = bytes; }
    [[nodiscard]] size_t GetMaxLength() const noexcept { return m_maxLength; }

    void SetOnTextChanged(TextChangedHandler handler) { m_onTextChanged = std::move(handler); }

    [[nodiscard]] bool IsFocused() const noexcept { return m_focused; }

    [[nodiscard]] size_t GetCaretIndex() const noexcept { return m_caretIndex; }
    [[nodiscard]] size_t GetAnchorIndex() const noexcept { return m_anchorIndex; }
    [[nodiscard]] size_t GetCaretByteOffset() const noexcept;
    [[nodiscard]] bool HasSelection() const noexcept { return m_caretIndex != m_anchorIndex; }
    [[nodiscard]] std::pair<size_t, size_t> GetSelectionIndices() const noexcept;
    [[nodiscard]] std::pair<size_t, size_t> GetSelectionByteRange() const noexcept;
    [[nodiscard]] const std::vector<size_t>& GetCodepointOffsets() const noexcept { return m_codepointOffsets; }
    [[nodiscard]] const std::vector<float>& GetCachedCaretPositions() const noexcept { return m_cachedCaretPositions; }
    [[nodiscard]] float GetCachedTextHeight() const noexcept { return m_cachedTextHeight; }

    void ClearSelection();
    void SelectAll();
    void UpdateCaretMetrics(const std::vector<float>& caretXPositions, float textHeight);

protected:
    void OnFocusGained() override;
    void OnFocusLost() override;
    void OnMouseClick(uint8_t button, const Vector2& position) override;
    void OnTextInput(const std::string& text) override;
    void OnKey(int scancode, bool pressed, bool repeat) override;

private:
    void NotifyTextChanged();
    void MoveCaretLeft(bool extendSelection, bool jumpWord);
    void MoveCaretRight(bool extendSelection, bool jumpWord);
    void MoveCaretToStart(bool extendSelection);
    void MoveCaretToEnd(bool extendSelection);
    void DeletePrevious();
    void DeleteNext();
    bool DeleteSelection();
    void InsertText(const std::string& text);
    void SetCaretIndex(size_t index, bool keepAnchor);
    void SetCaretByByteOffset(size_t byteOffset, bool keepAnchor);
    [[nodiscard]] size_t ClampCaretIndex(size_t index) const noexcept;
    void RebuildCodepointOffsets();
    void EnsureCaretWithinBounds();
    [[nodiscard]] size_t OffsetToIndex(size_t byteOffset) const noexcept;
    [[nodiscard]] size_t IndexToOffset(size_t index) const noexcept;
    [[nodiscard]] uint32_t CodepointAt(size_t index) const noexcept;
    [[nodiscard]] size_t FindPreviousWordIndex(size_t startIndex) const noexcept;
    [[nodiscard]] size_t FindNextWordIndex(size_t startIndex) const noexcept;
    [[nodiscard]] size_t HitTestCaretIndex(float localX) const noexcept;
    [[nodiscard]] std::string_view GetSelectionTextView() const noexcept;
    void CopySelectionToClipboard() const;
    void PasteFromClipboard();

    std::string m_text;
    std::string m_placeholder;
    TextChangedHandler m_onTextChanged;
    bool m_focused = false;
    bool m_readOnly = false;
    bool m_selectAllOnFocus = true;
    size_t m_maxLength = 0; // 0 means unlimited

    size_t m_caretIndex = 0;
    size_t m_anchorIndex = 0;
    std::vector<size_t> m_codepointOffsets;
    std::vector<float> m_cachedCaretPositions;
    float m_cachedTextHeight = 0.0f;
};

} // namespace Render::UI


