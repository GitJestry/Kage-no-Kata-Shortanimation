#include "lighting/light_math.hpp"

#include <glm/glm.hpp>

#include <iostream>

int main() {
  const glm::vec3 direction_to_sun = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
  const float sun_front =
      kage::lighting::lambertResponse(glm::vec3(0.0f, 1.0f, 0.0f),
                                      direction_to_sun);
  const float sun_back =
      kage::lighting::lambertResponse(glm::vec3(0.0f, -1.0f, 0.0f),
                                      direction_to_sun);
  if (sun_front <= sun_back) {
    std::cerr << "sun direction_to_light convention is inverted\n";
    return 1;
  }

  const glm::vec3 surface(0.0f);
  const glm::vec3 point_light(2.0f, 0.0f, 0.0f);
  const glm::vec3 direction_to_point =
      kage::lighting::directionToPointLight(surface, point_light);
  const float point_front =
      kage::lighting::lambertResponse(glm::vec3(1.0f, 0.0f, 0.0f),
                                      direction_to_point);
  const float point_back =
      kage::lighting::lambertResponse(glm::vec3(-1.0f, 0.0f, 0.0f),
                                      direction_to_point);
  if (point_front <= point_back) {
    std::cerr << "point light direction_to_light convention is inverted\n";
    return 1;
  }

  return 0;
}
