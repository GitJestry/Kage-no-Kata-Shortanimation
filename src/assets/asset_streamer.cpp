#include "assets/asset_streamer.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace kage::assets {

AssetStreamer::AssetStreamer(std::size_t parWorkerCount) {
  m_workers.reserve(std::max<std::size_t>(parWorkerCount, 1));
  for (std::size_t index = 0; index < std::max<std::size_t>(parWorkerCount, 1);
       ++index) {
    m_workers.emplace_back(
        [this](std::stop_token stop_token) { worker(stop_token); });
  }
}

AssetStreamer::~AssetStreamer() {
  for (std::jthread& worker_thread : m_workers) {
    worker_thread.request_stop();
  }
  m_condition.notify_all();
  // Join before the request/result queues begin member destruction.
  m_workers.clear();
}

void AssetStreamer::request(std::size_t parAssetIndex,
                            std::filesystem::path parPath,
                            AssetLoadPriority parPriority,
                            AssetQualityTier parQuality) {
  std::lock_guard lock(m_mutex);
  const auto existing = std::find_if(
      m_requests.begin(), m_requests.end(), [&](const Request& request) {
        return request.asset_index == parAssetIndex &&
               request.quality == parQuality;
      });
  if (existing != m_requests.end()) {
    existing->priority = std::min(existing->priority, parPriority);
    return;
  }
  m_requests.push_back(
      {parAssetIndex, std::move(parPath), parPriority, m_next_sequence++,
       parQuality});
  m_condition.notify_one();
}

bool AssetStreamer::cancel(std::size_t parAssetIndex,
                           AssetQualityTier parQuality) {
  const RequestKey key{parAssetIndex, parQuality};
  std::lock_guard lock(m_mutex);
  const std::size_t queued_before = m_requests.size();
  std::erase_if(m_requests, [&](const Request& request) {
    return request.asset_index == key.asset_index &&
           request.quality == key.quality;
  });
  const std::size_t results_before = m_results.size();
  std::erase_if(m_results, [&](const Result& result) {
    return result.asset_index == key.asset_index &&
           result.quality == key.quality;
  });
  const bool active = std::find(m_active_requests.begin(),
                                m_active_requests.end(), key) !=
                      m_active_requests.end();
  if (active && std::find(m_cancelled_requests.begin(),
                          m_cancelled_requests.end(), key) ==
                    m_cancelled_requests.end()) {
    m_cancelled_requests.push_back(key);
  }
  return active || queued_before != m_requests.size() ||
         results_before != m_results.size();
}

std::optional<AssetStreamer::Result> AssetStreamer::poll() {
  std::lock_guard lock(m_mutex);
  if (m_results.empty()) {
    return std::nullopt;
  }
  Result result = std::move(m_results.front());
  m_results.pop_front();
  return result;
}

std::size_t AssetStreamer::getPendingCount() const {
  std::lock_guard lock(m_mutex);
  return m_requests.size() + m_results.size() + m_active_requests.size();
}

std::size_t AssetStreamer::getActiveCount() const {
  std::lock_guard lock(m_mutex);
  return m_active_requests.size();
}

void AssetStreamer::worker(std::stop_token parStopToken) {
  while (!parStopToken.stop_requested()) {
    Request request;
    {
      std::unique_lock lock(m_mutex);
      m_condition.wait(lock, parStopToken,
                       [this] { return !m_requests.empty(); });
      if (parStopToken.stop_requested()) {
        return;
      }
      const auto next = std::min_element(
          m_requests.begin(), m_requests.end(),
          [](const Request& left, const Request& right) {
            if (left.priority != right.priority) {
              return left.priority < right.priority;
            }
            return left.sequence < right.sequence;
          });
      request = std::move(*next);
      m_requests.erase(next);
      m_active_requests.push_back(
          {request.asset_index, request.quality});
    }

    Result result;
    result.asset_index = request.asset_index;
    result.quality = request.quality;
    const auto begin = std::chrono::steady_clock::now();
    try {
      GltfAssetLoader loader;
      result.document = loader.loadDocument(request.path);
    } catch (const std::exception& error) {
      result.error = error.what();
    }
    result.cpu_ms = std::chrono::duration<float, std::milli>(
                        std::chrono::steady_clock::now() - begin)
                        .count();
    {
      std::lock_guard lock(m_mutex);
      const RequestKey key{request.asset_index, request.quality};
      std::erase(m_active_requests, key);
      const auto cancelled = std::find(m_cancelled_requests.begin(),
                                       m_cancelled_requests.end(), key);
      if (cancelled != m_cancelled_requests.end()) {
        m_cancelled_requests.erase(cancelled);
      } else {
        m_results.push_back(std::move(result));
      }
    }
  }
}

}  // namespace kage::assets
