#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

namespace kage::render {

struct DecodedEnvironmentImage final {
  std::uint64_t generation = 0;
  int source_width = 0;
  int source_height = 0;
  int width = 0;
  int height = 0;
  bool hdr = false;
  std::vector<unsigned char> ldr_pixels;
  std::vector<float> hdr_pixels;
  std::string error;
};

[[nodiscard]] DecodedEnvironmentImage decodeEnvironmentImage(
    const std::filesystem::path& parPath, int parMaxWidth = 8192,
    int parMaxHeight = 4096);

}  // namespace kage::render
