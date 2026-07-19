#include "check_helpers.hpp"

#include "platform/runtime_paths.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <string>

namespace {

class ExternalBuildFixture final {
public:
  ExternalBuildFixture()
      : m_original_directory(std::filesystem::current_path()),
        m_build_directory(
            std::filesystem::temp_directory_path() /
            ("kage_runtime_paths_check_" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
    std::filesystem::create_directories(m_build_directory / "assets");
    std::filesystem::current_path(m_build_directory);
  }

  ~ExternalBuildFixture() {
    std::error_code error_code;
    std::filesystem::current_path(m_original_directory, error_code);
    std::filesystem::remove_all(m_build_directory, error_code);
  }

  [[nodiscard]] std::filesystem::path getExecutablePath() const {
    return m_build_directory / "kage_engine";
  }

private:
  std::filesystem::path m_original_directory;
  std::filesystem::path m_build_directory;
};

[[nodiscard]] bool testExternalBuildUsesSourceRoot() {
  const ExternalBuildFixture fixture;
  const kage::platform::RuntimePaths paths(fixture.getExecutablePath());
  const std::filesystem::path expected_root = kage::platform::canonicalOrAbsolute(KAGE_SOURCE_DIR);

  return paths.getProjectRoot() == expected_root &&
         paths.getAssetDirectory() == expected_root / "assets" &&
         paths.getModelDirectory() == expected_root / "assets" / "models";
}

} // namespace

int main() {
  try {
    return testExternalBuildUsesSourceRoot()
               ? 0
               : kage::test::fail("External build did not resolve the source project root");
  } catch (const std::exception& error) {
    return kage::test::fail(error.what());
  }
}
