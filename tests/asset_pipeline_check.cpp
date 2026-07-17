#include "check_helpers.hpp"

#include "assets/asset_path.hpp"
#include "assets/asset_registry.hpp"
#include "assets/asset_streamer.hpp"
#include "assets/gltf_asset_loader.hpp"
#include "assets/project_asset_catalog.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using kage::test::fail;

class TempDirectory final {
public:
  TempDirectory() {
    path = std::filesystem::temp_directory_path() / "kage_asset_pipeline_check";
    std::error_code error;
    std::filesystem::remove_all(path, error);
    std::filesystem::create_directories(path);
  }
  ~TempDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  std::filesystem::path path;
};

void writeText(const std::filesystem::path& parPath, const std::string& parText) {
  std::filesystem::create_directories(parPath.parent_path());
  std::ofstream output(parPath);
  output << parText;
  if (!output) {
    throw std::runtime_error("could not write test fixture");
  }
}

[[nodiscard]] bool testPathsAndCatalog(const std::filesystem::path& parRoot) {
  using namespace kage::assets;
  if (!hasGltfExtension("MODEL.GLB") || !hasGltfExtension("model.gltf") ||
      hasGltfExtension("model.obj") || !hasPanoramaExtension("sky.HDR") ||
      defaultAssetLabelFromPath("folder/samurai.glb") != "samurai") {
    return false;
  }

  const auto source = parRoot / "incoming" / "model.glb";
  const auto assets = parRoot / "assets";
  const auto models = assets / "models";
  writeText(source, "fixture");
  std::string error;
  const auto copied = copyIntoProjectAssets(source, models, assets, error);
  if (!error.empty() || !std::filesystem::exists(copied) || !isInsideDirectory(copied, assets)) {
    return false;
  }
  const auto unique = makeUniqueDestination(models, source.filename());
  if (unique == copied) {
    return false;
  }

  ProjectAssetCatalog catalog;
  ProjectAssetEntry entry;
  entry.id = makeStableAssetId("model", "assets/models/model.glb");
  entry.label = "Model";
  entry.model_path = "assets/models/model.glb";
  catalog.assets.push_back(entry);
  EnvironmentAsset environment;
  environment.id = makeStableAssetId("environment", "assets/sky.hdr");
  environment.label = "Sky";
  environment.path = "assets/sky.hdr";
  environment.hdr = true;
  catalog.environments.push_back(environment);
  const auto catalog_path = parRoot / "projects" / "catalog.json";
  saveProjectAssetCatalog(catalog_path, catalog);
  const ProjectAssetCatalog loaded = loadProjectAssetCatalog(catalog_path);
  return loaded.assets.size() == 1 && loaded.environments.size() == 1 &&
         loaded.assets.front().id == entry.id &&
         std::filesystem::weakly_canonical(loaded.assets.front().model_path) ==
             std::filesystem::weakly_canonical(parRoot / entry.model_path);
}

[[nodiscard]] bool testRegistry() {
  using namespace kage::assets;
  AssetRegistry registry;
  const AssetId id = makeStableAssetId("model", "box.glb");
  const std::size_t index = registry.registerStaticAsset(id, "Box", "box.glb");
  if (registry.registerStaticAsset(id, "Duplicate", "other.glb") != index ||
      registry.getAssetIndexById(id) != index) {
    return false;
  }
  registry.requestLoad(index);
  registry.beginCpuLoad(index);
  ModelAsset model;
  model.static_model.scene_name = "Fixture";
  if (!registry.completeCpuLoad(index, std::move(model), {}, 1.25f)) {
    return false;
  }
  registry.completeGpuUpload(index);
  const auto* entry = registry.getAssetLibraryEntry(index);
  if (entry == nullptr || entry->load_state != AssetLoadState::Ready ||
      registry.getLoadedAsset(index) == nullptr || registry.reserveInstanceName(index) != "Box 1") {
    return false;
  }
  registry.releaseInstance(index);
  registry.failLoad(index, "expected failure");
  return registry.getAssetLibraryEntry(index)->load_state == AssetLoadState::Error;
}

[[nodiscard]] bool testGltfLoader(const std::filesystem::path& parRoot) {
  const auto valid = parRoot / "triangle.gltf";
  writeText(valid, R"({
    "asset":{"version":"2.0"},
    "buffers":[{"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA","byteLength":42}],
    "bufferViews":[
      {"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},
      {"buffer":0,"byteOffset":36,"byteLength":6,"target":34963}
    ],
    "accessors":[
      {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},
      {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
    ],
    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],
    "nodes":[{"mesh":0,"name":"Triangle"}],
    "scenes":[{"nodes":[0]}],"scene":0
  })");
  const kage::assets::GltfDocument document = kage::assets::GltfAssetLoader{}.loadDocument(valid);
  if (document.static_model.primitives.size() != 1 ||
      document.static_model.primitives.front().vertices.size() != 3 ||
      document.static_model.primitives.front().indices.size() != 3) {
    return false;
  }

  const auto pointer = parRoot / "pointer.glb";
  writeText(pointer, "version https://git-lfs.github.com/spec/v1\n"
                     "oid sha256:0000000000000000000000000000000000000000000000000000000000000000\n"
                     "size 123\n");
  try {
    static_cast<void>(kage::assets::GltfAssetLoader{}.loadDocument(pointer));
  } catch (const std::exception& error) {
    return std::string(error.what()).find("Git LFS") != std::string::npos;
  }
  return false;
}

[[nodiscard]] bool testStreamer(const std::filesystem::path& parRoot) {
  kage::assets::AssetStreamer streamer(1);
  streamer.request(7, parRoot / "triangle.gltf", kage::assets::AssetLoadPriority::Background);
  streamer.request(8, parRoot / "missing.glb", kage::assets::AssetLoadPriority::Selected);
  bool loaded_fixture = false;
  bool rejected_missing = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto result = streamer.poll()) {
      if (result->asset_index == 7) {
        loaded_fixture = result->document.has_value() && result->error.empty() &&
                         result->document->static_model.primitives.size() == 1;
      } else if (result->asset_index == 8) {
        rejected_missing = !result->document.has_value() && !result->error.empty();
      } else {
        return false;
      }
      if (loaded_fixture && rejected_missing) {
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

} // namespace

int main() {
  try {
    const TempDirectory temporary;
    if (!testPathsAndCatalog(temporary.path)) {
      return fail("asset paths or catalog regression");
    }
    if (!testRegistry()) {
      return fail("asset registry lifecycle regression");
    }
    if (!testGltfLoader(temporary.path)) {
      return fail("GLTF loader or LFS-pointer regression");
    }
    if (!testStreamer(temporary.path)) {
      return fail("asset streamer success/failure regression");
    }
  } catch (const std::exception& error) {
    return fail(error.what());
  }
  return 0;
}
