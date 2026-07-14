#pragma once

#include <imgui.h>

namespace kage::editor {

// BeginDisabled always changes ImGui's item-flag stack, including when the
// requested state is false. Keeping the matching EndDisabled in this scope
// makes early exits from Movie Editor actions safe.
class MovieDisabledScope final {
 public:
  explicit MovieDisabledScope(bool parDisabled) {
    ImGui::BeginDisabled(parDisabled);
  }

  ~MovieDisabledScope() { ImGui::EndDisabled(); }

  MovieDisabledScope(const MovieDisabledScope&) = delete;
  MovieDisabledScope& operator=(const MovieDisabledScope&) = delete;
};

}  // namespace kage::editor
