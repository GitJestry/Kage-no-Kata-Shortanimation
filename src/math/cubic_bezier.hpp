#pragma once

namespace kage::math {

template <typename Value>
[[nodiscard]] Value cubicBezier(const Value& parStart, const Value& parControl1,
                                const Value& parControl2, const Value& parEnd,
                                float parT) {
  const float inverse = 1.0f - parT;
  return inverse * inverse * inverse * parStart +
         3.0f * inverse * inverse * parT * parControl1 +
         3.0f * inverse * parT * parT * parControl2 +
         parT * parT * parT * parEnd;
}

}  // namespace kage::math
