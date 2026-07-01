#include "render/mesh_resource_cache.hpp"

namespace kage::render {

void MeshResourceCache::uploadStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle,
    const assets::StaticModel& parModel) {
  if (parHandle >= m_static_meshes.size()) {
    m_static_meshes.resize(parHandle + 1);
  }

  m_static_meshes[parHandle].upload(parModel);
}

const GpuMesh* MeshResourceCache::getStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle) const {
  if (parHandle >= m_static_meshes.size() ||
      !m_static_meshes[parHandle].isValid()) {
    return nullptr;
  }

  return &m_static_meshes[parHandle];
}

void MeshResourceCache::clear() {
  m_static_meshes.clear();
}

}  // namespace kage::render
