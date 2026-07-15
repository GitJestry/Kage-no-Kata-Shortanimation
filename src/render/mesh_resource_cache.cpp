#include "render/mesh_resource_cache.hpp"

#include <utility>

namespace kage::render {

void MeshResourceCache::uploadStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle,
    const assets::StaticModel& parModel) {
  if (parHandle >= m_static_meshes.size()) {
    m_static_meshes.resize(parHandle + 1);
  }

  m_static_meshes[parHandle].upload(parModel, m_texture_cache);
}

const GpuMesh* MeshResourceCache::getStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle) const {
  if (parHandle >= m_static_meshes.size()) {
    return nullptr;
  }
  const GpuMesh& mesh = m_static_meshes[parHandle];
  return mesh.isValid() ? &mesh : nullptr;
}

void MeshResourceCache::clear() {
  m_static_meshes.clear();
  m_texture_cache.clear();
}

std::size_t MeshResourceCache::getEstimatedTextureBytes() const {
  return m_texture_cache.getResidentBytes();
}

}  // namespace kage::render
