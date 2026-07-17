#include "check_helpers.hpp"

#include "camera/camera.hpp"
#include "camera/fly_camera_controller.hpp"
#include "camera/screen_metrics.hpp"
#include "camera/session_camera.hpp"
#include "math/bounds.hpp"
#include "math/cubic_bezier.hpp"
#include "math/screen_projection.hpp"
#include "render/gizmo_metrics.hpp"
#include "render/viewport_picking.hpp"
#include "render/viewport_policy.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <limits>

namespace {

using kage::test::close;
using kage::test::fail;

[[nodiscard]] bool closeVec3(const glm::vec3& parLeft, const glm::vec3& parRight) {
  return close(parLeft.x, parRight.x) && close(parLeft.y, parRight.y) &&
         close(parLeft.z, parRight.z);
}

[[nodiscard]] bool testBoundsAndProjection() {
  kage::math::Bounds3 bounds;
  bounds.includePoint({-1.0f, -2.0f, -3.0f});
  bounds.includePoint({2.0f, 4.0f, 6.0f});
  if (!bounds.is_valid || !closeVec3(bounds.getSize(), {3.0f, 6.0f, 9.0f})) {
    return false;
  }

  kage::math::Transform transform;
  transform.translation = {3.0f, 1.0f, -2.0f};
  transform.scale = {2.0f, 1.0f, 0.5f};
  const kage::math::Bounds3 transformed = kage::math::transformBounds(bounds, transform.toMatrix());
  if (!transformed.is_valid || !closeVec3(transformed.min, {1.0f, -1.0f, -3.5f}) ||
      !closeVec3(transformed.max, {7.0f, 5.0f, 1.0f})) {
    return false;
  }

  const glm::vec3 bezier =
      kage::math::cubicBezier(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.5f);
  if (!closeVec3(bezier, {0.75f, 0.5f, 0.0f}) ||
      !close(kage::math::distanceToSegment({1.0f, 1.0f}, {0.0f, 0.0f}, {2.0f, 0.0f}), 1.0f)) {
    return false;
  }

  const kage::math::ScreenPoint center =
      kage::math::projectPoint({0.0f, 0.0f, 0.0f}, glm::mat4(1.0f), {200.0f, 100.0f});
  return center.valid && close(center.pixel.x, 100.0f) && close(center.pixel.y, 50.0f);
}

[[nodiscard]] bool testCameraAndController() {
  kage::camera::Camera camera;
  camera.position = {0.0f, 0.0f, 5.0f};
  camera.lookAt({0.0f, 0.0f, 0.0f});
  if (!closeVec3(camera.getForward(), {0.0f, 0.0f, -1.0f}) ||
      !closeVec3(camera.getRight(), {1.0f, 0.0f, 0.0f})) {
    return false;
  }

  kage::camera::FlyCameraController controller;
  controller.setMoveSpeed(100.0f);
  if (!close(controller.getMoveSpeed(), 20.0f)) {
    return false;
  }
  kage::camera::FlyCameraInput input;
  input.forward = true;
  controller.update(camera, input, 0.25f);
  if (!closeVec3(camera.position, {0.0f, 0.0f, 0.0f})) {
    return false;
  }
  controller.adjustSpeed(-100.0f);
  if (!close(controller.getMoveSpeed(), 0.2f)) {
    return false;
  }

  kage::math::Transform session;
  session.translation = camera.position;
  session.rotation = camera.orientation;
  if (!kage::camera::isValidSessionCamera(session, 50.0f, 0.01f, 500.0f)) {
    return false;
  }
  session.translation.x = std::numeric_limits<float>::infinity();
  return !kage::camera::isValidSessionCamera(session, 50.0f, 0.01f, 500.0f);
}

[[nodiscard]] bool testPickingAndPolicies() {
  kage::camera::Camera camera;
  camera.position = {0.0f, 0.0f, 5.0f};
  camera.lookAt({0.0f, 0.0f, 0.0f});
  const kage::render::ViewportPickRay ray =
      kage::render::makeViewportPickRay(camera, {100.0f, 100.0f}, {200.0f, 200.0f});
  float distance = 0.0f;
  if (!kage::render::viewportRayIntersectsBounds(
          ray, kage::math::makePointBounds({0.0f, 0.0f, 0.0f}, 0.5f), distance) ||
      distance <= 0.0f) {
    return false;
  }

  kage::scene::World world;
  const kage::scene::EntityId near_entity = world.createEntity("Near");
  const kage::scene::EntityId far_entity = world.createEntity("Far");
  world.findEntity(near_entity)->transform.transform.translation = {0.0f, 0.0f, 0.0f};
  world.findEntity(far_entity)->transform.transform.translation = {0.0f, 0.0f, -2.0f};
  const auto picked = kage::render::pickViewportEntityBounds(
      world, &camera, nullptr, {100.0f, 100.0f}, {200.0f, 200.0f}, 0.5f);
  if (!picked.has_value() || *picked != near_entity) {
    return false;
  }

  const kage::math::Bounds3 visible = kage::math::makePointBounds({0.0f, 0.0f, 0.0f}, 0.25f);
  const kage::scene::EntityRecord* entity = world.findEntity(near_entity);
  return entity != nullptr && kage::render::isVisible(visible, glm::mat4(1.0f)) &&
         kage::camera::getWorldUnitsPerPixel(camera, {0.0f, 0.0f, 0.0f}, {1000.0f, 1000.0f}) >
             0.0f &&
         kage::render::getGizmoLength(camera, {1000.0f, 1000.0f}, *entity, visible) > 0.0f;
}

} // namespace

int main() {
  if (!testBoundsAndProjection()) {
    return fail("bounds, projection, or curve regression");
  }
  if (!testCameraAndController()) {
    return fail("camera or fly-controller regression");
  }
  if (!testPickingAndPolicies()) {
    return fail("viewport picking or policy regression");
  }
  return 0;
}
