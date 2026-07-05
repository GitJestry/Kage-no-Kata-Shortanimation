#include "editor/timeline_panel.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace {

constexpr float LEFT_PANEL_WIDTH = 335.0f;
constexpr float TIMELINE_WIDTH = 520.0f;
constexpr float TIMELINE_HEIGHT = 230.0f;

void clampCurrentPanel(const char* parPanelName) {
  const kage::editor::UiWorkArea area = kage::editor::getUiWorkArea();
  const ImVec2 size =
      kage::editor::clampPanelSize(area, ImGui::GetWindowSize(), true);
  if (size.x != ImGui::GetWindowSize().x ||
      size.y != ImGui::GetWindowSize().y) {
    ImGui::SetWindowSize(parPanelName, size, ImGuiCond_Always);
  }

  const ImVec2 position = kage::editor::clampPanelPosition(
      area, ImGui::GetWindowPos(), ImGui::GetWindowSize(), true);
  if (position.x != ImGui::GetWindowPos().x ||
      position.y != ImGui::GetWindowPos().y) {
    ImGui::SetWindowPos(parPanelName, position, ImGuiCond_Always);
  }
}

}  // namespace

namespace kage::editor {

std::optional<UiPanelRect> drawTimelinePanel(engine::EngineCore& parEngine,
                                             bool& parVisible,
                                             const glm::vec2& parViewportSize,
                                             FileBrowserDialog& parImportBrowser,
                                             std::array<char, 128>& parImportLabelBuffer,
                                             std::string& parImportError) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size(TIMELINE_WIDTH, TIMELINE_HEIGHT);
  const ImVec2 position = clampPanelPosition(
      area,
      ImVec2(area.position.x + LEFT_PANEL_WIDTH + 24.0f,
             area.position.y + area.size.y - TIMELINE_HEIGHT -
                 UI_STATUS_HEIGHT - 18.0f),
      size, true);
  ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
  const bool timeline_open =
      ImGui::Begin("Timeline", &parVisible, ImGuiWindowFlags_NoCollapse);
  clampCurrentPanel("Timeline");
  const UiPanelRect panel_rect = getCurrentPanelRect();
  if (!timeline_open) {
    ImGui::End();
    return panel_rect;
  }

  scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parEngine.getSelectedEntity());
  if (entity == nullptr || !entity->static_mesh.has_value()) {
    ImGui::TextDisabled("Select a rigged mesh entity to edit animation tracks");
    ImGui::End();
    return panel_rect;
  }

  const assets::AssetRegistry::AssetLibraryEntry* asset =
      parEngine.getAssetLibraryEntry(entity->static_mesh->asset_library_index);
  if (asset == nullptr) {
    ImGui::TextDisabled("Selected mesh has no asset library entry");
    ImGui::End();
    return panel_rect;
  }

  ImGui::Text("Track: %s", entity->name.name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("%s", assets::getAssetLoadStateLabel(asset->load_state));
  if (!asset->load_error.empty()) {
    ImGui::TextWrapped("Load error: %s", asset->load_error.c_str());
  }
  if (!asset->document.has_value()) {
    ImGui::TextDisabled("Asset is loading or still metadata-only");
    if (ImGui::Button("Request Load")) {
      parEngine.requestAssetLoad(entity->static_mesh->asset_library_index);
    }
    ImGui::End();
    return panel_rect;
  }

  const assets::ModelAsset& document = *asset->document;
  if (document.skins.empty() || !entity->rig.has_value()) {
    ImGui::TextDisabled("This asset has no rig track");
    ImGui::End();
    return panel_rect;
  }

  const std::size_t clip_count = document.animation_clips.size();
  ImGui::Text("Rig: skins %zu, joints %zu, clips %zu", document.skins.size(),
              document.stats.joint_count, clip_count);
  ImGui::SetNextItemWidth(220.0f);
  ImGui::InputText("Import label##AnimationImportLabel",
                   parImportLabelBuffer.data(), parImportLabelBuffer.size());
  ImGui::SameLine();
  if (ImGui::Button("Import Animation...")) {
    parImportBrowser.open("Import Animation", std::filesystem::current_path() /
                                                  "assets");
  }
  if (!parImportError.empty()) {
    ImGui::TextWrapped("Import error: %s", parImportError.c_str());
  }
  if (clip_count == 0) {
    ImGui::TextDisabled("Bind pose only");
    if (entity->animation_player.has_value() &&
        ImGui::Button("Clear Track")) {
      parEngine.clearAnimationPlayer(entity->id);
    }
    ImGui::End();
    return panel_rect;
  }

  scene::AnimationPlayerComponent player =
      entity->animation_player.value_or(scene::AnimationPlayerComponent{});
  player.clip_index = std::min(player.clip_index, clip_count - 1);
  player.blend_clip_index = std::min(player.blend_clip_index, clip_count - 1);
  bool changed = false;
  const auto get_clip_name = [&](std::size_t parIndex) {
    const std::string& name = document.animation_clips[parIndex].name;
    return name.empty() ? "Unnamed clip" : name.c_str();
  };
  const auto get_max_key_count = [](const assets::AnimationClip& parClip) {
    std::size_t key_count = 0;
    for (const assets::AnimationSampler& sampler : parClip.samplers) {
      key_count = std::max(key_count, sampler.input_times.size());
    }
    return key_count;
  };

  if (ImGui::BeginCombo("Clip", get_clip_name(player.clip_index))) {
    for (std::size_t clip_index = 0; clip_index < clip_count; ++clip_index) {
      const bool selected = clip_index == player.clip_index;
      if (ImGui::Selectable(get_clip_name(clip_index), selected)) {
        player.clip_index = clip_index;
        player.time_seconds = 0.0f;
        player.blend_duration_seconds = 0.0f;
        player.blend_time_seconds = 0.0f;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  const assets::AnimationClip& selected_clip =
      document.animation_clips[player.clip_index];
  const std::size_t key_count = get_max_key_count(selected_clip);
  const float clip_duration =
      std::max(selected_clip.duration_seconds, 0.001f);
  ImGui::TextDisabled("Clip length %.3fs, max keys %zu", clip_duration,
                      key_count);
  if (key_count <= 2 || selected_clip.duration_seconds < 0.25f) {
    ImGui::TextDisabled("Short pose clip; export more keys for final motion");
  }

  if (player.playing) {
    if (ImGui::Button("Stop")) {
      player.playing = false;
      player.time_seconds = 0.0f;
      player.blend_time_seconds = 0.0f;
      changed = true;
    }
  } else if (ImGui::Button("Play")) {
    player.playing = true;
    player.looping = true;
    if (player.time_seconds >= clip_duration) {
      player.time_seconds = 0.0f;
    }
    changed = true;
  }
  player.looping = true;
  ImGui::SameLine();
  ImGui::TextDisabled("Looping");

  float normalized_time = std::fmod(std::max(player.time_seconds, 0.0f),
                                    clip_duration);
  if (ImGui::SliderFloat("Progress", &normalized_time, 0.0f, clip_duration,
                         "%.3fs")) {
    player.time_seconds = normalized_time;
    changed = true;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("/ %.3fs", clip_duration);
  changed |= ImGui::SliderFloat("Speed", &player.playback_speed, 0.0f, 4.0f,
                                "%.2fx");

  if (clip_count > 1 && ImGui::CollapsingHeader("Blend")) {
    if (ImGui::BeginCombo("Blend target",
                          get_clip_name(player.blend_clip_index))) {
      for (std::size_t clip_index = 0; clip_index < clip_count; ++clip_index) {
        const bool selected = clip_index == player.blend_clip_index;
        if (ImGui::Selectable(get_clip_name(clip_index), selected)) {
          player.blend_clip_index = clip_index;
          changed = true;
        }
      }
      ImGui::EndCombo();
    }
    changed |= ImGui::DragFloat("Blend duration",
                                &player.blend_duration_seconds, 0.02f,
                                0.0f, 5.0f);
    if (ImGui::Button("Restart Blend")) {
      player.blend_time_seconds = 0.0f;
      if (player.blend_duration_seconds <= 0.0f) {
        player.blend_duration_seconds = 0.35f;
      }
      changed = true;
    }
  }

  if (ImGui::Button("Bind Pose")) {
    parEngine.clearAnimationPlayer(entity->id);
  }
  if (changed) {
    parEngine.setAnimationPlayer(entity->id, player);
  }

  ImGui::End();
  return panel_rect;
}

}  // namespace kage::editor
