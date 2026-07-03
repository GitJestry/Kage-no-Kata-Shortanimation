#include "assets/asset_registry.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace kage::assets {

namespace {

[[nodiscard]] float elapsedMilliseconds(
    std::chrono::steady_clock::time_point parStart,
    std::chrono::steady_clock::time_point parEnd) {
  return std::chrono::duration<float, std::milli>(parEnd - parStart).count();
}

[[nodiscard]] std::string normalizedPathKey(const std::filesystem::path& parPath) {
  const std::filesystem::path normalized = parPath.lexically_normal();
  std::filesystem::path asset_relative;
  bool found_asset_root = false;
  for (const std::filesystem::path& part : normalized) {
    if (found_asset_root) {
      asset_relative /= part;
      continue;
    }
    if (part == "assets") {
      found_asset_root = true;
      asset_relative = part;
    }
  }
  return found_asset_root ? asset_relative.generic_string()
                          : normalized.generic_string();
}

[[nodiscard]] std::optional<std::uint32_t> findNodeByName(
    const ModelAsset& parAsset, std::string_view parName) {
  if (parName.empty()) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < parAsset.nodes.size(); ++index) {
    if (parAsset.nodes[index].name == parName) {
      return static_cast<std::uint32_t>(index);
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool hasCompatibleJointNames(const ModelAsset& parTarget,
                                           const ModelAsset& parSource,
                                           std::string& parError) {
  if (parTarget.skins.empty() || parSource.skins.empty()) {
    parError = "target and imported animation must both contain a skin";
    return false;
  }

  const GltfSkin& source_skin = parSource.skins.front();
  for (std::uint32_t source_joint : source_skin.joints) {
    if (source_joint >= parSource.nodes.size()) {
      parError = "imported animation references an invalid joint";
      return false;
    }
    const std::string& joint_name = parSource.nodes[source_joint].name;
    const std::optional<std::uint32_t> target_node =
        findNodeByName(parTarget, joint_name);
    if (!target_node.has_value()) {
      parError = "target rig has no joint named " + joint_name;
      return false;
    }

    const std::uint32_t source_parent =
        parSource.nodes[source_joint].parent_index;
    if (source_parent == INVALID_NODE_INDEX) {
      continue;
    }
    if (source_parent >= parSource.nodes.size()) {
      parError = "imported animation references an invalid joint parent";
      return false;
    }
    const std::string& source_parent_name =
        parSource.nodes[source_parent].name;
    const std::uint32_t target_parent =
        parTarget.nodes[*target_node].parent_index;
    if (target_parent != INVALID_NODE_INDEX &&
        target_parent < parTarget.nodes.size() &&
        parTarget.nodes[target_parent].name == source_parent_name) {
      continue;
    }

    const bool parent_is_part_of_source_skin =
        std::find(source_skin.joints.begin(), source_skin.joints.end(),
                  source_parent) != source_skin.joints.end();
    if (parent_is_part_of_source_skin) {
      parError = "joint hierarchy differs at " + joint_name;
      return false;
    }
  }

  return true;
}

[[nodiscard]] std::size_t appendRemappedAnimationClips(
    ModelAsset& parTarget, const ModelAsset& parSource,
    std::string_view parPackLabel, std::string& parError) {
  std::size_t appended_count = 0;
  for (const AnimationClip& source_clip : parSource.animation_clips) {
    AnimationClip clip = source_clip;
    clip.channels.clear();
    for (const AnimationChannel& source_channel : source_clip.channels) {
      if (source_channel.target_node >= parSource.nodes.size()) {
        parError = "imported animation channel targets an invalid node";
        return appended_count;
      }
      const std::string& source_node_name =
          parSource.nodes[source_channel.target_node].name;
      const std::optional<std::uint32_t> target_node =
          findNodeByName(parTarget, source_node_name);
      if (!target_node.has_value()) {
        parError = "animation channel target is not in target rig: " +
                   source_node_name;
        return appended_count;
      }
      AnimationChannel channel = source_channel;
      channel.target_node = *target_node;
      clip.channels.push_back(channel);
    }

    if (clip.channels.empty()) {
      continue;
    }
    if (!parPackLabel.empty()) {
      clip.name = std::string(parPackLabel) + " / " +
                  (clip.name.empty() ? "Imported clip" : clip.name);
    }
    parTarget.animation_clips.push_back(std::move(clip));
    ++appended_count;
  }

  parTarget.stats.animation_count = parTarget.animation_clips.size();
  return appended_count;
}

}  // namespace

AssetId makeStableAssetId(std::string_view parNamespace,
                          const std::filesystem::path& parPath) {
  constexpr std::uint64_t FNV_OFFSET = 14695981039346656037ull;
  constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
  std::uint64_t hash = FNV_OFFSET;
  const std::string key =
      std::string(parNamespace) + ":" + normalizedPathKey(parPath);
  for (const unsigned char character : key) {
    hash ^= character;
    hash *= FNV_PRIME;
  }
  return AssetId{static_cast<std::size_t>(hash)};
}

const char* getAssetLoadStateLabel(AssetLoadState parState) {
  switch (parState) {
    case AssetLoadState::MetadataReady:
      return "Metadata";
    case AssetLoadState::Loading:
      return "Loading";
    case AssetLoadState::Ready:
      return "Ready";
    case AssetLoadState::Error:
      return "Error";
  }
  return "Unknown";
}

std::size_t AssetRegistry::registerStaticAsset(
    std::string parLabel, std::filesystem::path parPath) {
  return registerStaticAsset(std::move(parLabel), std::move(parPath), {});
}

std::size_t AssetRegistry::registerStaticAsset(
    std::string parLabel, std::filesystem::path parPath,
    std::filesystem::path parSourcePath) {
  return registerStaticAsset(makeStableAssetId("asset", parPath),
                             std::move(parLabel), std::move(parPath),
                             std::move(parSourcePath));
}

std::size_t AssetRegistry::registerStaticAsset(
    AssetId parAssetId, std::string parLabel, std::filesystem::path parPath,
    std::filesystem::path parSourcePath) {
  if (const std::optional<std::size_t> existing =
          getAssetIndexById(parAssetId);
      existing.has_value()) {
    return *existing;
  }

  AssetLibraryEntry entry;
  entry.id = parAssetId.isValid() ? parAssetId
                                  : makeStableAssetId("asset", parPath);
  entry.label = std::move(parLabel);
  entry.path = std::move(parPath);
  entry.source_path = std::move(parSourcePath);
  entry.load_state = AssetLoadState::MetadataReady;
  entry.mesh_handle = m_asset_library.size();

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
}

std::size_t AssetRegistry::registerModelAsset(std::string parLabel,
                                              std::filesystem::path parPath,
                                              ModelAsset parDocument) {
  AssetLibraryEntry entry;
  entry.id = makeStableAssetId("asset", parPath);
  entry.label = std::move(parLabel);
  entry.path = std::move(parPath);
  entry.document = std::move(parDocument);
  entry.load_state = AssetLoadState::Ready;
  entry.mesh_handle = m_asset_library.size();

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
}

bool AssetRegistry::addAnimationPack(AssetId parAssetId, std::string parLabel,
                                     std::filesystem::path parPath,
                                     std::string& parError) {
  AssetLibraryEntry* asset = getAssetLibraryEntryById(parAssetId);
  if (asset == nullptr) {
    parError = "target asset was not found";
    return false;
  }

  if (!asset->document.has_value()) {
    try {
      loadAsset(*getAssetIndexById(parAssetId));
    } catch (const std::exception& error) {
      parError = error.what();
      return false;
    }
  }

  try {
    GltfAssetLoader loader;
    ModelAsset animation_asset = loader.loadDocument(parPath);
    if (animation_asset.animation_clips.empty()) {
      parError = "imported file contains no animation clips";
      return false;
    }
    if (!hasCompatibleJointNames(*asset->document, animation_asset, parError)) {
      return false;
    }
    const std::size_t appended_count = appendRemappedAnimationClips(
        *asset->document, animation_asset, parLabel, parError);
    if (appended_count == 0) {
      parError = "no compatible animation channels were found";
      return false;
    }
  } catch (const std::exception& error) {
    parError = error.what();
    return false;
  }

  for (const AnimationPackEntry& pack : asset->animation_packs) {
    if (pack.path == parPath) {
      return true;
    }
  }
  asset->animation_packs.push_back({std::move(parLabel), std::move(parPath)});
  return true;
}

ModelAsset& AssetRegistry::loadAsset(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    throw std::runtime_error("Asset library index is out of range");
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (asset.pending_document.has_value()) {
    try {
      asset.document = asset.pending_document->get();
      asset.pending_document.reset();
      asset.last_cpu_import_ms =
          elapsedMilliseconds(asset.load_started_at,
                              std::chrono::steady_clock::now());
      asset.load_state = AssetLoadState::Ready;
      asset.load_error.clear();
      applyAnimationPacks(asset);
    } catch (const std::exception& error) {
      asset.pending_document.reset();
      asset.last_cpu_import_ms =
          elapsedMilliseconds(asset.load_started_at,
                              std::chrono::steady_clock::now());
      asset.load_state = AssetLoadState::Error;
      asset.load_error = error.what();
      throw;
    }
  }

  if (!asset.document.has_value()) {
    try {
      GltfAssetLoader loader;
      asset.load_state = AssetLoadState::Loading;
      asset.load_started_at = std::chrono::steady_clock::now();
      asset.document = loader.loadDocument(asset.path);
      asset.last_cpu_import_ms =
          elapsedMilliseconds(asset.load_started_at,
                              std::chrono::steady_clock::now());
      asset.load_state = AssetLoadState::Ready;
      asset.load_error.clear();
      applyAnimationPacks(asset);
    } catch (const std::exception& error) {
      asset.last_cpu_import_ms =
          elapsedMilliseconds(asset.load_started_at,
                              std::chrono::steady_clock::now());
      asset.load_state = AssetLoadState::Error;
      asset.load_error = error.what();
      throw;
    }
  }
  return *asset.document;
}

void AssetRegistry::requestLoad(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (asset.document.has_value() || asset.pending_document.has_value()) {
    return;
  }

  asset.load_state = AssetLoadState::Loading;
  asset.load_error.clear();
  asset.last_cpu_import_ms = 0.0f;
  asset.load_started_at = std::chrono::steady_clock::now();
  const std::filesystem::path path = asset.path;
  asset.pending_document = std::async(std::launch::async, [path]() {
    GltfAssetLoader loader;
    return loader.loadDocument(path);
  });
}

bool AssetRegistry::pollLoad(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return false;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (!asset.pending_document.has_value()) {
    return false;
  }

  using namespace std::chrono_literals;
  if (asset.pending_document->wait_for(0ms) != std::future_status::ready) {
    return false;
  }

  try {
    asset.document = asset.pending_document->get();
    asset.pending_document.reset();
    asset.last_cpu_import_ms =
        elapsedMilliseconds(asset.load_started_at,
                            std::chrono::steady_clock::now());
    asset.load_state = AssetLoadState::Ready;
    asset.load_error.clear();
    applyAnimationPacks(asset);
  } catch (const std::exception& error) {
    asset.pending_document.reset();
    asset.last_cpu_import_ms =
        elapsedMilliseconds(asset.load_started_at,
                            std::chrono::steady_clock::now());
    asset.load_state = AssetLoadState::Error;
    asset.load_error = error.what();
  }
  return true;
}

std::string AssetRegistry::reserveInstanceName(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    throw std::runtime_error("Asset library index is out of range");
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  ++asset.instance_count;
  ++asset.next_instance_number;
  return asset.label + " " + std::to_string(asset.next_instance_number);
}

void AssetRegistry::releaseInstance(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (asset.instance_count > 0) {
    --asset.instance_count;
  }
}

void AssetRegistry::setInstanceState(std::size_t parAssetIndex,
                                     std::size_t parCount,
                                     std::size_t parNextInstanceNumber) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  asset.instance_count = parCount;
  asset.next_instance_number = parNextInstanceNumber;
}

void AssetRegistry::resetInstanceCounts() {
  for (AssetLibraryEntry& asset : m_asset_library) {
    asset.instance_count = 0;
    asset.next_instance_number = 0;
  }
}

void AssetRegistry::rebuildInstanceCountsFromAssets(
    std::span<const std::size_t> parAssets) {
  resetInstanceCounts();
  for (std::size_t asset_index : parAssets) {
    if (asset_index >= m_asset_library.size()) {
      continue;
    }
    AssetLibraryEntry& asset = m_asset_library[asset_index];
    ++asset.instance_count;
    ++asset.next_instance_number;
  }
}

std::span<const AssetRegistry::AssetLibraryEntry>
AssetRegistry::getAssetLibrary() const {
  return m_asset_library;
}

const AssetRegistry::AssetLibraryEntry* AssetRegistry::getAssetLibraryEntry(
    std::size_t parAssetIndex) const {
  if (parAssetIndex >= m_asset_library.size()) {
    return nullptr;
  }

  return &m_asset_library[parAssetIndex];
}

AssetRegistry::AssetLibraryEntry* AssetRegistry::getAssetLibraryEntry(
    std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return nullptr;
  }

