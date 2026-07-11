#include "editor/world_editor.hpp"

#include "assets/asset_path.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace kage::editor {

WorldEditor::WorldEditor(engine::EngineCore& parEngine)
    : m_engine(parEngine) {
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
  if (parInput.key_z_pressed && !parInput.wants_capture_keyboard) {
    const int current = static_cast<int>(m_engine.getViewportMode());
    m_engine.setViewportMode(
        static_cast<render::ViewportMode>((current + 1) % 4));
  }
  if (parInput.key_delete_pressed && !parInput.wants_capture_keyboard &&
      !parInput.wants_capture_mouse) {
    if (!cancelActiveOperation()) {
      const scene::EntityRecord* entity =
          m_engine.getWorld().findEntity(m_engine.getSelectedEntity());
      if (entity != nullptr) {
        const scene::EntityId entity_id = entity->id;
        ConfirmationDialog::Request request;
        request.title = "Delete Entity";
        request.message = "Delete \"" + entity->name.name + "\" (id " +
                          std::to_string(entity_id.value) +
                          ")? This cannot be undone.";
        request.confirm_text = "Delete";
        request.cancel_text = "Cancel";
        request.destructive = true;
        request.on_confirm = [this, entity_id]() {
          m_engine.deleteEntity(entity_id);
        };
        m_confirmation_dialog.request(std::move(request));
      }
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
            m_gizmo_controller, m_confirmation_dialog, parViewportSize, parDeltaSeconds,
            parFrameCount);
  m_confirmation_dialog.draw();
}

bool WorldEditor::cancelActiveOperation() {
  bool cancelled = false;
  cancelled |= m_placement_controller.cancel(m_engine);
  if (m_confirmation_dialog.isOpen()) {
    m_confirmation_dialog.cancel();
    cancelled = true;
  }
  if (m_gizmo_controller.isActive()) {
    m_gizmo_controller.end();
    m_engine.clearGizmoGuide();
    cancelled = true;
  }
  return cancelled;
}

void WorldEditor::registerDefaultAssets() {
  const platform::RuntimePaths& paths = m_engine.getRuntimePaths();
  m_engine.loadProjectAssetCatalog(paths.getProjectAssetCatalogPath());
  if (!m_engine.getAssetLibrary().empty()) {
    return;
  }

  std::error_code error_code;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(paths.getModelDirectory(),
                                           error_code)) {
    if (error_code) {
      return;
    }
    if (!entry.is_regular_file(error_code) || error_code ||
        !assets::hasGltfExtension(entry.path())) {
      error_code.clear();
      continue;
    }
    const std::string label = entry.path().stem().string();
    m_engine.registerStaticAsset(label, entry.path());
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
  const bool cursor_over_panel = m_ui.isCursorOverPanel(parInput.ui_cursor);
  const bool ui_blocks_left =
      cursor_over_panel || parInput.wants_capture_mouse ||
      parInput.ui_item_active || parInput.ui_popup_open;
  const bool viewport_active = parInput.viewport_hovered && !ui_blocks_left;
  const bool right_look_start_area =
      parInput.viewport_hovered && !cursor_over_panel &&
      !parInput.ui_item_active && !parInput.ui_popup_open;
  if (!parInput.right_mouse_down) {
    m_right_look_active = false;
  }
  if (parInput.right_mouse_down && right_look_start_area &&
      !m_right_look_active) {
    m_right_look_active = true;
    m_gizmo_controller.end();
    m_engine.clearGizmoGuide();
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
    publishGizmoGuide();
    return;
  }

  if (!viewport_active || !parInput.left_mouse_pressed ||
      parInput.right_mouse_down) {
    return;
  }

  if (m_gizmo_controller.begin(m_engine, parInput.framebuffer_cursor,
                               parInput.framebuffer_size)) {
    publishGizmoGuide();
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

void WorldEditor::publishGizmoGuide() {
  const std::optional<render::GizmoGuide> guide =
      m_gizmo_controller.getActiveGuide(m_engine);
  if (guide.has_value()) {
    m_engine.setGizmoGuide(*guide);
  } else {
    m_engine.clearGizmoGuide();
  }
}

}  // namespace kage::editor
