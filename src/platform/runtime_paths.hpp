#pragma once

#include <filesystem>

namespace kage::platform {

class RuntimePaths final {
 public:
  explicit RuntimePaths(std::filesystem::path parExecutablePath);

  [[nodiscard]] static RuntimePaths fromExecutable();

  [[nodiscard]] const std::filesystem::path& getExecutableDirectory() const;
  [[nodiscard]] const std::filesystem::path& getAssetDirectory() const;
  [[nodiscard]] std::filesystem::path getAssetPath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getModelPath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getTexturePath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getAudioPath(
      const std::filesystem::path& parRelativePath) const;

 private:
  std::filesystem::path m_executable_directory;
  std::filesystem::path m_asset_directory;
};

}  // namespace kage::platform
