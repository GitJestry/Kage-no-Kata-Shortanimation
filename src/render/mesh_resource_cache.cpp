#include "render/mesh_resource_cache.hpp"

#include <utility>

namespace kage::render {

void MeshResourceCache::uploadStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle,
    const assets::StaticModel& parModel,
    assets::AssetQualityTier parQuality) {
  if (parHandle >= m_static_meshes.size()) {
    m_static_meshes.resize(parHandle + 1);
  }

  GpuMesh uploaded;
  uploaded.upload(parModel, parQuality, m_texture_cache);
  GpuMesh& destination = parQuality == assets::AssetQualityTier::Final
                             ? m_static_meshes[parHandle].final
                             : m_static_meshes[parHandle].proxy;
  destination = std::move(uploaded);
}

const GpuMesh* MeshResourceCache::getStaticMesh(
    assets::AssetRegistry::StaticMeshHandle parHandle,
    assets::AssetQualityTier parQuality) const {
  if (parHandle >= m_static_meshes.size()) {
    return nullptr;
  }
  const GpuMesh& mesh = parQuality == assets::AssetQualityTier::Final
                            ? m_static_meshes[parHandle].final
                            : m_static_meshes[parHandle].proxy;
  return mesh.isValid() ? &mesh : nullptr;
}

void MeshResourceCache::releaseFinalMeshes() {
  for (MeshTiers& tiers : m_static_meshes) {
    tiers.final.clear();
  }
  m_texture_cache.releaseExpired(assets::AssetQualityTier::Final);
}

void MeshResourceCache::clear() {
  m_static_meshes.clear();
  m_texture_cache.clear();
}

std::size_t MeshResourceCache::getEstimatedTextureBytes(
    assets::AssetQualityTier parQuality) const {
  return m_texture_cache.getResidentBytes(parQuality);
}

}  // namespace kage::render
