#include "editor/movie_imgui_scope.hpp"

#include <imgui.h>

#include <iostream>

namespace {

[[nodiscard]] bool completeNewSequenceAction(bool parCreated) {
  kage::editor::MovieDisabledScope disabled(false);
  if (parCreated) {
    return true;
  }
  return false;
}

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  ImGuiContext* context = ImGui::CreateContext();
  if (context == nullptr) {
    return fail("could not create an ImGui context");
  }

  ImGui::GetIO().DisplaySize = ImVec2(1280.0f, 720.0f);
  ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width,
                                           &font_height);
  ImGui::NewFrame();
  ImGui::Begin("Movie Inspector");
  if (!completeNewSequenceAction(true)) {
    ImGui::End();
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
    return fail("New Sequence success path did not complete");
  }
  ImGui::End();
  ImGui::EndFrame();
  ImGui::DestroyContext(context);
  return 0;
}
