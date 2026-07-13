#pragma once

#include "assets/asset_types.hpp"
#include "render/texture_2d.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace kage::render {

class TextureResourceCache final {
 public:
  [[nodiscard]] std::shared_ptr<Texture2D> acquire(
      const assets::StaticImage& parImage, TextureColorSpace parColorSpace);
  void clear();
  [[nodiscard]] std::size_t getResidentBytes() const;

 private:
  struct Key final {
    std::uint64_t content_hash = 0;
    int width = 0;
    int height = 0;
    int component_count = 0;
    TextureColorSpace color_space = TextureColorSpace::Linear;
    friend bool operator==(const Key&, const Key&) = default;
  };

  struct KeyHash final {
    [[nodiscard]] std::size_t operator()(const Key& parKey) const;
  };

  struct Entry final {
    std::weak_ptr<Texture2D> texture;
    std::size_t resident_bytes = 0;
  };

  std::unordered_map<Key, Entry, KeyHash> m_entries;
};

}  // namespace kage::render
