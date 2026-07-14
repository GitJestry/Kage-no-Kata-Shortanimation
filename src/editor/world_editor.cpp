#include "editor/world_editor.hpp"

#include "assets/asset_path.hpp"
#include "editor/ui_layout.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <utility>

namespace kage::editor {
namespace {

void paintBrushAssets(engine::EngineCore& parEngine,
                      const glm::vec3& parCenter, float parRadius,
                      int parDensity,
                      const std::vector<std::size_t>& parAssetIndices) {
  if (parAssetIndices.empty()) {
    return;
  }

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> offset_distribution(-parRadius, parRadius);
  std::uniform_real_distribution<float> rotation_distribution(0.0f, glm::two_pi<float>());
  std::uniform_int_distribution<std::size_t> asset_distribution(0, parAssetIndices.size() - 1);

  const int spawn_count = std::max(1, parDensity);
  for (int spawn_index = 0; spawn_index < spawn_count; ++spawn_index) {
    glm::vec3 offset(0.0f);
    // 1. Keep the Y offset at 0.0f so assets stay flat on the XZ grid floor
    for (int attempt = 0; attempt < 8; ++attempt) {
      offset = glm::vec3(offset_distribution(rng), 0.0f, offset_distribution(rng));
      if (glm::length(glm::vec2(offset.x, offset.z)) <= parRadius) {
        break;
      }
    }

    const glm::vec3 position = parCenter + offset;
    const float yaw_radians = rotation_distribution(rng);
    
    // 2. Rotate purely around the Y-axis (0.0, 1.0, 0.0) 
    // This spins the mesh flat on the ground floor.
    const glm::quat rotation = glm::angleAxis(yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f));
    
    const std::size_t asset_index = parAssetIndices[asset_distribution(rng)];
    const scene::EntityId entity = parEngine.instantiateAssetAt(asset_index, position);
    
    parEngine.setEntityTransform(
        entity, kage::math::Transform{position, rotation, glm::vec3(1.0f)});
  }
}

}  // namespace

WorldEditor::WorldEditor(engine::EngineCore& parEngine)
    : m_engine(parEngine) {
  registerDefaultAssets();
  if (!m_engine.loadProject()) {
    m_engine.createDefaultProject();
  }
  m_engine.loadLocalSession();
  loadEditorSession(m_engine.getLocalSessionSavePath(), m_session);
  m_gizmo_controller.setAxisSpace(
      m_engine.getGizmoAxisSpace() == render::GizmoAxisSpace::World
          ? GizmoController::AxisSpace::World
          : GizmoController::AxisSpace::Local);
}

WorldEditor::~WorldEditor() {
  m_engine.saveLocalSession();
  saveEditorSession(m_engine.getLocalSessionSavePath(), m_session);
}

