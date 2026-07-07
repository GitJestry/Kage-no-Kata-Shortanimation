#include "assets/asset_path.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace kage::assets {

bool hasGltfExtension(const std::filesystem::path& parPath) {
  std::string extension = parPath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char parCharacter) {
                   return static_cast<char>(std::tolower(parCharacter));
                 });
  return extension == ".glb" || extension == ".gltf";
}

std::string defaultAssetLabelFromPath(const std::filesystem::path& parPath) {
  const std::string stem = parPath.stem().string();
  return stem.empty() ? "Asset" : stem;
}

bool isInsideDirectory(const std::filesystem::path& parPath,
                       const std::filesystem::path& parRoot) {
  const std::filesystem::path absolute_path =
      std::filesystem::weakly_canonical(parPath);
  const std::filesystem::path absolute_root =
      std::filesystem::weakly_canonical(parRoot);
  auto path_it = absolute_path.begin();
  for (auto root_it = absolute_root.begin(); root_it != absolute_root.end();
       ++root_it, ++path_it) {
    if (path_it == absolute_path.end() || *path_it != *root_it) {
      return false;
    }
  }
  return true;
}

std::filesystem::path makeUniqueDestination(
    const std::filesystem::path& parDirectory,
    const std::filesystem::path& parFilename) {
  std::filesystem::path destination = parDirectory / parFilename.filename();
  if (!std::filesystem::exists(destination)) {
    return destination;
  }

  const std::string stem = parFilename.stem().string();
  const std::string extension = parFilename.extension().string();
  for (int suffix = 1; suffix < 10000; ++suffix) {
    destination =
        parDirectory / (stem + "_" + std::to_string(suffix) + extension);
    if (!std::filesystem::exists(destination)) {
      return destination;
    }
  }
  return destination;
}

std::filesystem::path copyIntoProjectAssets(
    const std::filesystem::path& parSourcePath,
    const std::filesystem::path& parDestinationDirectory,
    const std::filesystem::path& parAssetRoot, std::string& parError) {
  if (isInsideDirectory(parSourcePath, parAssetRoot)) {
    return std::filesystem::weakly_canonical(parSourcePath);
  }

  std::filesystem::create_directories(parDestinationDirectory);
  const std::filesystem::path destination =
      makeUniqueDestination(parDestinationDirectory, parSourcePath.filename());
  std::error_code error_code;
  std::filesystem::copy_file(parSourcePath, destination,
                             std::filesystem::copy_options::none, error_code);
  if (error_code) {
    parError = error_code.message();
    return {};
  }
  return destination;
}

}  // namespace kage::assets
