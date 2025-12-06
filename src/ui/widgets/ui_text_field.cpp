/*
 * Copyright (c) 2025 Li Chaoyu
 * 
 * This file is part of Render.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * For commercial licensing, please contact: 2052046346@qq.com
 */
#include "render/ui/widgets/ui_text_field.h"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <algorithm>
#include <cctype>
#include <limits>

#include "render/logger.h"

namespace Render::UI {

namespace {

size_t Utf8Next(const std::string& text, size_t index) {
    if (index >= text.size()) {
        return text.size();
    }
    unsigned char lead = static_cast<unsigned char>(text[index]);
    size_t advance = 1;
    if ((lead & 0x80u) == 0x00u) {
        advance = 1;
    } else if ((lead & 0xE0u) == 0xC0u) {
        advance = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        advance = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        advance = 4;
    } else {
        advance = 1;
    }
    size_t next = index + advance;
    if (advance > 1) {
        for (size_t i = 1; i < advance; ++i) {
            size_t pos = index + i;
            if (pos >= text.size()) {
                next = text.size();
                break;
            }
            unsigned char ch = static_cast<unsigned char>(text[pos]);
            if ((ch & 0xC0u) != 0x80u) {
                next = index + 1;
                break;
            }
        }
    }
    return std::min(next, text.size());
}

size_t Utf8Prev(const std::string& text, size_t index) {
    if (index == 0 || text.empty()) {
        return 0;
    }
    size_t pos = std::min(index, text.size());
    do {
        --pos;
        unsigned char ch = static_cast<unsigned char>(text[pos]);
        if ((ch & 0xC0u) != 0x80u || pos == 0) {
            return pos;
        }
    } while (pos > 0);
    return 0;
}

uint32_t DecodeCodepoint(const std::string& text, size_t index, size_t* nextOut = nullptr) {
    if (index >= text.size()) {
        if (nextOut) {
            *nextOut = text.size();
        }
        return 0;
    }

    unsigned char lead = static_cast<unsigned char>(text[index]);
    size_t advance = 1;
    uint32_t codepoint = 0;

    if ((lead & 0x80u) == 0x00u) {
        codepoint = lead;
    } else if ((lead & 0xE0u) == 0xC0u && index + 1 < text.size()) {
        unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
        if ((b1 & 0xC0u) == 0x80u) {
            codepoint = static_cast<uint32_t>(lead & 0x1Fu) << 6;
            codepoint |= static_cast<uint32_t>(b1 & 0x3Fu);
            advance = 2;
        }
    } else if ((lead & 0xF0u) == 0xE0u && index + 2 < text.size()) {
        unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
        unsigned char b2 = static_cast<unsigned char>(text[index + 2]);
        if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u)) {
            codepoint = static_cast<uint32_t>(lead & 0x0Fu) << 12;
            codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 6;
            codepoint |= static_cast<uint32_t>(b2 & 0x3Fu);
            advance = 3;
        }
    } else if ((lead & 0xF8u) == 0xF0u && index + 3 < text.size()) {
        unsigned char b1 = static_cast<unsigned char>(text[index + 1]);
        unsigned char b2 = static_cast<unsigned char>(text[index + 2]);
        unsigned char b3 = static_cast<unsigned char>(text[index + 3]);
        if (((b1 & 0xC0u) == 0x80u) && ((b2 & 0xC0u) == 0x80u) && ((b3 & 0xC0u) == 0x80u)) {
            codepoint = static_cast<uint32_t>(lead & 0x07u) << 18;
            codepoint |= static_cast<uint32_t>(b1 & 0x3Fu) << 12;
            codepoint |= static_cast<uint32_t>(b2 & 0x3Fu) << 6;
            codepoint |= static_cast<uint32_t>(b3 & 0x3Fu);
            advance = 4;
        }
    }

    if (codepoint == 0) {
        codepoint = lead;
        advance = 1;
    }

    if (nextOut) {
        *nextOut = std::min(index + advance, text.size());
    }
    return codepoint;
}

