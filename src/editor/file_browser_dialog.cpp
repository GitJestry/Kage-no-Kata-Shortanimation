#include "editor/file_browser_dialog.hpp"

#include "assets/asset_path.hpp"

#include <imgui.h>

#include <algorithm>
#include <system_error>
#include <utility>

namespace {

[[nodiscard]] std::filesystem::path canonicalOrAbsolute(
    const std::filesystem::path& parPath) {
  std::error_code error_code;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(parPath, error_code);
  if (!error_code) {
    return canonical_path;
  }
  return std::filesystem::absolute(parPath);
}

[[nodiscard]] std::filesystem::path fallbackDirectory() {
  return canonicalOrAbsolute(std::filesystem::current_path());
}

[[nodiscard]] std::filesystem::path resolveStartDirectory(
    const std::filesystem::path& parStartDirectory) {
  std::error_code error_code;
  if (std::filesystem::is_directory(parStartDirectory, error_code)) {
    return canonicalOrAbsolute(parStartDirectory);
  }
  return fallbackDirectory();
}

[[nodiscard]] std::filesystem::path findAssetDirectory(
    std::filesystem::path parDirectory) {
  parDirectory = canonicalOrAbsolute(std::move(parDirectory));
  while (!parDirectory.empty()) {
    if (parDirectory.filename() == "assets") {
      return parDirectory;
    }
    const std::filesystem::path parent = parDirectory.parent_path();
    if (parent == parDirectory) {
      break;
    }
    parDirectory = parent;
  }
  return fallbackDirectory();
}

}  // namespace

namespace kage::editor {

void FileBrowserDialog::open(std::string parTitle,
                             std::filesystem::path parStartDirectory) {
  m_title = std::move(parTitle);
  m_current_directory = resolveStartDirectory(parStartDirectory);
  m_asset_directory = findAssetDirectory(m_current_directory);
  m_selected_file.reset();
  m_open = true;
  m_open_requested = true;
  refreshEntries();
}

std::optional<std::filesystem::path> FileBrowserDialog::draw() {
  if (!m_open) {
    return std::nullopt;
  }
  if (m_open_requested) {
    ImGui::OpenPopup(m_title.c_str());
    m_open_requested = false;
  }

  std::optional<std::filesystem::path> result;
  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(620.0f, 430.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(m_title.c_str(), nullptr,
                             ImGuiWindowFlags_NoCollapse)) {
    ImGui::TextWrapped("%s", m_current_directory.string().c_str());
    if (ImGui::Button("Up")) {
      if (m_current_directory.has_parent_path()) {
        m_current_directory = m_current_directory.parent_path();
        m_selected_file.reset();
        refreshEntries();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Project Assets")) {
      m_current_directory = m_asset_directory;
      m_selected_file.reset();
      refreshEntries();
    }

    ImGui::BeginChild("FileBrowserEntries", ImVec2(0.0f, -42.0f), true);
    for (const Entry& entry : m_entries) {
      const bool selected =
          m_selected_file.has_value() && *m_selected_file == entry.path;
      const std::string label =
          entry.directory ? "[dir] " + entry.label : entry.label;
      if (ImGui::Selectable(label.c_str(), selected)) {
        if (entry.directory) {
          m_current_directory = entry.path;
          m_selected_file.reset();
          refreshEntries();
        } else {
          m_selected_file = entry.path;
        }
      }
    }
    ImGui::EndChild();

    if (!m_selected_file.has_value()) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Select", ImVec2(110.0f, 0.0f))) {
      result = m_selected_file;
      m_open = false;
      ImGui::CloseCurrentPopup();
    }
    if (!m_selected_file.has_value()) {
      ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
      m_open = false;
      m_selected_file.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  return result;
}

bool FileBrowserDialog::isOpen() const {
  return m_open;
}

void FileBrowserDialog::refreshEntries() {
  m_entries.clear();
  std::error_code error_code;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(m_current_directory, error_code)) {
    if (error_code) {
      return;
    }
    const bool directory = entry.is_directory(error_code);
    if (error_code) {
      error_code.clear();
      continue;
    }
    if (!directory && !assets::hasGltfExtension(entry.path())) {
      continue;
    }
    m_entries.push_back({
        entry.path(),
        entry.path().filename().string(),
        directory,
    });
  }
  std::sort(m_entries.begin(), m_entries.end(),
            [](const Entry& parLeft, const Entry& parRight) {
              if (parLeft.directory != parRight.directory) {
                return parLeft.directory > parRight.directory;
              }
              return parLeft.label < parRight.label;
            });
}

}  // namespace kage::editor
