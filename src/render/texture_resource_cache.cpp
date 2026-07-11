#include "render/texture_resource_cache.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

namespace {

constexpr int MAX_PROXY_DIMENSION = 1024;
constexpr int MIN_PROXY_DIMENSION = 64;
constexpr std::size_t PROXY_TEXTURE_BUDGET = 512ull * 1024ull * 1024ull;
constexpr std::size_t FINAL_TEXTURE_BUDGET = 3ull * 1024ull * 1024ull * 1024ull;

[[nodiscard]] std::uint64_t hashImage(
    const kage::assets::StaticImage& parImage) {
  constexpr std::uint64_t FNV_OFFSET = 14695981039346656037ull;
  constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
  std::uint64_t hash = FNV_OFFSET;
  const auto append = [&](std::span<const unsigned char> bytes) {
    for (const unsigned char value : bytes) {
      hash ^= value;
      hash *= FNV_PRIME;
    }
  };
  append(parImage.pixels);
  return hash;
}

[[nodiscard]] std::size_t estimateMipBytes(int parWidth, int parHeight,
                                           int parComponents) {
  const std::size_t base = static_cast<std::size_t>(parWidth) *
                           static_cast<std::size_t>(parHeight) *
                           static_cast<std::size_t>(parComponents);
  return base + base / 3;
}

struct ImageView final {
  int width = 0;
  int height = 0;
  std::vector<unsigned char> owned_pixels;
  std::span<const unsigned char> pixels;
};

[[nodiscard]] ImageView resizeImage(const kage::assets::StaticImage& parImage,
                                    int parMaxDimension) {
  ImageView result;
  const int largest = std::max(parImage.width, parImage.height);
  if (largest <= parMaxDimension) {
    result.width = parImage.width;
    result.height = parImage.height;
    result.pixels = parImage.pixels;
    return result;
  }

  const float scale = static_cast<float>(parMaxDimension) /
                      static_cast<float>(largest);
  result.width = std::max(1, static_cast<int>(parImage.width * scale));
  result.height = std::max(1, static_cast<int>(parImage.height * scale));
  result.owned_pixels.resize(static_cast<std::size_t>(result.width) *
                             static_cast<std::size_t>(result.height) *
                             static_cast<std::size_t>(
                                 parImage.component_count));
  for (int y = 0; y < result.height; ++y) {
    const int source_y = std::min(
        parImage.height - 1,
        static_cast<int>((static_cast<long long>(y) * parImage.height) /
                         result.height));
    for (int x = 0; x < result.width; ++x) {
      const int source_x = std::min(
          parImage.width - 1,
          static_cast<int>((static_cast<long long>(x) * parImage.width) /
                           result.width));
      const std::size_t source_offset =
          (static_cast<std::size_t>(source_y) * parImage.width + source_x) *
          parImage.component_count;
      const std::size_t destination_offset =
          (static_cast<std::size_t>(y) * result.width + x) *
          parImage.component_count;
      std::copy_n(parImage.pixels.data() + source_offset,
                  parImage.component_count,
                  result.owned_pixels.data() + destination_offset);
    }
  }
  result.pixels = result.owned_pixels;
  return result;
}

}  // namespace

namespace kage::render {

std::size_t TextureResourceCache::KeyHash::operator()(
    const Key& parKey) const {
  std::size_t hash = static_cast<std::size_t>(parKey.content_hash);
  const auto combine = [&](std::size_t value) {
    hash ^= value + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
  };
  combine(static_cast<std::size_t>(parKey.width));
  combine(static_cast<std::size_t>(parKey.height));
  combine(static_cast<std::size_t>(parKey.component_count));
  combine(static_cast<std::size_t>(parKey.color_space));
  combine(static_cast<std::size_t>(parKey.quality));
  return hash;
}

std::shared_ptr<Texture2D> TextureResourceCache::acquire(
    const assets::StaticImage& parImage, TextureColorSpace parColorSpace,
    assets::AssetQualityTier parQuality) {
  const Key key{hashImage(parImage), parImage.width, parImage.height,
                parImage.component_count, parColorSpace, parQuality};
  if (const auto entry = m_entries.find(key); entry != m_entries.end()) {
    if (std::shared_ptr<Texture2D> texture = entry->second.texture.lock()) {
      return texture;
    }
    m_entries.erase(entry);
  }

  int max_dimension = std::max(parImage.width, parImage.height);
  const std::size_t budget = parQuality == assets::AssetQualityTier::Proxy
                                 ? PROXY_TEXTURE_BUDGET
                                 : FINAL_TEXTURE_BUDGET;
  if (parQuality == assets::AssetQualityTier::Proxy) {
    max_dimension = std::min(max_dimension, MAX_PROXY_DIMENSION);
    while (max_dimension > MIN_PROXY_DIMENSION) {
      const float scale = static_cast<float>(max_dimension) /
                          static_cast<float>(
                              std::max(parImage.width, parImage.height));
      const int width = std::max(1, static_cast<int>(parImage.width * scale));
      const int height =
          std::max(1, static_cast<int>(parImage.height * scale));
      if (getResidentBytes(parQuality) +
              estimateMipBytes(width, height, parImage.component_count) <=
          budget) {
        break;
      }
      max_dimension /= 2;
    }
  }

  ImageView image = resizeImage(parImage, max_dimension);
  auto texture = std::make_shared<Texture2D>();
  texture->upload(image.width, image.height, parImage.component_count,
                  image.pixels, parColorSpace);
  m_entries.emplace(
      key, Entry{texture, estimateMipBytes(image.width, image.height,
                                           parImage.component_count)});
  return texture;
}

void TextureResourceCache::releaseExpired(
    assets::AssetQualityTier parQuality) {
  std::erase_if(m_entries, [&](const auto& item) {
    return item.first.quality == parQuality && item.second.texture.expired();
  });
}

void TextureResourceCache::clear() {
  m_entries.clear();
}

std::size_t TextureResourceCache::getResidentBytes(
    assets::AssetQualityTier parQuality) const {
  std::size_t bytes = 0;
  for (const auto& [key, entry] : m_entries) {
    if (key.quality == parQuality && !entry.texture.expired()) {
      bytes += entry.resident_bytes;
    }
  }
  return bytes;
}

}  // namespace kage::render
