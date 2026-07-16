#include "platform/runtime_paths.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  std::error_code error_code;
  const std::filesystem::path original_directory =
      std::filesystem::current_path(error_code);
  if (error_code) {
    return fail("Could not read the current directory");
  }

  const std::filesystem::path external_build_directory =
      std::filesystem::temp_directory_path(error_code) /
      ("kage_runtime_paths_check_" +
       std::to_string(std::chrono::steady_clock::now()
                          .time_since_epoch()
                          .count()));
  if (error_code) {
    return fail("Could not locate the temporary directory");
  }
  std::filesystem::create_directories(external_build_directory / "assets",
                                      error_code);
  if (error_code) {
    return fail("Could not create the external build fixture");
  }

  std::filesystem::current_path(external_build_directory, error_code);
  if (error_code) {
    std::filesystem::remove_all(external_build_directory, error_code);
    return fail("Could not enter the external build fixture");
  }

  const kage::platform::RuntimePaths paths(
      external_build_directory / "kage_engine");
  const std::filesystem::path source_root =
      std::filesystem::weakly_canonical(KAGE_SOURCE_ROOT);
  const bool resolved_source_root = paths.getProjectRoot() == source_root &&
                                    paths.getModelDirectory() ==
                                        source_root / "assets/models" &&
                                    paths.getProjectAssetCatalogPath() ==
                                        source_root /
                                            "projects/kage_no_kata_assets.kage.json";

  std::filesystem::current_path(original_directory, error_code);
  std::filesystem::remove_all(external_build_directory, error_code);
  if (error_code) {
    return fail(
        "Could not restore the current directory or clean up the fixture");
  }
  return resolved_source_root
             ? 0
             : fail("External build did not resolve the source project root");
}