bool IsWhitespace(uint32_t codepoint) {
    if (codepoint <= 0x7Fu) {
        return std::isspace(static_cast<unsigned char>(codepoint)) != 0;
    }
    return codepoint == 0x3000u; // full-width space
}

std::string TruncateUtf8(const std::string& text, size_t maxBytes) {
    if (maxBytes == 0) {
        return {};
    }
    if (text.size() <= maxBytes) {
        return text;
    }

    std::string result;
    result.reserve(std::min(text.size(), maxBytes));
    size_t cursor = 0;
    while (cursor < text.size()) {
        size_t next = Utf8Next(text, cursor);
        size_t charBytes = next - cursor;
        if (result.size() + charBytes > maxBytes) {
            break;
        }
        result.append(text, cursor, charBytes);
        cursor = next;
    }
    return result;
}

} // namespace

UITextField::UITextField(std::string id)
    : UIWidget(std::move(id)) {
    SetPadding(Vector4(kPaddingLeft, kPaddingTop, kPaddingRight, kPaddingBottom));
    SetMinSize(Vector2(140.0f, 48.0f));
    SetPreferredSize(Vector2(220.0f, 64.0f));
    RebuildCodepointOffsets();
    UpdateCaretMetrics({}, 0.0f);
}

void UITextField::SetText(std::string text) {
    if (m_text == text) {
        return;
    }
    m_text = std::move(text);
    RebuildCodepointOffsets();
    SetCaretIndex(m_codepointOffsets.size() - 1, false);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    NotifyTextChanged();
}

