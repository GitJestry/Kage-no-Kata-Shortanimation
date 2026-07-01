#include "editor/world_editor.hpp"

#include "assets/gltf_asset_loader.hpp"
#include "assets/model_validation.hpp"

#include <exception>
#include <filesystem>
#include <utility>

namespace kage::editor {

WorldEditor::WorldEditor(engine::EngineCore& parEngine)
    : m_engine(parEngine),
      m_runtime_paths(platform::RuntimePaths::fromExecutable()) {
  registerDefaultAssets();
  if (!m_engine.loadProject()) {
    m_engine.createDefaultProject();
  }
  m_engine.loadLocalSession();
  m_gizmo_controller.setAxisSpace(
      m_engine.getGizmoAxisSpace() == render::GizmoAxisSpace::World
          ? GizmoController::AxisSpace::World
          : GizmoController::AxisSpace::Local);
}

WorldEditor::~WorldEditor() {
  m_engine.saveLocalSession();
}

void WorldEditor::update(float parDeltaSeconds,
                         const input::EditorInputSnapshot& parInput) {
  m_selection_controller.update(parDeltaSeconds);
  m_engine.setGizmoAxisSpace(
      m_gizmo_controller.getAxisSpace() == GizmoController::AxisSpace::World
          ? render::GizmoAxisSpace::World
          : render::GizmoAxisSpace::Local);
  if (parInput.key_tab_pressed && !parInput.wants_capture_keyboard) {
    m_ui.togglePanelVisibility();
  }
  if (parInput.key_delete_pressed && !parInput.wants_capture_keyboard &&
      !parInput.wants_capture_mouse) {
    if (!cancelActiveOperation()) {
      m_engine.deleteEntity(m_engine.getSelectedEntity());
    }
  }

  applyCameraMovement(parInput);
  handlePointerInput(parInput);
  m_engine.update(parDeltaSeconds);
}

void WorldEditor::render(const glm::vec2& parViewportSize) {
  m_engine.render(parViewportSize);
}

void WorldEditor::buildImGui(const glm::vec2& parViewportSize,
                             float parDeltaSeconds,
                             unsigned int parFrameCount) {
  m_ui.draw(m_engine, m_placement_controller, m_selection_controller,
            m_gizmo_controller, parViewportSize, parDeltaSeconds,
            parFrameCount);
}

bool WorldEditor::cancelActiveOperation() {
  bool cancelled = false;
  cancelled |= m_placement_controller.cancel(m_engine);
  if (m_gizmo_controller.isActive()) {
    m_gizmo_controller.end();
    cancelled = true;
  }
  return cancelled;
}

void WorldEditor::registerDefaultAssets() {
  m_engine.registerStaticAsset("Sword", m_runtime_paths.getModelPath("sword.glb"));
  m_engine.registerStaticAsset("Torii gate",
                               m_runtime_paths.getModelPath("torii_gate.glb"));
  const std::filesystem::path samurai_path =
      m_runtime_paths.getModelPath("samurai.glb");
  if (!std::filesystem::exists(samurai_path)) {
    return;
  }

  try {
    assets::GltfAssetLoader loader;
    assets::ModelAsset samurai = loader.loadDocument(samurai_path);
    const assets::RigValidationReport report =
        assets::validateRiggedModelAsset(samurai);
    if (!report.passed()) {
      return;
    }
    m_engine.registerModelAsset("Samurai", samurai_path, std::move(samurai));
  } catch (const std::exception&) {
    return;
  }
}

void WorldEditor::applyCameraMovement(
    const input::EditorInputSnapshot& parInput) {
  const bool viewport_active =
      parInput.viewport_hovered && !m_ui.isCursorOverPanel(parInput.ui_cursor);
  const bool camera_keyboard_active =
      !parInput.wants_capture_keyboard &&
      (viewport_active || m_right_look_active ||
       m_placement_controller.isActive() || m_gizmo_controller.isActive());
  camera::CameraSystem& camera_system = m_engine.getCameraSystem();
  camera_system.setMovement(camera::CameraMovement::Forward,
                            camera_keyboard_active && parInput.key_w_down);
  camera_system.setMovement(camera::CameraMovement::Backward,
                            camera_keyboard_active && parInput.key_s_down);
  camera_system.setMovement(camera::CameraMovement::Left,
                            camera_keyboard_active && parInput.key_a_down);
  camera_system.setMovement(camera::CameraMovement::Right,
                            camera_keyboard_active && parInput.key_d_down);
  camera_system.setMovement(camera::CameraMovement::Up,
                            camera_keyboard_active && parInput.key_space_down);
  camera_system.setMovement(camera::CameraMovement::Down,
                            camera_keyboard_active && parInput.key_shift_down);
}

void WorldEditor::handlePointerInput(
    const input::EditorInputSnapshot& parInput) {
  const bool viewport_active =
      parInput.viewport_hovered && !m_ui.isCursorOverPanel(parInput.ui_cursor);
  if (!parInput.right_mouse_down) {
    m_right_look_active = false;
  }
  if (parInput.right_mouse_down && viewport_active && !m_right_look_active) {
    m_right_look_active = true;
    m_gizmo_controller.end();
  }

  const glm::vec2 ui_viewport_size =
      glm::max(parInput.framebuffer_size / parInput.ui_to_framebuffer_scale,
               glm::vec2(1.0f));
  if (m_right_look_active && parInput.right_mouse_down) {
    m_engine.getCameraSystem().handleMouseMove(
        parInput.ui_delta, false, true, false, ui_viewport_size);
  }

  if (parInput.scroll_y != 0.0f && viewport_active) {
    m_engine.getCameraSystem().handleScroll(parInput.scroll_y);
  }

  if (m_placement_controller.isActive()) {
    if (viewport_active) {
      m_placement_controller.update(m_engine, parInput.framebuffer_cursor,
                                    parInput.framebuffer_size,
                                    parInput.left_mouse_down);
      if (parInput.left_mouse_pressed &&
          m_placement_controller.canCommit()) {
        m_placement_controller.commit(m_engine);
      }
    }
    return;
  }

  if (m_gizmo_controller.isActive()) {
    m_gizmo_controller.update(m_engine,
                              parInput.ui_delta *
                                  parInput.ui_to_framebuffer_scale,
                              parInput.framebuffer_cursor,
                              parInput.framebuffer_size,
                              parInput.left_mouse_down);
    return;
  }

  if (!viewport_active || !parInput.left_mouse_pressed ||
      parInput.right_mouse_down) {
    return;
  }

  if (m_gizmo_controller.begin(m_engine, parInput.framebuffer_cursor,
                               parInput.framebuffer_size)) {
    return;
  }

  const bool selected = m_selection_controller.handleViewportLeftPress(
      m_engine, parInput.framebuffer_cursor, parInput.framebuffer_size);
  if (!selected &&
      !m_engine.pickEntity(parInput.framebuffer_cursor, parInput.framebuffer_size)
           .has_value()) {
    m_engine.clearSelection();
  }
}

}  // namespace kage::editor
