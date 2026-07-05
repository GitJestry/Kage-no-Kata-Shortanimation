#pragma once

#include <filesystem>

namespace kage::engine {

class EngineCore;

class ProjectSerializer final {
 public:
  [[nodiscard]] static bool loadProject(EngineCore& parEngine);
  static void saveProject(EngineCore& parEngine);

  [[nodiscard]] static bool loadLocalSession(EngineCore& parEngine);
  static void saveLocalSession(const EngineCore& parEngine);

};

}  // namespace kage::engine
