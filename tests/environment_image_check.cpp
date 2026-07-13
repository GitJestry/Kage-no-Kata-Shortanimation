#include "render/environment_image.hpp"
#include "assets/asset_path.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount != 2) {
    std::cerr << "usage: environment_image_check <panorama.hdr>\n";
    return 2;
  }
  const kage::render::DecodedEnvironmentImage image =
      kage::render::decodeEnvironmentImage(
          std::filesystem::path(parArguments[1]), 512, 256);
  if (!kage::assets::hasPanoramaExtension(parArguments[1]) ||
      kage::assets::hasPanoramaExtension("model.glb") ||
      !image.error.empty() || !image.hdr || image.source_width != 15000 ||
      image.source_height != 7500 || image.width != 512 ||
      image.height != 256 ||
      image.hdr_pixels.size() !=
          static_cast<std::size_t>(image.width * image.height * 3)) {
    std::cerr << "real HDR panorama decode failed: " << image.error << '\n';
    return 1;
  }
  return 0;
}
