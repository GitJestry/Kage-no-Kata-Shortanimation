#pragma once

#include <filesystem>
#include <string>

namespace kage::assets {

[[nodiscard]] bool hasGltfExtension(const std::filesystem::path& parPath);
[[nodiscard]] bool hasPanoramaExtension(const std::filesystem::path& parPath);
[[nodiscard]] bool hasHdrExtension(const std::filesystem::path& parPath);
[[nodiscard]] std::string defaultAssetLabelFromPath(
    const std::filesystem::path& parPath);
[[nodiscard]] bool isInsideDirectory(const std::filesystem::path& parPath,
                                     const std::filesystem::path& parRoot);
[[nodiscard]] std::filesystem::path makeUniqueDestination(
    const std::filesystem::path& parDirectory,
    const std::filesystem::path& parFilename);
[[nodiscard]] std::filesystem::path copyIntoProjectAssets(
    const std::filesystem::path& parSourcePath,
    const std::filesystem::path& parDestinationDirectory,
    const std::filesystem::path& parAssetRoot, std::string& parError);

}  // namespace kage::assets
