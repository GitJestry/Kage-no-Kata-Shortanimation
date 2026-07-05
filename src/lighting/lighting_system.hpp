#pragma once

#include "lighting/light.hpp"

namespace kage::lighting {

class LightingSystem final {
 public:
  void setState(const LightingState& parState);
  [[nodiscard]] const LightingState& getState() const;
  [[nodiscard]] LightingState& getState();

 private:
  LightingState m_state;
};

}  // namespace kage::lighting
