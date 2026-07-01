#pragma once

#include "assets/asset_types.hpp"

#include <string>
#include <vector>

namespace kage::assets {

struct RigValidationReport final {
  std::vector<std::string> errors;

  [[nodiscard]] bool passed() const {
    return errors.empty();
  }
};

[[nodiscard]] RigValidationReport validateRiggedModelAsset(
    const ModelAsset& parAsset);

}  // namespace kage::assets
