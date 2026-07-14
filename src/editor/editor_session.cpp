#include "editor/editor_session.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <fstream>

namespace kage::editor {

void loadEditorSession(const std::filesystem::path&, EditorSession&) {}

void saveEditorSession(const std::filesystem::path& parPath,
                       const EditorSession&) {
  nlohmann::json document = nlohmann::json::object();
  try {
    std::ifstream input(parPath);
    if (input) {
      input >> document;
    }
  } catch (const nlohmann::json::exception&) {
    document = nlohmann::json::object();
  }
  document["version"] = 4;
  document.erase("film_editor_height");
  document.erase("target_sequence_pixels_per_frame");
  const std::filesystem::path temporary = parPath.string() + ".ui.tmp";
  {
    std::ofstream output(temporary, std::ios::trunc);
    output << document.dump(2);
  }
  std::error_code error;
  std::filesystem::rename(temporary, parPath, error);
  if (error) {
    std::filesystem::remove(parPath, error);
    error.clear();
    std::filesystem::rename(temporary, parPath, error);
  }
}

}  // namespace kage::editor
