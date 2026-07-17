#include "check_helpers.hpp"

#include "platform/runtime_paths.hpp"

#include <exception>
#include <filesystem>

namespace {

[[nodiscard]] bool testExecutableRelativePaths() {
  std::filesystem::current_path(std::filesystem::temp_directory_path());

  const kage::platform::RuntimePaths paths = kage::platform::RuntimePaths::fromExecutable();
  const std::filesystem::path expected_root = kage::platform::canonicalOrAbsolute(KAGE_SOURCE_DIR);
  return paths.getProjectRoot() == expected_root &&
         paths.getAssetDirectory() == expected_root / "assets" &&
         paths.getModelDirectory() == expected_root / "assets" / "models" &&
         paths.getProjectAssetCatalogPath() ==
             expected_root / "projects" / "kage_no_kata_assets.kage.json" &&
         paths.getProjectWorldPath() == expected_root / "projects" / "kage_no_kata_world.kage.json";
}

} // namespace

int main() {
  try {
    return testExecutableRelativePaths()
               ? 0
               : kage::test::fail("executable-relative runtime path regression");
  } catch (const std::exception& error) {
    return kage::test::fail(error.what());
  }
}