  return &m_asset_library[parAssetIndex];
}

const AssetRegistry::AssetLibraryEntry*
AssetRegistry::getAssetLibraryEntryById(AssetId parAssetId) const {
  const std::optional<std::size_t> index = getAssetIndexById(parAssetId);
  return index.has_value() ? &m_asset_library[*index] : nullptr;
}

AssetRegistry::AssetLibraryEntry* AssetRegistry::getAssetLibraryEntryById(
    AssetId parAssetId) {
  const std::optional<std::size_t> index = getAssetIndexById(parAssetId);
  return index.has_value() ? &m_asset_library[*index] : nullptr;
}

std::optional<std::size_t> AssetRegistry::getAssetIndexById(
    AssetId parAssetId) const {
  if (!parAssetId.isValid()) {
    return std::nullopt;
  }
  for (std::size_t index = 0; index < m_asset_library.size(); ++index) {
    if (m_asset_library[index].id == parAssetId) {
      return index;
    }
  }
  return std::nullopt;
}

const ModelAsset* AssetRegistry::getLoadedAsset(
    std::size_t parAssetIndex) const {
  const AssetLibraryEntry* asset = getAssetLibraryEntry(parAssetIndex);
  return asset != nullptr && asset->document.has_value()
             ? &*asset->document
             : nullptr;
}

