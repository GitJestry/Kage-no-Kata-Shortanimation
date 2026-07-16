#include "render/mesh_resource_cache.hpp"

#include <utility>

namespace kage::render {

void MeshResourceCache::uploadStaticMesh(
    std::size_t parAssetIndex, const assets::StaticModel& parModel) {
  if (parAssetIndex >= m_static_meshes.size()) {
    m_static_meshes.resize(parAssetIndex + 1);
  }

  m_static_meshes[parAssetIndex].upload(parModel, m_texture_cache);
}

const GpuMesh* MeshResourceCache::getStaticMesh(
    std::size_t parAssetIndex) const {
  if (parAssetIndex >= m_static_meshes.size()) {
    return nullptr;
  }
  const GpuMesh& mesh = m_static_meshes[parAssetIndex];
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
