#include "render/environment_image.hpp"

#include <stb_image.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>

namespace {

[[nodiscard]] glm::ivec2 fitSize(int parWidth, int parHeight,
                                 int parMaxWidth, int parMaxHeight) {
  const float scale = std::min(
      {1.0f, static_cast<float>(std::max(parMaxWidth, 1)) / parWidth,
       static_cast<float>(std::max(parMaxHeight, 1)) / parHeight});
  return {std::max(1, static_cast<int>(std::floor(parWidth * scale))),
          std::max(1, static_cast<int>(std::floor(parHeight * scale)))};
}

[[nodiscard]] bool readHdrScanline(std::ifstream& parInput, int parWidth,
                                   std::vector<unsigned char>& parRgbe) {
  std::array<unsigned char, 4> header{};
  parInput.read(reinterpret_cast<char*>(header.data()), header.size());
  if (!parInput || header[0] != 2 || header[1] != 2 ||
      (header[2] & 0x80u) != 0 ||
      ((static_cast<int>(header[2]) << 8) | header[3]) != parWidth) {
    return false;
  }
  parRgbe.resize(static_cast<std::size_t>(parWidth) * 4);
  for (int channel = 0; channel < 4; ++channel) {
    int x = 0;
    while (x < parWidth) {
      unsigned char count = 0;
      unsigned char value = 0;
      parInput.read(reinterpret_cast<char*>(&count), 1);
      parInput.read(reinterpret_cast<char*>(&value), 1);
      if (!parInput || count == 0) {
        return false;
      }
      if (count > 128) {
        const int run = count - 128;
        if (x + run > parWidth) {
          return false;
        }
        for (int index = 0; index < run; ++index) {
          parRgbe[static_cast<std::size_t>(x++) * 4 + channel] = value;
        }
      } else {
        if (x + count > parWidth) {
          return false;
        }
        parRgbe[static_cast<std::size_t>(x++) * 4 + channel] = value;
        for (int index = 1; index < count; ++index) {
          parInput.read(reinterpret_cast<char*>(&value), 1);
          if (!parInput) {
            return false;
          }
          parRgbe[static_cast<std::size_t>(x++) * 4 + channel] = value;
        }
      }
    }
  }
  return true;
}

[[nodiscard]] kage::render::DecodedEnvironmentImage decodeHdr(
    const std::filesystem::path& parPath, int parMaxWidth, int parMaxHeight) {
  kage::render::DecodedEnvironmentImage image;
  image.hdr = true;
  std::ifstream input(parPath, std::ios::binary);
  std::string line;
  if (!std::getline(input, line) || line.rfind("#?", 0) != 0) {
    image.error = "Invalid Radiance HDR header";
    return image;
  }
  while (std::getline(input, line) && !line.empty() && line != "\r") {
  }
  char y_sign = 0;
  char x_sign = 0;
  char y_axis = 0;
  char x_axis = 0;
  if (!(input >> y_sign >> y_axis >> image.source_height >> x_sign >> x_axis >>
        image.source_width) ||
      y_axis != 'Y' || x_axis != 'X' || y_sign != '-' || x_sign != '+' ||
      image.source_width <= 0 || image.source_height <= 0) {
    image.error = "Unsupported Radiance HDR orientation";
    return image;
  }
  input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  const glm::ivec2 output = fitSize(image.source_width, image.source_height,
                                    parMaxWidth, parMaxHeight);
  image.width = output.x;
  image.height = output.y;
  image.hdr_pixels.resize(static_cast<std::size_t>(image.width) * image.height *
                          3);
  std::vector<unsigned char> scanline;
  int target_y = 0;
  for (int source_y = 0; source_y < image.source_height; ++source_y) {
    if (!readHdrScanline(input, image.source_width, scanline)) {
      image.error = "Could not decode Radiance HDR scanline";
      image.hdr_pixels.clear();
      return image;
    }
    while (target_y < image.height &&
           std::min(image.source_height - 1,
                    static_cast<int>((static_cast<double>(target_y) + 0.5) *
                                     image.source_height / image.height)) ==
               source_y) {
      for (int target_x = 0; target_x < image.width; ++target_x) {
        const int source_x = std::min(
            image.source_width - 1,
            static_cast<int>((static_cast<double>(target_x) + 0.5) *
                             image.source_width / image.width));
        const unsigned char* rgbe =
            &scanline[static_cast<std::size_t>(source_x) * 4];
        glm::vec3 value(0.0f);
        if (rgbe[3] != 0) {
          const float scale = std::ldexp(1.0f, rgbe[3] - (128 + 8));
          value = glm::vec3(rgbe[0], rgbe[1], rgbe[2]) * scale;
        }
        const std::size_t pixel =
            static_cast<std::size_t>(target_y * image.width + target_x) * 3;
        image.hdr_pixels[pixel] = value.r;
        image.hdr_pixels[pixel + 1] = value.g;
        image.hdr_pixels[pixel + 2] = value.b;
      }
      ++target_y;
    }
  }
  if (target_y != image.height) {
    image.error = "Radiance HDR ended before all output rows were decoded";
    image.hdr_pixels.clear();
    return image;
  }
  return image;
}

}  // namespace

namespace kage::render {

DecodedEnvironmentImage decodeEnvironmentImage(
    const std::filesystem::path& parPath, int parMaxWidth, int parMaxHeight) {
  const std::string path = parPath.string();
  if (stbi_is_hdr(path.c_str()) != 0) {
    return decodeHdr(parPath, parMaxWidth, parMaxHeight);
  }
  DecodedEnvironmentImage image;
  int components = 0;
  std::unique_ptr<unsigned char, decltype(&stbi_image_free)> source(
      stbi_load(path.c_str(), &image.source_width, &image.source_height,
                &components, 4),
      stbi_image_free);
  if (!source || image.source_width <= 0 || image.source_height <= 0) {
    image.error = "Could not decode panorama";
    return image;
  }
  const glm::ivec2 output = fitSize(image.source_width, image.source_height,
                                    parMaxWidth, parMaxHeight);
  image.width = output.x;
  image.height = output.y;
  image.ldr_pixels.resize(
      static_cast<std::size_t>(image.width) * image.height * 4);
  for (int y = 0; y < image.height; ++y) {
    const int source_y = std::min(
        image.source_height - 1,
        static_cast<int>((static_cast<double>(y) + 0.5) *
                         image.source_height / image.height));
    for (int x = 0; x < image.width; ++x) {
      const int source_x = std::min(
          image.source_width - 1,
          static_cast<int>((static_cast<double>(x) + 0.5) *
                           image.source_width / image.width));
      const unsigned char* input =
          source.get() + static_cast<std::size_t>(
                             (source_y * image.source_width + source_x) * 4);
      unsigned char* output_pixel =
          image.ldr_pixels.data() +
          static_cast<std::size_t>((y * image.width + x) * 4);
      std::copy_n(input, 4, output_pixel);
    }
  }
  return image;
}

}  // namespace kage::render
