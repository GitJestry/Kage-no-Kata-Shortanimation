#pragma once

#include <functional>
#include <optional>
#include <string>

namespace kage::editor {

class ConfirmationDialog final {
 public:
  struct Request final {
    std::string title;
    std::string message;
    std::string confirm_text = "Yes";
    std::string cancel_text = "No";
    bool destructive = false;
    std::function<void()> on_confirm;
    std::function<void()> on_cancel;
  };

  void request(Request parRequest);
  void draw();
  [[nodiscard]] bool isOpen() const;
  void cancel();

 private:
  std::optional<Request> m_request;
  bool m_open_requested = false;
};

}  // namespace kage::editor
