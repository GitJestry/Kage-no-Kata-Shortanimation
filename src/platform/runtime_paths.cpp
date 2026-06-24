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
#endif

namespace {

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
  return std::filesystem::current_path();
#endif
}

}  // namespace

namespace kage::platform {

RuntimePaths::RuntimePaths(std::filesystem::path parExecutablePath)
    : m_executable_directory(std::filesystem::absolute(parExecutablePath)
                                 .parent_path()),
      m_asset_directory(m_executable_directory / "assets") {}

RuntimePaths RuntimePaths::fromExecutable() {
  return RuntimePaths(getExecutablePath());
}

const std::filesystem::path& RuntimePaths::getExecutableDirectory() const {
  return m_executable_directory;
}

const std::filesystem::path& RuntimePaths::getAssetDirectory() const {
  return m_asset_directory;
}

std::filesystem::path RuntimePaths::getAssetPath(
    const std::filesystem::path& parRelativePath) const {
  return m_asset_directory / parRelativePath;
}

std::filesystem::path RuntimePaths::getModelPath(
    const std::filesystem::path& parRelativePath) const {
  return getAssetPath(std::filesystem::path("models") / parRelativePath);
}

std::filesystem::path RuntimePaths::getTexturePath(
    const std::filesystem::path& parRelativePath) const {
  return getAssetPath(std::filesystem::path("textures") / parRelativePath);
}

std::filesystem::path RuntimePaths::getAudioPath(
    const std::filesystem::path& parRelativePath) const {
  return getAssetPath(std::filesystem::path("audio") / parRelativePath);
}

}  // namespace kage::platform
