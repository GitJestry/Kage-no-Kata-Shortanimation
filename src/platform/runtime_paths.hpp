#pragma once

#include <filesystem>
#include <system_error>

namespace kage::platform {

[[nodiscard]] inline std::filesystem::path canonicalOrAbsolute(
    const std::filesystem::path& parPath) {
  std::error_code error_code;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(parPath, error_code);
  return error_code ? std::filesystem::absolute(parPath) : canonical_path;
}

class RuntimePaths final {
 public:
  explicit RuntimePaths(std::filesystem::path parExecutablePath);

  [[nodiscard]] static RuntimePaths fromExecutable();

  [[nodiscard]] const std::filesystem::path& getProjectRoot() const;
  [[nodiscard]] const std::filesystem::path& getAssetDirectory() const;
  [[nodiscard]] const std::filesystem::path& getModelDirectory() const;
  [[nodiscard]] std::filesystem::path getProjectPath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getAssetPath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getTexturePath(
      const std::filesystem::path& parRelativePath) const;
  [[nodiscard]] std::filesystem::path getProjectAssetCatalogPath() const;
  [[nodiscard]] std::filesystem::path getProjectWorldPath() const;
  [[nodiscard]] std::filesystem::path getLocalSessionPath() const;

 private:
  std::filesystem::path m_project_root;
  std::filesystem::path m_asset_directory;
  std::filesystem::path m_model_directory;
};

}  // namespace kage::platform
