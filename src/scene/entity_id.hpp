#pragma once

#include <cstdint>
#include <limits>

namespace kage::scene {

struct EntityId final {
  std::uint32_t value = std::numeric_limits<std::uint32_t>::max();

  [[nodiscard]] bool isValid() const;
  friend bool operator==(EntityId parLeft, EntityId parRight) = default;
};

inline bool EntityId::isValid() const {
  return value != std::numeric_limits<std::uint32_t>::max();
}

}  // namespace kage::scene
