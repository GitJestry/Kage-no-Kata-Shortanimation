#include "editor/editor_session.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "kage_editor_session_check.json";
  {
    std::ofstream output(path, std::ios::trunc);
    output << nlohmann::json{{"retained", true},
                             {"film_editor_height", 900.0f},
                             {"target_sequence_pixels_per_frame", 30.0f}}
                  .dump();
  }

  kage::editor::EditorSession session;
  kage::editor::loadEditorSession(path, session);
  if (std::abs(session.film_editor_height - 260.0f) > 0.001f ||
      std::abs(session.target_sequence_pixels_per_frame - 12.0f) > 0.001f) {
    return fail("editor layout values were loaded from disk");
  }

  session.film_editor_height = 900.0f;
  session.target_sequence_pixels_per_frame = 30.0f;
  kage::editor::saveEditorSession(path, session);

  nlohmann::json saved;
  {
    std::ifstream input(path);
    input >> saved;
  }
  std::error_code error;
  std::filesystem::remove(path, error);
  if (!saved.value("retained", false) || saved.value("version", 0) != 4 ||
      saved.contains("film_editor_height") ||
      saved.contains("target_sequence_pixels_per_frame")) {
    return fail("editor layout values were serialized to disk");
  }
  return 0;
}
