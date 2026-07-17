#pragma once

#include "assets/asset_types.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace kage::assets {

enum class AssetLoadPriority : int {
  Selected = 0,
  Visible = 1,
  Background = 2
};

class AssetStreamer final {
 public:
  struct Result final {
    std::size_t asset_index = 0;
    std::optional<ModelAsset> document;
    std::string error;
    float cpu_ms = 0.0f;
  };

  explicit AssetStreamer(std::size_t parWorkerCount = 2);
  ~AssetStreamer();

  AssetStreamer(const AssetStreamer&) = delete;
  AssetStreamer& operator=(const AssetStreamer&) = delete;

  void request(std::size_t parAssetIndex, std::filesystem::path parPath,
               AssetLoadPriority parPriority);
  bool cancel(std::size_t parAssetIndex);
  [[nodiscard]] std::optional<Result> poll();
  [[nodiscard]] std::size_t getPendingCount() const;

 private:
  struct Request final {
    std::size_t asset_index = 0;
    std::filesystem::path path;
    AssetLoadPriority priority = AssetLoadPriority::Visible;
    std::uint64_t sequence = 0;
    std::uint64_t generation = 0;
  };

  void worker();

  mutable std::mutex m_mutex;
  std::condition_variable m_condition;
  std::vector<std::thread> m_workers;
  std::vector<Request> m_requests;
  std::deque<Result> m_results;
  std::uint64_t m_next_sequence = 0;
  std::vector<std::size_t> m_active_requests;
  std::unordered_map<std::size_t, std::uint64_t> m_generations;
  bool m_stopping = false;
};

}  // namespace kage::assets
