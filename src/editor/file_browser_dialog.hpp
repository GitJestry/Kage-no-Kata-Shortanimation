#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kage::editor {

class FileBrowserDialog final {
 public:
  void open(std::string parTitle, std::filesystem::path parStartDirectory);
  [[nodiscard]] std::optional<std::filesystem::path> draw();
  [[nodiscard]] bool isOpen() const;

 private:
  void refreshEntries();

  struct Entry final {
    std::filesystem::path path;
    std::string label;
    bool directory = false;
  };

  std::string m_title;
  std::filesystem::path m_current_directory;
  std::optional<std::filesystem::path> m_selected_file;
  std::vector<Entry> m_entries;
  bool m_open = false;
  bool m_open_requested = false;
};

}  // namespace kage::editor
