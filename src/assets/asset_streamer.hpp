#pragma once

#include "assets/asset_types.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
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
    AssetQualityTier quality = AssetQualityTier::Proxy;
  };

  explicit AssetStreamer(std::size_t parWorkerCount = 2);
  ~AssetStreamer();

  AssetStreamer(const AssetStreamer&) = delete;
  AssetStreamer& operator=(const AssetStreamer&) = delete;

  void request(std::size_t parAssetIndex, std::filesystem::path parPath,
               AssetLoadPriority parPriority,
               AssetQualityTier parQuality = AssetQualityTier::Proxy);
  bool cancel(std::size_t parAssetIndex,
              AssetQualityTier parQuality = AssetQualityTier::Proxy);
  [[nodiscard]] std::optional<Result> poll();
  [[nodiscard]] std::size_t getPendingCount() const;
  [[nodiscard]] std::size_t getActiveCount() const;

 private:
  struct Request final {
    std::size_t asset_index = 0;
    std::filesystem::path path;
    AssetLoadPriority priority = AssetLoadPriority::Visible;
    std::uint64_t sequence = 0;
    AssetQualityTier quality = AssetQualityTier::Proxy;
  };

  struct RequestKey final {
    std::size_t asset_index = 0;
    AssetQualityTier quality = AssetQualityTier::Proxy;
    friend bool operator==(const RequestKey&, const RequestKey&) = default;
  };

  void worker(std::stop_token parStopToken);

  mutable std::mutex m_mutex;
  std::condition_variable_any m_condition;
  std::vector<std::jthread> m_workers;
  std::vector<Request> m_requests;
  std::deque<Result> m_results;
  std::uint64_t m_next_sequence = 0;
  std::vector<RequestKey> m_active_requests;
  std::vector<RequestKey> m_cancelled_requests;
};

}  // namespace kage::assets
