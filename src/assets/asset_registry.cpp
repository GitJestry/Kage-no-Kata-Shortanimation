#include "assets/asset_registry.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace kage::assets {

namespace {

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


[[nodiscard]] std::filesystem::path canonicalPathKey(
    const std::filesystem::path& parPath) {
  std::error_code error_code;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(parPath, error_code);
  if (!error_code) {
    return canonical_path.lexically_normal();
  }
  return std::filesystem::absolute(parPath).lexically_normal();
}

[[nodiscard]] std::string normalizedTextKey(std::string_view parText) {
  std::string key;
  key.reserve(parText.size());
  for (const unsigned char character : parText) {
    if (std::isalnum(character) != 0) {
      key.push_back(static_cast<char>(std::tolower(character)));
    }
  }
  return key;
}

[[nodiscard]] std::string makeImportedClipName(std::string_view parPackLabel,
                                               const AnimationClip& parClip) {
  const std::string source_name =
      parClip.name.empty() ? "Imported clip" : parClip.name;
  if (parPackLabel.empty()) {
    return source_name;
  }
  return std::string(parPackLabel) + " / " + source_name;
}

[[nodiscard]] bool hasClipNamed(const ModelAsset& parAsset,
                                std::string_view parClipName) {
  const std::string clip_key = normalizedTextKey(parClipName);
  for (const AnimationClip& clip : parAsset.animation_clips) {
    if (normalizedTextKey(clip.name) == clip_key) {
      return true;
    }
  }
  return false;
}

void deduplicateAnimationPacks(AssetRegistry::AssetLibraryEntry& parAsset) {
  std::vector<AssetRegistry::AnimationPackEntry> unique_packs;
  unique_packs.reserve(parAsset.animation_packs.size());
  std::unordered_set<std::string> seen_paths;

  for (AssetRegistry::AnimationPackEntry& pack : parAsset.animation_packs) {
    const std::string path_key = canonicalPathKey(pack.path).generic_string();
    if (seen_paths.insert(path_key).second) {
      unique_packs.push_back(std::move(pack));
    }
  }

  parAsset.animation_packs = std::move(unique_packs);
}

[[nodiscard]] std::vector<const AnimationClip*> selectImportedAnimationClips(
    const ModelAsset& parSource, std::string_view parPackLabel) {
  std::vector<const AnimationClip*> clips;
  const std::string label_key = normalizedTextKey(parPackLabel);

  if (!label_key.empty()) {
    for (const AnimationClip& clip : parSource.animation_clips) {
      if (!clip.channels.empty() && normalizedTextKey(clip.name) == label_key) {
        clips.push_back(&clip);
      }
    }
    if (!clips.empty()) {
      return clips;
    }
  }

  for (const AnimationClip& clip : parSource.animation_clips) {
    if (!clip.channels.empty()) {
      clips.push_back(&clip);
    }
  }

  return clips;
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
  const std::vector<const AnimationClip*> source_clips =
      selectImportedAnimationClips(parSource, parPackLabel);
  if (source_clips.empty()) {
    parError = "imported animation contains no usable animation channels";
    return appended_count;
  }

  for (const AnimationClip* source_clip : source_clips) {
    AnimationClip clip = *source_clip;
    clip.channels.clear();
    for (const AnimationChannel& source_channel : source_clip->channels) {
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

    clip.name = makeImportedClipName(parPackLabel, clip);
    if (hasClipNamed(parTarget, clip.name)) {
      continue;
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

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
}

std::size_t AssetRegistry::registerModelAsset(std::string parLabel,
                                              std::filesystem::path parPath,
                                              ModelAsset parDocument) {
  const AssetId asset_id = makeStableAssetId("asset", parPath);
  if (const std::optional<std::size_t> existing = getAssetIndexById(asset_id);
      existing.has_value()) {
    AssetLibraryEntry& entry = m_asset_library[*existing];
    entry.label = std::move(parLabel);
    entry.path = std::move(parPath);
    entry.document = std::move(parDocument);
    entry.load_state = AssetLoadState::Ready;
    entry.load_error.clear();
    return *existing;
  }

  AssetLibraryEntry entry;
  entry.id = asset_id;
  entry.label = std::move(parLabel);
  entry.path = std::move(parPath);
  entry.document = std::move(parDocument);
  entry.load_state = AssetLoadState::Ready;

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
}

void AssetRegistry::requestLoad(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (asset.document.has_value() &&
      (!asset.document->static_model.primitives.empty() ||
       !asset.document->skins.empty())) {
    return;
  }
  if (asset.load_state == AssetLoadState::Queued ||
      asset.load_state == AssetLoadState::CpuLoading ||
      asset.load_state == AssetLoadState::GpuUploading) {
    return;
  }
  asset.document.reset();

  asset.load_state = AssetLoadState::Queued;
  asset.load_error.clear();
  asset.last_cpu_import_ms = 0.0f;
  asset.load_started_at = std::chrono::steady_clock::now();
}

void AssetRegistry::beginCpuLoad(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }
  m_asset_library[parAssetIndex].load_state = AssetLoadState::CpuLoading;
}

bool AssetRegistry::completeCpuLoad(std::size_t parAssetIndex,
                                    std::optional<ModelAsset> parDocument,
                                    std::string parError, float parCpuMs) {
  if (parAssetIndex >= m_asset_library.size()) {
    return false;
  }
  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  asset.last_cpu_import_ms = parCpuMs;
  if (!parDocument.has_value()) {
    asset.load_state = AssetLoadState::Error;
    asset.load_error = std::move(parError);
    return false;
  }
  asset.document = std::move(parDocument);
  asset.load_state = AssetLoadState::GpuUploading;
  asset.load_error.clear();
  applyAnimationPacks(asset);
  return true;
}

void AssetRegistry::completeGpuUpload(std::size_t parAssetIndex) {
  AssetLibraryEntry* asset = getAssetLibraryEntry(parAssetIndex);
  if (asset != nullptr && asset->load_state == AssetLoadState::GpuUploading) {
    asset->load_state = AssetLoadState::Ready;
  }
}

void AssetRegistry::failLoad(std::size_t parAssetIndex, std::string parError) {
  AssetLibraryEntry* asset = getAssetLibraryEntry(parAssetIndex);
  if (asset == nullptr) {
    return;
  }
  asset->load_state = AssetLoadState::Error;
  asset->load_error = std::move(parError);
  asset->document.reset();
}

void AssetRegistry::releaseStaticGeometryPayload(std::size_t parAssetIndex) {
  AssetLibraryEntry* asset = getAssetLibraryEntry(parAssetIndex);
  if (asset == nullptr || !asset->document.has_value()) {
    return;
  }
  asset->document->static_model.primitives.clear();
  asset->document->static_model.primitives.shrink_to_fit();
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
    std::size_t parAssetIndex) const {
  const ModelAsset* asset = getLoadedAsset(parAssetIndex);
  return asset != nullptr ? &asset->static_model : nullptr;
}

void AssetRegistry::applyAnimationPacks(AssetLibraryEntry& parAsset) {
  if (!parAsset.document.has_value() || parAsset.animation_packs.empty()) {
    return;
  }

  deduplicateAnimationPacks(parAsset);
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
