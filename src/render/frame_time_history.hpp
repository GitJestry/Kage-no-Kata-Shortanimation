#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>

namespace kage::render {

struct FrameTimeStats final {
  float average_ms = 0.0f;
  float p95_ms = 0.0f;
};

class FrameTimeHistory final {
 public:
  [[nodiscard]] FrameTimeStats record(float parMilliseconds) {
    m_samples[m_cursor] = parMilliseconds;
    m_cursor = (m_cursor + 1) % m_samples.size();
    m_count = std::min(m_count + 1, m_samples.size());

    std::array<float, 120> sorted = m_samples;
    std::sort(sorted.begin(), sorted.begin() + m_count);
    const float total =
        std::accumulate(sorted.begin(), sorted.begin() + m_count, 0.0f);
    const std::size_t p95_index =
        std::min(m_count - 1, (m_count * 95 + 99) / 100 - 1);
    return {total / static_cast<float>(m_count), sorted[p95_index]};
  }

 private:
  std::array<float, 120> m_samples{};
  std::size_t m_count = 0;
  std::size_t m_cursor = 0;
};

}  // namespace kage::render