void WorldEditor::update(float parDeltaSeconds,
                         const input::EditorInputSnapshot& parInput) {
  updateViewportRect(parInput);
  m_selection_controller.update(parDeltaSeconds);
  if (m_session.workspace == Workspace::Movie) {
    static_cast<void>(m_placement_controller.cancel(m_engine));
    m_gizmo_controller.end();
    m_engine.clearGizmoGuide();
  }
  if (m_session.solo_clip_preview) {
    const film::FilmClip* clip = m_engine.getFilmSequence().findClip(
        m_session.selected_film_clip);
    if (clip == nullptr ||
        m_engine.getFilmPlayback().playhead_frame >= clip->end_frame) {
      m_session.solo_clip_preview = false;
      m_engine.getFilmPlayback().playing = false;
      if (clip != nullptr) {
        m_engine.getFilmPlayback().playhead_frame = clip->start_frame;
      }
    }
  }
  m_engine.setGizmoAxisSpace(
      m_gizmo_controller.getAxisSpace() == GizmoController::AxisSpace::World
          ? render::GizmoAxisSpace::World
          : render::GizmoAxisSpace::Local);
  if (parInput.key_z_pressed && !parInput.wants_capture_keyboard) {
    const int current = static_cast<int>(m_engine.getViewportMode());
    m_engine.setViewportMode(
        static_cast<render::ViewportMode>((current + 1) % 4));
  }
  if (m_session.workspace == Workspace::WorldEdit &&
      parInput.key_delete_pressed && !parInput.wants_capture_keyboard &&
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
  m_engine.update(parDeltaSeconds, m_session.workspace == Workspace::Movie);
}

void WorldEditor::render(const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  m_engine.render(m_viewport,
                  m_session.workspace == Workspace::Movie,
                  m_session.shot_preview, -1.0,
                  !m_session.shot_preview, 0,
                  m_session.selected_film_clip,
                  m_session.solo_clip_preview
                      ? m_session.selected_film_clip
                      : 0);
  m_engine.advanceFilmExport();
}

void WorldEditor::buildImGui(const glm::vec2& parViewportSize,
                             float parDeltaSeconds,
                             unsigned int parFrameCount) {
  m_ui.draw(m_engine, m_session, m_placement_controller, m_selection_controller,
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
      m_viewport.contains(parInput.framebuffer_cursor) &&
      !m_ui.isCursorOverPanel(parInput.ui_cursor);
  camera::CameraSystem& camera_system = m_engine.getCameraSystem();
  const bool camera_keyboard_active =
      !parInput.wants_capture_keyboard &&
      !m_session.shot_preview &&
      (viewport_active || m_right_look_active ||
       m_placement_controller.isActive() || m_gizmo_controller.isActive());
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
  if (m_session.shot_preview) {
    m_right_look_active = false;
    m_gizmo_controller.end();
    m_engine.clearGizmoGuide();
    return;
  }
  const bool world_edit = m_session.workspace == Workspace::WorldEdit;
  const bool inside_viewport =
      m_viewport.contains(parInput.framebuffer_cursor);
  const glm::vec2 viewport_cursor =
      m_viewport.toLocal(parInput.framebuffer_cursor);
  const glm::vec2 viewport_size = m_viewport.extent();
  const bool cursor_over_panel = m_ui.isCursorOverPanel(parInput.ui_cursor);
  const bool ui_blocks_left =
      cursor_over_panel || parInput.wants_capture_mouse ||
      parInput.ui_item_active || parInput.ui_popup_open;
  const bool viewport_active = inside_viewport && !ui_blocks_left;
  const bool right_look_start_area =
      inside_viewport && !cursor_over_panel &&
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

  if (world_edit && m_ui.isPaintbrushEnabled() && viewport_active) {
    const glm::vec3 paintbrush_position =
        m_engine.getPlacementPointOnFloor(viewport_cursor, viewport_size);
    m_engine.setPaintbrushPreview(
        paintbrush_position, static_cast<float>(m_ui.getPaintbrushBrushSize()),
        m_ui.getPaintbrushPaintDensity());

    if (parInput.left_mouse_down && !parInput.right_mouse_down) {
      const std::vector<std::size_t> selected_assets =
          m_ui.getPaintbrushSelectedAssetIndices();
      paintBrushAssets(m_engine, paintbrush_position,
                       static_cast<float>(m_ui.getPaintbrushBrushSize()),
                       m_ui.getPaintbrushPaintDensity(), selected_assets);
    }
  } else {
    m_engine.clearPaintbrushPreview();
  }

  if (world_edit && m_placement_controller.isActive()) {
    if (viewport_active) {
      m_placement_controller.update(m_engine, viewport_cursor,
                                    viewport_size,
                                    parInput.left_mouse_down);
      if (parInput.left_mouse_pressed &&
          m_placement_controller.canCommit()) {
        m_placement_controller.commit(m_engine);
      }
    }
    return;
  }

  if (world_edit && m_gizmo_controller.isActive()) {
    m_gizmo_controller.update(m_engine,
                              parInput.ui_delta *
                                  parInput.ui_to_framebuffer_scale,
                              viewport_cursor, viewport_size,
                              parInput.left_mouse_down);
    publishGizmoGuide();
    return;
  }

  if (!viewport_active || !parInput.left_mouse_pressed ||
      parInput.right_mouse_down) {
    return;
  }

  if (world_edit &&
      m_gizmo_controller.begin(m_engine, viewport_cursor, viewport_size)) {
    publishGizmoGuide();
    return;
  }

  const bool selected = m_selection_controller.handleViewportLeftPress(
      m_engine, viewport_cursor, viewport_size);
  if (!selected) {
    m_engine.clearSelection();
  }
}

void WorldEditor::updateViewportRect(
    const input::EditorInputSnapshot& parInput) {
  const float bottom_ui = UI_STATUS_HEIGHT +
      (m_session.workspace == Workspace::Movie
           ? m_session.film_editor_height
           : 0.0f);
  const int reserved_bottom = std::max(
      0, static_cast<int>(std::lround(
             bottom_ui * parInput.ui_to_framebuffer_scale.y)));
  const int width = std::max(static_cast<int>(parInput.framebuffer_size.x), 1);
  const int full_height =
      std::max(static_cast<int>(parInput.framebuffer_size.y), 1);
  m_viewport = {{0, 0},
                {width, std::max(full_height - reserved_bottom, 1)},
                full_height};
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
