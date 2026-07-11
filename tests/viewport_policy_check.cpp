#include "math/bounds.hpp"
#include "render/viewport_policy.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/epsilon.hpp>

#include <cassert>
#include <cmath>

int main() {
  kage::math::Bounds3 local;
  local.includePoint(glm::vec3(-1.0f));
  local.includePoint(glm::vec3(1.0f));
  const glm::mat4 transform =
      glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 2.0f, -4.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 0.5f));
  const kage::math::Bounds3 world =
      kage::math::transformBounds(local, transform);
  assert(world.is_valid);
  assert(glm::all(glm::epsilonEqual(world.min, glm::vec3(1.0f, 1.0f, -4.5f),
                                   0.0001f)));
  assert(glm::all(glm::epsilonEqual(world.max, glm::vec3(5.0f, 3.0f, -3.5f),
                                   0.0001f)));

  kage::math::Bounds3 visible;
  visible.includePoint(glm::vec3(-0.5f));
  visible.includePoint(glm::vec3(0.5f));
  assert(kage::render::ViewportPolicy::isVisible(visible, glm::mat4(1.0f)));
  const kage::math::Bounds3 outside = kage::math::transformBounds(
      visible,
      glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f)));
  assert(!kage::render::ViewportPolicy::isVisible(outside, glm::mat4(1.0f)));

  using kage::render::ViewportMode;
  using kage::render::ViewportPolicy;
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 400.0f, false) == 0);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 200.0f, false) == 1);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 40.0f, false) == 2);
  assert(ViewportPolicy::selectLod(ViewportMode::Solid, 400.0f, false) == 2);
  assert(ViewportPolicy::selectLod(ViewportMode::Final, 1.0f, false) == 0);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 1.0f, true) == 0);

  // A previous LOD remains stable inside the 15% transition band.
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 320.0f, false, 0) ==
         0);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 390.0f, false, 1) ==
         1);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 85.0f, false, 2) ==
         2);
  assert(ViewportPolicy::selectLod(ViewportMode::Material, 60.0f, false, 1) ==
         2);

  kage::camera::Camera camera;
  camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
  camera.vertical_fov_degrees = 90.0f;
  const float pixels = ViewportPolicy::projectedDiameterPixels(
      visible, camera, glm::vec2(1920.0f, 1080.0f));
  assert(std::abs(pixels - 54.0f) < 0.1f);
  return 0;
}
