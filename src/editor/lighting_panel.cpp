#include "editor/lighting_panel.hpp"

#include <glm/glm.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {

[[nodiscard]] glm::vec2 directionToYawPitch(const glm::vec3& parDirection) {
  const glm::vec3 direction = glm::normalize(parDirection);
  return glm::vec2(glm::degrees(std::atan2(direction.x, direction.z)),
                   glm::degrees(std::asin(std::clamp(direction.y, -1.0f,
                                                     1.0f))));
}

[[nodiscard]] glm::vec3 yawPitchToDirection(const glm::vec2& parYawPitch) {
  const float yaw = glm::radians(parYawPitch.x);
  const float pitch = glm::radians(parYawPitch.y);
  const float horizontal = std::cos(pitch);
  return glm::normalize(glm::vec3(std::sin(yaw) * horizontal,
                                  std::sin(pitch),
                                  std::cos(yaw) * horizontal));
}

}  // namespace

namespace kage::editor {

void drawLightingPanel(engine::EngineCore& parEngine) {
  ImGui::TextUnformatted("Lighting");
  ImGui::SeparatorText("Sun");
  scene::SunLightSettings sun = parEngine.getSunLightSettings();
  bool sun_changed = false;
  sun_changed |= ImGui::Checkbox("Sun enabled", &sun.enabled);
  glm::vec2 yaw_pitch = directionToYawPitch(sun.direction_to_sun);
  if (ImGui::DragFloat2("Sun direction", &yaw_pitch.x, 0.25f, -180.0f,
                        180.0f, "%.1f deg")) {
    yaw_pitch.y = std::clamp(yaw_pitch.y, -89.0f, 89.0f);
    sun.direction_to_sun = yawPitchToDirection(yaw_pitch);
    sun_changed = true;
  }
  sun_changed |= ImGui::ColorEdit3("Sun color", &sun.color.x);
  sun_changed |= ImGui::DragFloat("Sun intensity", &sun.intensity, 0.02f,
                                  0.0f, 20.0f);
  if (sun_changed) {
    parEngine.setSunLightSettings(sun);
  }

  ImGui::SeparatorText("Environment");
  float exposure = parEngine.getLightingState().exposure;
  ImGui::TextDisabled("Illumination comes from the sky or panorama");
  if (ImGui::DragFloat("Exposure", &exposure, 0.02f, 0.0f, 8.0f)) {
    parEngine.setExposure(exposure);
  }

  int active_lights = sun.enabled && sun.intensity > 0.0f ? 1 : 0;
  for (const scene::EntityRecord& entity : parEngine.getWorld().getEntities()) {
    if (!entity.alive || !entity.light.has_value() ||
        entity.light->type != scene::LightType::Point ||
        !entity.light->enabled) {
      continue;
    }
    ++active_lights;
  }
  ImGui::TextDisabled("Active lights: %d", active_lights);
  ImGui::BeginChild("ActiveLightList", ImVec2(0.0f, 84.0f), true);
  for (const scene::EntityRecord& entity : parEngine.getWorld().getEntities()) {
    if (!entity.alive || !entity.light.has_value()) {
      continue;
    }
    if (entity.light->type != scene::LightType::Point) {
      continue;
    }
    ImGui::Text("%s  Point  %.2f", entity.name.name.c_str(),
                entity.light->intensity);
  }
  ImGui::EndChild();
}

}  // namespace kage::editor
