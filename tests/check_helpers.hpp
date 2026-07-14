#pragma once

#include <cmath>
#include <iostream>
#include <string_view>

namespace kage::test {

[[nodiscard]] inline bool close(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) < 0.001f;
}

inline int fail(std::string_view parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace kage::test