void UITextField::SetPlaceholder(std::string placeholder) {
    if (m_placeholder == placeholder) {
        return;
    }
    m_placeholder = std::move(placeholder);
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::SetReadOnly(bool readOnly) noexcept {
    if (m_readOnly == readOnly) {
        return;
    }
    m_readOnly = readOnly;
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

size_t UITextField::GetCaretByteOffset() const noexcept {
    return IndexToOffset(m_caretIndex);
}

std::pair<size_t, size_t> UITextField::GetSelectionIndices() const noexcept {
    size_t first = std::min(m_anchorIndex, m_caretIndex);
    size_t second = std::max(m_anchorIndex, m_caretIndex);
    return {first, second};
}

std::pair<size_t, size_t> UITextField::GetSelectionByteRange() const noexcept {
    auto [startIndex, endIndex] = GetSelectionIndices();
    return {IndexToOffset(startIndex), IndexToOffset(endIndex)};
}

void UITextField::ClearSelection() {
    size_t clamped = ClampCaretIndex(m_caretIndex);
    if (m_anchorIndex == clamped) {
        return;
    }
    m_anchorIndex = clamped;
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::SelectAll() {
    if (m_codepointOffsets.size() <= 1) {
        ClearSelection();
        return;
    }
    m_anchorIndex = 0;
    m_caretIndex = m_codepointOffsets.size() - 1;
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::UpdateCaretMetrics(const std::vector<float>& caretXPositions, float textHeight) {
    if (caretXPositions.empty()) {
        m_cachedCaretPositions = {0.0f};
    } else {
        m_cachedCaretPositions = caretXPositions;
    }
    if (m_cachedCaretPositions.size() < m_codepointOffsets.size()) {
        m_cachedCaretPositions.resize(m_codepointOffsets.size(), m_cachedCaretPositions.back());
    }
    if (m_cachedCaretPositions.size() > m_codepointOffsets.size()) {
        m_cachedCaretPositions.resize(m_codepointOffsets.size());
    }
    m_cachedTextHeight = textHeight;

    const float desiredHeight = std::max(textHeight + kPaddingTop + kPaddingBottom, 1.0f);
    if (desiredHeight > 0.0f) {
        Vector2 minSize = GetMinSize();
        if (minSize.y() + 0.5f < desiredHeight) {
            minSize.y() = desiredHeight;
            SetMinSize(minSize);
        }

        Vector2 preferred = GetPreferredSize();
        if (preferred.y() + 0.5f < desiredHeight) {
            preferred.y() = desiredHeight;
            SetPreferredSize(preferred);
        }
    }
}

void UITextField::OnFocusGained() {
    if (m_focused) {
        return;
    }
    m_focused = true;
    Logger::GetInstance().InfoFormat("[UITextField] FocusGained id=%s", GetId().c_str());
    if (m_selectAllOnFocus && !m_text.empty()) {
        SelectAll();
    } else {
        SetCaretIndex(m_caretIndex, false);
    }
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::OnFocusLost() {
    if (!m_focused) {
        return;
    }
    m_focused = false;
    Logger::GetInstance().InfoFormat("[UITextField] FocusLost id=%s", GetId().c_str());
    ClearSelection();
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::OnMouseClick(uint8_t button, const Vector2& position) {
    if (button != SDL_BUTTON_LEFT || !IsEnabled()) {
        return;
    }
    Logger::GetInstance().InfoFormat("[UITextField] OnMouseClick id=%s pos=(%.1f, %.1f) focused=%s",
                                     GetId().c_str(),
                                     position.x(),
                                     position.y(),
                                     m_focused ? "true" : "false");
    const Rect& rect = GetLayoutRect();
    float localX = position.x() - rect.x;
    float relativeX = localX - kPaddingLeft;
    bool extendSelection = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    size_t hitIndex = HitTestCaretIndex(relativeX);
    SetCaretIndex(hitIndex, extendSelection);
}

void UITextField::OnTextInput(const std::string& text) {
    if (!m_focused || !IsEnabled() || m_readOnly) {
        return;
    }
    if (text.empty()) {
        return;
    }
    Logger::GetInstance().InfoFormat("[UITextField] OnTextInput id=%s text=\"%s\" current=\"%s\"",
                                     GetId().c_str(),
                                     text.c_str(),
                                     m_text.c_str());

    std::string filtered;
    filtered.reserve(text.size());
    for (char ch : text) {
        if (ch == '\n' || ch == '\r') {
            continue;
        }
        filtered.push_back(ch);
    }

    if (filtered.empty()) {
        return;
    }

    InsertText(filtered);
}

void UITextField::OnKey(int scancode, bool pressed, bool repeat) {
    if (!m_focused || !IsEnabled()) {
        return;
    }
    if (!pressed) {
        return;
    }
    (void)repeat;

    SDL_Keymod mods = SDL_GetModState();
    bool ctrl = (mods & SDL_KMOD_CTRL) != 0;
    bool shift = (mods & SDL_KMOD_SHIFT) != 0;
    bool alt = (mods & SDL_KMOD_ALT) != 0;
    (void)alt;

    switch (scancode) {
    case SDL_SCANCODE_LEFT:
        MoveCaretLeft(shift, ctrl);
        break;
    case SDL_SCANCODE_RIGHT:
        MoveCaretRight(shift, ctrl);
        break;
    case SDL_SCANCODE_HOME:
        MoveCaretToStart(shift);
        break;
    case SDL_SCANCODE_END:
        MoveCaretToEnd(shift);
        break;
    case SDL_SCANCODE_BACKSPACE:
        if (ctrl) {
            if (!HasSelection()) {
                size_t startIndex = FindPreviousWordIndex(m_caretIndex);
                if (startIndex != m_caretIndex) {
                    size_t startByte = IndexToOffset(startIndex);
                    size_t endByte = IndexToOffset(m_caretIndex);
                    m_text.erase(startByte, endByte - startByte);
                    RebuildCodepointOffsets();
                    SetCaretIndex(startIndex, false);
                    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
                    NotifyTextChanged();
                }
            } else {
                DeleteSelection();
            }
        } else {
            DeletePrevious();
        }
        break;
    case SDL_SCANCODE_DELETE:
        if (ctrl) {
            if (!HasSelection()) {
                size_t endIndex = FindNextWordIndex(m_caretIndex);
                if (endIndex != m_caretIndex) {
                    size_t startByte = IndexToOffset(m_caretIndex);
                    size_t endByte = IndexToOffset(endIndex);
                    m_text.erase(startByte, endByte - startByte);
                    RebuildCodepointOffsets();
                    SetCaretIndex(m_caretIndex, false);
                    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
                    NotifyTextChanged();
                }
            } else {
                DeleteSelection();
            }
        } else {
            DeleteNext();
        }
        break;
    case SDL_SCANCODE_A:
        if (ctrl) {
            SelectAll();
        }
        break;
    case SDL_SCANCODE_C:
        if (ctrl) {
            CopySelectionToClipboard();
        }
        break;
    case SDL_SCANCODE_X:
        if (ctrl && !m_readOnly) {
            CopySelectionToClipboard();
            DeleteSelection();
        }
        break;
    case SDL_SCANCODE_V:
        if (ctrl && !m_readOnly) {
            PasteFromClipboard();
        }
        break;
    default:
        break;
    }
}

void UITextField::NotifyTextChanged() {
    if (m_onTextChanged) {
        m_onTextChanged(*this, m_text);
    }
}

void UITextField::MoveCaretLeft(bool extendSelection, bool jumpWord) {
    if (m_codepointOffsets.size() <= 1) {
        return;
    }
    size_t targetIndex = m_caretIndex;
    if (jumpWord) {
        targetIndex = FindPreviousWordIndex(targetIndex);
    } else if (targetIndex > 0) {
        --targetIndex;
    }
    SetCaretIndex(targetIndex, extendSelection);
}

void UITextField::MoveCaretRight(bool extendSelection, bool jumpWord) {
    if (m_codepointOffsets.size() <= 1) {
        return;
    }
    size_t targetIndex = m_caretIndex;
    size_t maxIndex = m_codepointOffsets.size() - 1;
    if (jumpWord) {
        targetIndex = FindNextWordIndex(targetIndex);
    } else if (targetIndex < maxIndex) {
        ++targetIndex;
    }
    SetCaretIndex(targetIndex, extendSelection);
}

void UITextField::MoveCaretToStart(bool extendSelection) {
    SetCaretIndex(0, extendSelection);
}

void UITextField::MoveCaretToEnd(bool extendSelection) {
    if (!m_codepointOffsets.empty()) {
        SetCaretIndex(m_codepointOffsets.size() - 1, extendSelection);
    }
}

void UITextField::DeletePrevious() {
    if (!IsEnabled() || m_readOnly) {
        return;
    }
    if (DeleteSelection()) {
        return;
    }
    if (m_caretIndex == 0) {
        return;
    }
    size_t startIndex = m_caretIndex - 1;
    size_t startByte = IndexToOffset(startIndex);
    size_t endByte = IndexToOffset(m_caretIndex);
    m_text.erase(startByte, endByte - startByte);
    RebuildCodepointOffsets();
    SetCaretIndex(startIndex, false);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    NotifyTextChanged();
}

void UITextField::DeleteNext() {
    if (!IsEnabled() || m_readOnly) {
        return;
    }
    if (DeleteSelection()) {
        return;
    }
    size_t maxIndex = m_codepointOffsets.empty() ? 0 : m_codepointOffsets.size() - 1;
    if (m_caretIndex >= maxIndex) {
        return;
    }
    size_t startByte = IndexToOffset(m_caretIndex);
    size_t endByte = IndexToOffset(m_caretIndex + 1);
    m_text.erase(startByte, endByte - startByte);
    RebuildCodepointOffsets();
    SetCaretIndex(std::min(m_caretIndex, m_codepointOffsets.size() - 1), false);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    NotifyTextChanged();
}

bool UITextField::DeleteSelection() {
    if (!HasSelection()) {
        return false;
    }
    if (m_readOnly) {
        return false;
    }
    auto [startIndex, endIndex] = GetSelectionIndices();
    if (startIndex == endIndex) {
        return false;
    }
    size_t startByte = IndexToOffset(startIndex);
    size_t endByte = IndexToOffset(endIndex);
    m_text.erase(startByte, endByte - startByte);
    RebuildCodepointOffsets();
    SetCaretIndex(startIndex, false);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    NotifyTextChanged();
    return true;
}

void UITextField::InsertText(const std::string& text) {
    if (text.empty()) {
        return;
    }
    size_t availableBytes = text.size();
    if (m_maxLength > 0) {
        size_t currentBytes = m_text.size();
        size_t selectionBytes = 0;
        if (HasSelection()) {
            auto [startByte, endByte] = GetSelectionByteRange();
            selectionBytes = endByte - startByte;
        }
        if (currentBytes - selectionBytes >= m_maxLength) {
            return;
        }
        size_t allowed = m_maxLength - (currentBytes - selectionBytes);
        availableBytes = std::min(availableBytes, allowed);
    }

    std::string insertText = text;
    if (insertText.size() > availableBytes) {
        insertText = TruncateUtf8(insertText, availableBytes);
    }
    if (insertText.empty()) {
        return;
    }

    size_t insertOffset = GetCaretByteOffset();
    if (HasSelection()) {
        auto [startByte, endByte] = GetSelectionByteRange();
        m_text.erase(startByte, endByte - startByte);
        insertOffset = startByte;
    }

    m_text.insert(insertOffset, insertText);
    Logger::GetInstance().InfoFormat("[UITextField] InsertText id=%s inserted=\"%s\" result=\"%s\"",
                                     GetId().c_str(),
                                     insertText.c_str(),
                                     m_text.c_str());
    RebuildCodepointOffsets();
    SetCaretByByteOffset(insertOffset + insertText.size(), false);
    MarkDirty(UIWidgetDirtyFlag::Layout | UIWidgetDirtyFlag::Visual);
    NotifyTextChanged();
}

void UITextField::SetCaretIndex(size_t index, bool keepAnchor) {
    if (m_codepointOffsets.empty()) {
        m_caretIndex = 0;
        m_anchorIndex = 0;
        MarkDirty(UIWidgetDirtyFlag::Visual);
        return;
    }
    size_t clamped = ClampCaretIndex(index);
    if (m_caretIndex == clamped && (keepAnchor || m_anchorIndex == clamped)) {
        return;
    }
    m_caretIndex = clamped;
    if (!keepAnchor) {
        m_anchorIndex = clamped;
    } else {
        m_anchorIndex = ClampCaretIndex(m_anchorIndex);
    }
    MarkDirty(UIWidgetDirtyFlag::Visual);
}

void UITextField::SetCaretByByteOffset(size_t byteOffset, bool keepAnchor) {
    size_t index = OffsetToIndex(byteOffset);
    SetCaretIndex(index, keepAnchor);
}

size_t UITextField::ClampCaretIndex(size_t index) const noexcept {
    if (m_codepointOffsets.empty()) {
        return 0;
    }
    size_t maxIndex = m_codepointOffsets.size() - 1;
    return std::min(index, maxIndex);
}

void UITextField::RebuildCodepointOffsets() {
    m_codepointOffsets.clear();
    m_codepointOffsets.reserve(m_text.size() + 1);
    m_codepointOffsets.push_back(0);
    size_t cursor = 0;
    while (cursor < m_text.size()) {
        size_t next = Utf8Next(m_text, cursor);
        if (next <= cursor) {
            next = cursor + 1;
        }
        m_codepointOffsets.push_back(next);
        cursor = next;
    }
    if (m_codepointOffsets.back() != m_text.size()) {
        m_codepointOffsets.push_back(m_text.size());
    }
    EnsureCaretWithinBounds();
}

void UITextField::EnsureCaretWithinBounds() {
    if (m_codepointOffsets.empty()) {
        m_codepointOffsets = {0};
    }
    size_t maxIndex = m_codepointOffsets.size() - 1;
    if (m_caretIndex > maxIndex) {
        m_caretIndex = maxIndex;
    }
    if (m_anchorIndex > maxIndex) {
        m_anchorIndex = maxIndex;
    }
}

size_t UITextField::OffsetToIndex(size_t byteOffset) const noexcept {
    if (m_codepointOffsets.empty()) {
        return 0;
    }
    auto it = std::lower_bound(m_codepointOffsets.begin(), m_codepointOffsets.end(), byteOffset);
    if (it == m_codepointOffsets.end()) {
        return m_codepointOffsets.size() - 1;
    }
    size_t index = static_cast<size_t>(std::distance(m_codepointOffsets.begin(), it));
    if (*it != byteOffset && it != m_codepointOffsets.begin() && byteOffset < *it) {
        index = static_cast<size_t>(std::distance(m_codepointOffsets.begin(), it - 1));
    }
    return ClampCaretIndex(index);
}

size_t UITextField::IndexToOffset(size_t index) const noexcept {
    if (m_codepointOffsets.empty()) {
        return 0;
    }
    size_t clamped = ClampCaretIndex(index);
    return m_codepointOffsets[clamped];
}

uint32_t UITextField::CodepointAt(size_t index) const noexcept {
    if (m_codepointOffsets.empty()) {
        return 0;
    }
    size_t clamped = ClampCaretIndex(index);
    if (clamped >= m_codepointOffsets.size() - 1) {
        if (clamped == 0) {
            return 0;
        }
        clamped -= 1;
    }
    size_t byteOffset = m_codepointOffsets[clamped];
    return DecodeCodepoint(m_text, byteOffset);
}

size_t UITextField::FindPreviousWordIndex(size_t startIndex) const noexcept {
    if (startIndex == 0 || m_codepointOffsets.size() <= 1) {
        return 0;
    }
    size_t index = ClampCaretIndex(startIndex);
    if (index == 0) {
        return 0;
    }
    // Move to previous codepoint
    --index;
    while (index > 0 && IsWhitespace(CodepointAt(index))) {
        --index;
    }
    while (index > 0 && !IsWhitespace(CodepointAt(index - 1))) {
        --index;
    }
    return index;
}

size_t UITextField::FindNextWordIndex(size_t startIndex) const noexcept {
    if (m_codepointOffsets.size() <= 1) {
        return 0;
    }
    size_t maxIndex = m_codepointOffsets.size() - 1;
    size_t index = ClampCaretIndex(startIndex);
    while (index < maxIndex && !IsWhitespace(CodepointAt(index))) {
        ++index;
    }
    while (index < maxIndex && IsWhitespace(CodepointAt(index))) {
        ++index;
    }
    return index;
}

size_t UITextField::HitTestCaretIndex(float localX) const noexcept {
    if (m_cachedCaretPositions.empty()) {
        return ClampCaretIndex(0);
    }
    float x = std::max(localX, 0.0f);
    size_t index = 0;
    auto it = std::lower_bound(m_cachedCaretPositions.begin(), m_cachedCaretPositions.end(), x);
    if (it == m_cachedCaretPositions.end()) {
        index = m_cachedCaretPositions.size() - 1;
    } else {
        index = static_cast<size_t>(std::distance(m_cachedCaretPositions.begin(), it));
    }
    return ClampCaretIndex(index);
}

std::string_view UITextField::GetSelectionTextView() const noexcept {
    if (!HasSelection()) {
        return {};
    }
    auto [startByte, endByte] = GetSelectionByteRange();
    if (startByte >= endByte || startByte >= m_text.size()) {
        return {};
    }
    size_t length = std::min(endByte, m_text.size()) - startByte;
    return std::string_view(m_text).substr(startByte, length);
}

void UITextField::CopySelectionToClipboard() const {
    if (!HasSelection()) {
        return;
    }
    std::string_view sel = GetSelectionTextView();
    if (sel.empty()) {
        return;
    }
    std::string text(sel);
    SDL_SetClipboardText(text.c_str());
}

void UITextField::PasteFromClipboard() {
    if (m_readOnly) {
        return;
    }
    char* clipboard = SDL_GetClipboardText();
    if (!clipboard) {
        return;
    }
    std::string pasted = clipboard;
    SDL_free(clipboard);
    if (pasted.empty()) {
        return;
    }
    InsertText(pasted);
}

} // namespace Render::UI

