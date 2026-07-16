#pragma once

#include "math/transform.hpp"

#include <glm/gtc/quaternion.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <cmath>

namespace kage::serialization {

using Json = nlohmann::json;

[[nodiscard]] inline glm::vec3 readVec3(const Json& parJson,
                                         const glm::vec3& parFallback = {}) {
  if (!parJson.is_array() || parJson.size() != 3) {
    return parFallback;
  }
  return {parJson[0].get<float>(), parJson[1].get<float>(),
          parJson[2].get<float>()};
}

[[nodiscard]] inline math::Transform readTransform(const Json& parJson) {
  math::Transform value;
  if (!parJson.is_object()) {
    return value;
  }
  value.translation =
      readVec3(parJson.value("position", Json::array()), value.translation);
  const Json& rotation_json = parJson.value("rotation", Json::array());
  if (rotation_json.is_array() && rotation_json.size() == 4) {
    const glm::quat rotation(
        rotation_json[0].get<float>(), rotation_json[1].get<float>(),
        rotation_json[2].get<float>(), rotation_json[3].get<float>());
    const float length = glm::length(rotation);
    value.rotation =
        std::isfinite(length) && length > 0.00001f ? glm::normalize(rotation)
                                                   : value.rotation;
  }
  value.scale = readVec3(parJson.value("scale", Json::array()), value.scale);
  return value;
}

[[nodiscard]] inline Json writeVec3(const glm::vec3& parValue) {
  return Json::array({parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] inline Json writeTransform(const math::Transform& parValue) {
  return {{"position", writeVec3(parValue.translation)},
          {"rotation",
           Json::array({parValue.rotation.w, parValue.rotation.x,
                        parValue.rotation.y, parValue.rotation.z})},
          {"scale", writeVec3(parValue.scale)}};
}

}  // namespace kage::serialization
