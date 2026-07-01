#include "lighting/lighting_system.hpp"

namespace kage::lighting {

void LightingSystem::setState(const LightingState& parState) {
  m_state = parState;
}

const LightingState& LightingSystem::getState() const {
  return m_state;
}

LightingState& LightingSystem::getState() {
  return m_state;
}

}  // namespace kage::lighting
