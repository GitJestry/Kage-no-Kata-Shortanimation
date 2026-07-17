#include "assets/asset_streamer.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace kage::assets {

AssetStreamer::AssetStreamer(std::size_t parWorkerCount) {
  m_workers.reserve(std::max<std::size_t>(parWorkerCount, 1));
  for (std::size_t index = 0; index < std::max<std::size_t>(parWorkerCount, 1); ++index) {
    m_workers.emplace_back([this] { worker(); });
  }
}

AssetStreamer::~AssetStreamer() {
  {
    std::lock_guard lock(m_mutex);
    m_stopping = true;
  }
  m_condition.notify_all();
  for (std::thread& worker_thread : m_workers) {
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  }
}

void AssetStreamer::request(std::size_t parAssetIndex, std::filesystem::path parPath,
                            AssetLoadPriority parPriority) {
  std::lock_guard lock(m_mutex);
  const auto existing =
      std::find_if(m_requests.begin(), m_requests.end(),
                   [&](const Request& request) { return request.asset_index == parAssetIndex; });
  if (existing != m_requests.end()) {
    existing->priority = std::min(existing->priority, parPriority);
    return;
  }
  const std::uint64_t generation = ++m_generations[parAssetIndex];
  m_requests.push_back(
      {parAssetIndex, std::move(parPath), parPriority, m_next_sequence++, generation});
  m_condition.notify_one();
}

bool AssetStreamer::cancel(std::size_t parAssetIndex) {
  std::lock_guard lock(m_mutex);
  const std::size_t queued_before = m_requests.size();
  std::erase_if(m_requests,
                [&](const Request& request) { return request.asset_index == parAssetIndex; });
  const std::size_t results_before = m_results.size();
  std::erase_if(m_results,
                [&](const Result& result) { return result.asset_index == parAssetIndex; });
  const bool active = std::find(m_active_requests.begin(), m_active_requests.end(),
                                parAssetIndex) != m_active_requests.end();
  ++m_generations[parAssetIndex];
  return active || queued_before != m_requests.size() || results_before != m_results.size();
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

void AssetStreamer::worker() {
  while (true) {
    Request request;
    {
      std::unique_lock lock(m_mutex);
      m_condition.wait(lock, [this] { return m_stopping || !m_requests.empty(); });
      if (m_stopping) {
        return;
      }
      const auto next = std::min_element(m_requests.begin(), m_requests.end(),
                                         [](const Request& left, const Request& right) {
                                           if (left.priority != right.priority) {
                                             return left.priority < right.priority;
                                           }
                                           return left.sequence < right.sequence;
                                         });
      request = std::move(*next);
      m_requests.erase(next);
      m_active_requests.push_back(request.asset_index);
    }

    Result result;
    result.asset_index = request.asset_index;
    const auto begin = std::chrono::steady_clock::now();
    try {
      GltfAssetLoader loader;
      result.document = loader.loadDocument(request.path);
    } catch (const std::exception& error) {
      result.error = error.what();
    }
    result.cpu_ms =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
    {
      std::lock_guard lock(m_mutex);
      std::erase(m_active_requests, request.asset_index);
      if (m_generations[request.asset_index] == request.generation) {
        m_results.push_back(std::move(result));
      }
    }
  }
}

} // namespace kage::assets
