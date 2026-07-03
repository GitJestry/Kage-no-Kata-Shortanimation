#include "editor/confirmation_dialog.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

#include <utility>

namespace kage::editor {

void ConfirmationDialog::request(Request parRequest) {
  m_request = std::move(parRequest);
  m_open_requested = true;
}

void ConfirmationDialog::draw() {
  if (!m_request.has_value()) {
    return;
  }

  if (m_open_requested) {
    ImGui::OpenPopup(m_request->title.c_str());
    m_open_requested = false;
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(m_request->title.c_str(), nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", m_request->message.c_str());
    ImGui::Spacing();

    const bool destructive = m_request->destructive;
    if (destructive) {
      pushDestructiveButtonStyle();
    }
    if (ImGui::Button(m_request->confirm_text.c_str(), ImVec2(110.0f, 0.0f))) {
      if (m_request->on_confirm) {
        m_request->on_confirm();
      }
      m_request.reset();
      ImGui::CloseCurrentPopup();
    }
    if (destructive) {
      popDestructiveButtonStyle();
    }
    if (m_request.has_value()) {
      ImGui::SameLine();
      if (ImGui::Button(m_request->cancel_text.c_str(),
                        ImVec2(110.0f, 0.0f))) {
        if (m_request->on_cancel) {
          m_request->on_cancel();
        }
        m_request.reset();
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

}

bool ConfirmationDialog::isOpen() const {
  return m_request.has_value();
}

void ConfirmationDialog::cancel() {
  m_request.reset();
  m_open_requested = false;
}

}  // namespace kage::editor
