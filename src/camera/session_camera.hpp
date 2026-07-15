#pragma once

#include "math/transform.hpp"

namespace kage::camera {

[[nodiscard]] bool isValidSessionCamera(const math::Transform& parTransform,
                                        float parFovDegrees,
                                        float parNearPlane,
                                        float parFarPlane);

}  // namespace kage::camera
