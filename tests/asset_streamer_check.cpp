#include "assets/asset_streamer.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

bool waitFor(const auto& parPredicate,
             std::chrono::seconds parTimeout = std::chrono::seconds(120)) {
  const auto deadline = std::chrono::steady_clock::now() + parTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (parPredicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

}  // namespace

int main(int parArgumentCount, char** parArguments) {
  assert(parArgumentCount == 4);
  const std::filesystem::path slow_asset = parArguments[1];
  const std::filesystem::path background_asset = parArguments[2];
  const std::filesystem::path selected_asset = parArguments[3];

  {
    kage::assets::AssetStreamer streamer(1);
    streamer.request(0, slow_asset, kage::assets::AssetLoadPriority::Visible);
    assert(waitFor([&] { return streamer.getActiveCount() == 1; }));
    streamer.request(1, background_asset,
                     kage::assets::AssetLoadPriority::Background);
    streamer.request(2, selected_asset,
                     kage::assets::AssetLoadPriority::Selected);
    assert(streamer.cancel(1));

    std::vector<std::size_t> completed;
    assert(waitFor([&] {
      while (auto result = streamer.poll()) {
        assert(result->document.has_value());
        completed.push_back(result->asset_index);
      }
      return streamer.getPendingCount() == 0;
    }));
    assert((completed == std::vector<std::size_t>{0, 2}));
  }

  {
    kage::assets::AssetStreamer streamer(2);
    for (std::size_t index = 0; index < 3; ++index) {
      streamer.request(index, background_asset,
                       kage::assets::AssetLoadPriority::Visible);
    }
    std::size_t maximum_active = 0;
    assert(waitFor([&] {
      maximum_active = std::max(maximum_active, streamer.getActiveCount());
      while (streamer.poll()) {
      }
      return streamer.getPendingCount() == 0;
    }));
    assert(maximum_active == 2);
  }
  return 0;
}
