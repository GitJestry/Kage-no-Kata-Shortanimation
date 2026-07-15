#include "editor/editor_session.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <fstream>

namespace kage::editor {

void loadEditorSession(const std::filesystem::path& parPath,
                       EditorSession& parSession) {
  std::ifstream input(parPath);
  if (!input) {
    return;
  }
  try {
    nlohmann::json document;
    input >> document;
    parSession.film_editor_height = std::clamp(
        document.value("film_editor_height", parSession.film_editor_height),
        220.0f, 1200.0f);
  } catch (const nlohmann::json::exception&) {
  }
}

void saveEditorSession(const std::filesystem::path& parPath,
                       const EditorSession& parSession) {
  nlohmann::json document;
  try {
    std::ifstream input(parPath);
    if (input) {
      input >> document;
    }
  } catch (const nlohmann::json::exception&) {
    document = nlohmann::json::object();
  }
  document["version"] = 4;
  document["film_editor_height"] = parSession.film_editor_height;
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
