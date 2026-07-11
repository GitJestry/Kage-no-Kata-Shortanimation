#pragma once

#include "assets/asset_registry.hpp"
#include "render/gpu_mesh.hpp"
#include "render/texture_resource_cache.hpp"

#include <cstddef>
#include <vector>

namespace kage::render {

class MeshResourceCache final {
 public:
  void uploadStaticMesh(assets::AssetRegistry::StaticMeshHandle parHandle,
                        const assets::StaticModel& parModel,
                        assets::AssetQualityTier parQuality);
  [[nodiscard]] const GpuMesh* getStaticMesh(
      assets::AssetRegistry::StaticMeshHandle parHandle,
      assets::AssetQualityTier parQuality = assets::AssetQualityTier::Proxy) const;
  void releaseFinalMeshes();
  void clear();
  [[nodiscard]] std::size_t getEstimatedTextureBytes(
      assets::AssetQualityTier parQuality) const;

 private:
  struct MeshTiers final {
    GpuMesh proxy;
    GpuMesh final;
  };
  TextureResourceCache m_texture_cache;
  std::vector<MeshTiers> m_static_meshes;
};

}  // namespace kage::render
