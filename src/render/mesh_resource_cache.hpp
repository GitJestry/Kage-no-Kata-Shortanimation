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
                        const assets::StaticModel& parModel);
  [[nodiscard]] const GpuMesh* getStaticMesh(
      assets::AssetRegistry::StaticMeshHandle parHandle) const;
  void clear();
  [[nodiscard]] std::size_t getEstimatedTextureBytes() const;

 private:
  TextureResourceCache m_texture_cache;
  std::vector<GpuMesh> m_static_meshes;
};

}  // namespace kage::render
