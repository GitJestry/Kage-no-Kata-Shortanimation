#include "check_helpers.hpp"

#include "editor/gizmo_controller.hpp"
#include "editor/movie_editor_layout.hpp"
#include "scene/scene_manager.hpp"

namespace {

using kage::test::close;
using kage::test::fail;

[[nodiscard]] bool testWorldLifecycle() {
  kage::scene::World world;
  const kage::scene::EntityId first = world.createEntity("First");
  const kage::scene::EntityId explicit_id = world.createEntityWithId("Explicit", {42});
  const kage::scene::EntityId next = world.createEntity("Next");
  if (!first.isValid() || explicit_id.value != 42 || next.value <= 42 ||
      world.getEntities().size() != 3) {
    return false;
  }

  kage::scene::CameraComponent camera;
  camera.vertical_fov_degrees = 65.0f;
  world.setCamera(first, camera);
  kage::scene::LightComponent light;
  light.intensity = 7.0f;
  world.setLight(first, light);
  const kage::scene::EntityRecord* record = world.findEntity(first);
  if (record == nullptr || !record->camera.has_value() || !record->light.has_value() ||
      !close(record->camera->vertical_fov_degrees, 65.0f)) {
    return false;
  }
  return world.deleteEntity(first) && !world.deleteEntity(first) &&
         world.findEntity(first) == nullptr;
}

[[nodiscard]] bool testSceneOwnership() {
  kage::scene::SceneManager scenes;
  const std::size_t first = scenes.createScene("A");
  const std::size_t second = scenes.createScene("B");
  scenes.setActiveScene(second);
  scenes.renameScene(second, "Renamed");
  const kage::scene::EntityId entity = scenes.getActiveScene().world.createEntity("Selected");
  scenes.selectEntity(entity);
  if (first != 0 || second != 1 || scenes.getActiveSceneIndex() != second ||
      scenes.getActiveScene().name != "Renamed" || scenes.getSelectedEntity() != entity) {
    return false;
  }
  if (!scenes.deleteScene(first) || scenes.getScenes().size() != 1 ||
      scenes.getActiveSceneIndex() != 0) {
    return false;
  }
  scenes.clearScenes();
  return scenes.getScenes().empty();
}

[[nodiscard]] bool testEditorStateAndLayout() {
  const kage::editor::MovieEditorLayout layout =
      kage::editor::computeMovieEditorLayout({10.0f, 20.0f}, {1200.0f, 800.0f}, 300.0f);
  const glm::vec2 preview_size = layout.film_preview.max - layout.film_preview.min;
  if (preview_size.x <= 0.0f || preview_size.y <= 0.0f ||
      !close(preview_size.x / preview_size.y, kage::film::FILM_OUTPUT_ASPECT_RATIO) ||
      layout.timeline.min.y != 520.0f) {
    return false;
  }

  kage::editor::GizmoController gizmo;
  gizmo.setMode(kage::editor::GizmoController::TransformMode::Rotate);
  gizmo.setAxisSpace(kage::editor::GizmoController::AxisSpace::World);
  if (gizmo.getMode() != kage::editor::GizmoController::TransformMode::Rotate ||
      gizmo.getAxisSpace() != kage::editor::GizmoController::AxisSpace::World || gizmo.isActive()) {
    return false;
  }

  return true;
}

} // namespace

int main() {
  if (!testWorldLifecycle()) {
    return fail("World entity/component lifecycle regression");
  }
  if (!testSceneOwnership()) {
    return fail("Scene ownership or selection regression");
  }
  if (!testEditorStateAndLayout()) {
    return fail("Editor state or layout regression");
  }
  return 0;
}
