#include "render/texture_resource_cache.hpp"

#include <span>

namespace {

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
  return hash;
}

std::shared_ptr<Texture2D> TextureResourceCache::acquire(
    const assets::StaticImage& parImage, TextureColorSpace parColorSpace) {
  const Key key{hashImage(parImage), parImage.width, parImage.height,
                parImage.component_count, parColorSpace};
  if (const auto entry = m_entries.find(key); entry != m_entries.end()) {
    if (std::shared_ptr<Texture2D> texture = entry->second.texture.lock()) {
      return texture;
    }
    m_entries.erase(entry);
  }

  auto texture = std::make_shared<Texture2D>();
  texture->upload(parImage.width, parImage.height, parImage.component_count,
                  parImage.pixels, parColorSpace);
  m_entries.emplace(
      key, Entry{texture, estimateMipBytes(parImage.width, parImage.height,
                                           parImage.component_count)});
  return texture;
}

void TextureResourceCache::clear() {
  m_entries.clear();
}

std::size_t TextureResourceCache::getResidentBytes() const {
  std::size_t bytes = 0;
  for (const auto& [key, entry] : m_entries) {
    if (!entry.texture.expired()) {
      bytes += entry.resident_bytes;
    }
  }
  return bytes;
}

}  // namespace kage::render
