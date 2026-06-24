#pragma once

#include "platform/runtime_paths.hpp"

#include <framework/app.hpp>

namespace kage::app {

class MainApp final : public App {
 public:
  MainApp();

 protected:
  void render() override;
  void buildImGui() override;
  void keyCallback(Key parKey, Action parAction,
                   Modifier parModifier) override;

 private:
  platform::RuntimePaths m_runtime_paths;
};

}  // namespace kage::app
