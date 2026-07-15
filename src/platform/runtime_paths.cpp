#include "platform/runtime_paths.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace {

using kage::platform::canonicalOrAbsolute;

constexpr const char* PROJECT_ASSET_CATALOG_PATH =
    "projects/kage_no_kata_assets.kage.json";
constexpr const char* PROJECT_WORLD_PATH =
    "projects/kage_no_kata_world.kage.json";
constexpr const char* LOCAL_SESSION_PATH = ".kage_local/editor_session.json";

std::filesystem::path getExecutablePath() {
#if defined(_WIN32)
  std::vector<wchar_t> buffer(MAX_PATH);
  DWORD copied_size = 0;

  while (true) {
    copied_size = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (copied_size == 0) {
      throw std::runtime_error("Failed to resolve executable path");
    }

    if (copied_size < buffer.size()) {
      return std::filesystem::path(
          std::wstring(buffer.data(), static_cast<std::size_t>(copied_size)));
    }

    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  uint32_t buffer_size = 0;
  _NSGetExecutablePath(nullptr, &buffer_size);

  std::vector<char> buffer(buffer_size);
  if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
    throw std::runtime_error("Failed to resolve executable path");
  }

  std::error_code error_code;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(buffer.data(), error_code);
  if (!error_code) {
    return canonical_path;
  }

  return std::filesystem::absolute(buffer.data());
#else
  std::vector<char> buffer(1024);
  while (true) {
    const ssize_t copied_size =
        readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (copied_size < 0) {
      return std::filesystem::current_path();
    }
    if (static_cast<std::size_t>(copied_size) < buffer.size()) {
      return std::filesystem::path(
          std::string(buffer.data(), static_cast<std::size_t>(copied_size)));
    }
    buffer.resize(buffer.size() * 2);
  }
#endif
}

[[nodiscard]] bool isProjectRoot(const std::filesystem::path& parPath) {
  return std::filesystem::exists(parPath / "assets") &&
         std::filesystem::exists(parPath / "projects");
}

[[nodiscard]] bool hasAssetDirectory(const std::filesystem::path& parPath) {
  return std::filesystem::exists(parPath / "assets");
}

[[nodiscard]] std::filesystem::path searchRootUpwards(
    std::filesystem::path parStart) {
  parStart = canonicalOrAbsolute(std::move(parStart));
  std::filesystem::path asset_only_candidate;

  while (!parStart.empty()) {
    if (isProjectRoot(parStart)) {
      return canonicalOrAbsolute(parStart);
    }
    if (asset_only_candidate.empty() && hasAssetDirectory(parStart)) {
      asset_only_candidate = canonicalOrAbsolute(parStart);
    }

    const std::filesystem::path parent = parStart.parent_path();
    if (parent == parStart) {
      break;
    }
    parStart = parent;
  }

  return asset_only_candidate;
}

[[nodiscard]] std::filesystem::path resolveProjectRoot(
    const std::filesystem::path& parExecutableDirectory) {
  std::filesystem::path root = searchRootUpwards(std::filesystem::current_path());
  if (!root.empty() && isProjectRoot(root)) {
    return root;
  }

  std::filesystem::path executable_root = searchRootUpwards(parExecutableDirectory);
  if (!executable_root.empty() && isProjectRoot(executable_root)) {
    return executable_root;
  }

  if (!root.empty()) {
    return root;
  }
  if (!executable_root.empty()) {
    return executable_root;
  }
  return canonicalOrAbsolute(std::filesystem::current_path());
}

}  // namespace

namespace kage::platform {

RuntimePaths::RuntimePaths(std::filesystem::path parExecutablePath)
    : m_project_root(resolveProjectRoot(
          canonicalOrAbsolute(std::move(parExecutablePath)).parent_path())),
      m_asset_directory(m_project_root / "assets"),
      m_model_directory(m_asset_directory / "models") {}

RuntimePaths RuntimePaths::fromExecutable() {
  return RuntimePaths(getExecutablePath());
}

const std::filesystem::path& RuntimePaths::getProjectRoot() const {
  return m_project_root;
}

const std::filesystem::path& RuntimePaths::getAssetDirectory() const {
  return m_asset_directory;
}

const std::filesystem::path& RuntimePaths::getModelDirectory() const {
  return m_model_directory;
}

std::filesystem::path RuntimePaths::getProjectPath(
    const std::filesystem::path& parRelativePath) const {
  return m_project_root / parRelativePath;
}

std::filesystem::path RuntimePaths::getAssetPath(
    const std::filesystem::path& parRelativePath) const {
  return m_asset_directory / parRelativePath;
}

std::filesystem::path RuntimePaths::getTexturePath(
    const std::filesystem::path& parRelativePath) const {
  return getAssetPath(std::filesystem::path("textures") / parRelativePath);
}

std::filesystem::path RuntimePaths::getProjectAssetCatalogPath() const {
  return getProjectPath(PROJECT_ASSET_CATALOG_PATH);
}

std::filesystem::path RuntimePaths::getProjectWorldPath() const {
  return getProjectPath(PROJECT_WORLD_PATH);
}

std::filesystem::path RuntimePaths::getLocalSessionPath() const {
  return getProjectPath(LOCAL_SESSION_PATH);
}

}  // namespace kage::platform
