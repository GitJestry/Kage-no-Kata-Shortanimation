#pragma once

#include <algorithm>
#include <cstring>
#include <span>
#include <string_view>

namespace kage::editor {

inline void copyTextToBuffer(std::string_view parText,
                             std::span<char> parBuffer) {
  if (parBuffer.empty()) {
    return;
  }
  const std::size_t size = std::min(parText.size(), parBuffer.size() - 1);
  std::memcpy(parBuffer.data(), parText.data(), size);
  parBuffer[size] = '\0';
}

}  // namespace kage::editor
