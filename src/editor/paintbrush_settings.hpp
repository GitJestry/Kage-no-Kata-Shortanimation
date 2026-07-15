#pragma once

namespace kage::editor {

struct PaintbrushSettings final {
  int brush_size = 4;
  int paint_density = 3;
  bool randomize_scale = false;
  bool randomize_rotation = false;
  double brush_spacing = 0.5;  // Spacing between paintbrush stamps, in world units
};

}  // namespace kage::editor
