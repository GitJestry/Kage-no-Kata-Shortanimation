#pragma once

#include "assets/asset_registry.hpp"
#include "render/gpu_mesh.hpp"

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

 private:
  std::vector<GpuMesh> m_static_meshes;
};

}  // namespace kage::render