const StaticModel* AssetRegistry::getStaticMeshSource(
    StaticMeshHandle parHandle) const {
  for (const AssetLibraryEntry& entry : m_asset_library) {
    if (entry.mesh_handle == parHandle && entry.document.has_value()) {
      return &entry.document->static_model;
    }
  }
  return nullptr;
}

void AssetRegistry::applyAnimationPacks(AssetLibraryEntry& parAsset) {
  if (!parAsset.document.has_value() || parAsset.animation_packs.empty()) {
    return;
  }

  for (const AnimationPackEntry& pack : parAsset.animation_packs) {
    try {
      GltfAssetLoader loader;
      ModelAsset animation_asset = loader.loadDocument(pack.path);
      std::string error;
      if (!hasCompatibleJointNames(*parAsset.document, animation_asset, error)) {
        parAsset.load_error = error;
        parAsset.load_state = AssetLoadState::Error;
        return;
      }
      const std::size_t appended_count = appendRemappedAnimationClips(
          *parAsset.document, animation_asset, pack.label, error);
      if (appended_count == 0) {
        parAsset.load_error = error.empty() ? "animation pack added no clips"
                                            : error;
        parAsset.load_state = AssetLoadState::Error;
        return;
      }
    } catch (const std::exception& error) {
      parAsset.load_error = error.what();
      parAsset.load_state = AssetLoadState::Error;
      return;
    }
  }
}

}  // namespace kage::assets
