#include "assets/gltf_asset_loader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr int DEFAULT_SCENE_INDEX = 0;
constexpr int DEFAULT_MODE_TRIANGLES = 4;

[[nodiscard]] std::runtime_error makeImportError(
    const std::filesystem::path& parPath, const std::string& parMessage) {
  return std::runtime_error("Failed to import " + parPath.string() + ": " +
                            parMessage);
}

[[nodiscard]] const tinygltf::Accessor& getAccessor(
    const tinygltf::Model& parModel, int parAccessorIndex,
    const std::filesystem::path& parPath) {
  if (parAccessorIndex < 0 ||
      static_cast<std::size_t>(parAccessorIndex) >= parModel.accessors.size()) {
    throw makeImportError(parPath, "accessor index is out of range");
  }

  const tinygltf::Accessor& accessor =
      parModel.accessors[static_cast<std::size_t>(parAccessorIndex)];
  if (accessor.sparse.isSparse) {
    throw makeImportError(parPath, "sparse accessors are not supported yet");
  }

  return accessor;
}

[[nodiscard]] const unsigned char* getAccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.bufferView < 0 ||
      static_cast<std::size_t>(parAccessor.bufferView) >=
          parModel.bufferViews.size()) {
    throw makeImportError(parPath, "accessor has no valid buffer view");
  }

  const tinygltf::BufferView& buffer_view =
      parModel.bufferViews[static_cast<std::size_t>(parAccessor.bufferView)];
  if (buffer_view.buffer < 0 ||
      static_cast<std::size_t>(buffer_view.buffer) >= parModel.buffers.size()) {
    throw makeImportError(parPath, "buffer view has no valid buffer");
  }

  const int byte_stride = parAccessor.ByteStride(buffer_view);
  if (byte_stride <= 0) {
    throw makeImportError(parPath, "accessor byte stride is invalid");
  }

  const tinygltf::Buffer& buffer =
      parModel.buffers[static_cast<std::size_t>(buffer_view.buffer)];
  const std::size_t byte_offset =
      buffer_view.byteOffset + parAccessor.byteOffset +
      static_cast<std::size_t>(byte_stride) * parElementIndex;
  if (byte_offset >= buffer.data.size()) {
    throw makeImportError(parPath, "accessor byte offset is out of range");
  }

  return buffer.data.data() + byte_offset;
}

[[nodiscard]] glm::vec3 readVec3AccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      parAccessor.type != TINYGLTF_TYPE_VEC3) {
    throw makeImportError(parPath, "expected a float vec3 accessor");
  }

  glm::vec3 value{};
  std::memcpy(glm::value_ptr(value),
              getAccessorElement(parModel, parAccessor, parElementIndex,
                                 parPath),
              sizeof(float) * 3);
  return value;
}

[[nodiscard]] glm::vec2 readVec2AccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      parAccessor.type != TINYGLTF_TYPE_VEC2) {
    throw makeImportError(parPath, "expected a float vec2 accessor");
  }

  glm::vec2 value{};
  std::memcpy(glm::value_ptr(value),
              getAccessorElement(parModel, parAccessor, parElementIndex,
                                 parPath),
              sizeof(float) * 2);
  return value;
}

[[nodiscard]] std::vector<std::uint32_t> readIndices(
    const tinygltf::Model& parModel, const tinygltf::Primitive& parPrimitive,
    std::size_t parVertexCount, const std::filesystem::path& parPath) {
  if (parPrimitive.indices < 0) {
    std::vector<std::uint32_t> indices(parVertexCount);
    for (std::size_t index = 0; index < parVertexCount; ++index) {
      indices[index] = static_cast<std::uint32_t>(index);
    }
    return indices;
  }

  const tinygltf::Accessor& accessor =
      getAccessor(parModel, parPrimitive.indices, parPath);
  if (accessor.type != TINYGLTF_TYPE_SCALAR) {
    throw makeImportError(parPath, "expected scalar index accessor");
  }

  std::vector<std::uint32_t> indices;
  indices.reserve(accessor.count);

  for (std::size_t index = 0; index < accessor.count; ++index) {
    const unsigned char* element =
        getAccessorElement(parModel, accessor, index, parPath);

    switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        indices.push_back(*element);
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        std::uint16_t value = 0;
        std::memcpy(&value, element, sizeof(value));
        indices.push_back(value);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        std::uint32_t value = 0;
        std::memcpy(&value, element, sizeof(value));
        indices.push_back(value);
        break;
      }
      default:
        throw makeImportError(parPath, "unsupported index component type");
    }
  }

  return indices;
}

[[nodiscard]] glm::mat4 getNodeMatrix(const tinygltf::Node& parNode) {
  if (parNode.matrix.size() == 16) {
    return glm::make_mat4(parNode.matrix.data());
  }

  glm::mat4 transform{1.0f};
  if (parNode.translation.size() == 3) {
    transform = glm::translate(
        transform, glm::vec3(static_cast<float>(parNode.translation[0]),
                             static_cast<float>(parNode.translation[1]),
                             static_cast<float>(parNode.translation[2])));
  }

  if (parNode.rotation.size() == 4) {
    const glm::quat rotation(
        static_cast<float>(parNode.rotation[3]),
        static_cast<float>(parNode.rotation[0]),
        static_cast<float>(parNode.rotation[1]),
        static_cast<float>(parNode.rotation[2]));
    transform *= glm::mat4_cast(rotation);
  }

  if (parNode.scale.size() == 3) {
    transform = glm::scale(
        transform, glm::vec3(static_cast<float>(parNode.scale[0]),
                             static_cast<float>(parNode.scale[1]),
                             static_cast<float>(parNode.scale[2])));
  }

  return transform;
}

void importPrimitive(const tinygltf::Model& parModel,
                     const tinygltf::Primitive& parPrimitive,
                     const glm::mat4& parNodeTransform,
                     const std::string& parName,
                     const std::filesystem::path& parPath,
                     kage::assets::StaticModel& parOutput) {
  const int primitive_mode =
      parPrimitive.mode >= 0 ? parPrimitive.mode : DEFAULT_MODE_TRIANGLES;
  if (primitive_mode != TINYGLTF_MODE_TRIANGLES) {
    throw makeImportError(parPath, "only triangle primitives are supported");
  }

  const auto position_attribute = parPrimitive.attributes.find("POSITION");
  if (position_attribute == parPrimitive.attributes.end()) {
    throw makeImportError(parPath, "primitive has no POSITION attribute");
  }

  const tinygltf::Accessor& position_accessor =
      getAccessor(parModel, position_attribute->second, parPath);
  const tinygltf::Accessor* normal_accessor = nullptr;
  const tinygltf::Accessor* tex_coord_accessor = nullptr;

  const auto normal_attribute = parPrimitive.attributes.find("NORMAL");
  if (normal_attribute != parPrimitive.attributes.end()) {
    normal_accessor = &getAccessor(parModel, normal_attribute->second, parPath);
  }

  const auto tex_coord_attribute = parPrimitive.attributes.find("TEXCOORD_0");
  if (tex_coord_attribute != parPrimitive.attributes.end()) {
    tex_coord_accessor =
        &getAccessor(parModel, tex_coord_attribute->second, parPath);
  }

  kage::assets::StaticPrimitive output_primitive;
  output_primitive.name = parName;
  output_primitive.transform = parNodeTransform;
  if (parPrimitive.material >= 0) {
    output_primitive.material_index =
        static_cast<std::uint32_t>(parPrimitive.material);
  }
  output_primitive.vertices.reserve(position_accessor.count);

  for (std::size_t vertex_index = 0; vertex_index < position_accessor.count;
       ++vertex_index) {
    kage::assets::StaticVertex vertex;
    vertex.position = readVec3AccessorElement(parModel, position_accessor,
                                              vertex_index, parPath);
    if (normal_accessor != nullptr) {
      vertex.normal =
          readVec3AccessorElement(parModel, *normal_accessor, vertex_index,
                                  parPath);
    }
    if (tex_coord_accessor != nullptr) {
      vertex.tex_coord =
          readVec2AccessorElement(parModel, *tex_coord_accessor, vertex_index,
                                  parPath);
    }

    output_primitive.vertices.push_back(vertex);

    const glm::vec3 world_position =
        glm::vec3(parNodeTransform * glm::vec4(vertex.position, 1.0f));
    output_primitive.bounds.includePoint(world_position);
    parOutput.bounds.includePoint(world_position);
  }

  output_primitive.indices = readIndices(parModel, parPrimitive,
                                         output_primitive.vertices.size(),
                                         parPath);

  parOutput.stats.vertex_count += output_primitive.vertices.size();
  parOutput.stats.index_count += output_primitive.indices.size();
  parOutput.stats.triangle_count += output_primitive.indices.size() / 3;
  parOutput.primitives.push_back(std::move(output_primitive));
}

void importNode(const tinygltf::Model& parModel, int parNodeIndex,
                const glm::mat4& parParentTransform,
                const std::filesystem::path& parPath,
                kage::assets::StaticModel& parOutput) {
  if (parNodeIndex < 0 ||
      static_cast<std::size_t>(parNodeIndex) >= parModel.nodes.size()) {
    throw makeImportError(parPath, "node index is out of range");
  }

  const tinygltf::Node& node =
      parModel.nodes[static_cast<std::size_t>(parNodeIndex)];
  const glm::mat4 node_transform = parParentTransform * getNodeMatrix(node);

  if (node.mesh >= 0) {
    if (static_cast<std::size_t>(node.mesh) >= parModel.meshes.size()) {
      throw makeImportError(parPath, "mesh index is out of range");
    }

    const tinygltf::Mesh& mesh =
        parModel.meshes[static_cast<std::size_t>(node.mesh)];
    for (std::size_t primitive_index = 0;
         primitive_index < mesh.primitives.size(); ++primitive_index) {
      const std::string primitive_name =
          node.name.empty() ? mesh.name : node.name;
      importPrimitive(parModel, mesh.primitives[primitive_index],
                      node_transform, primitive_name, parPath, parOutput);
    }
  }

  for (const int child_index : node.children) {
    importNode(parModel, child_index, node_transform, parPath, parOutput);
  }
}

}  // namespace

namespace kage::assets {

void AssetBounds::includePoint(const glm::vec3& parPoint) {
  if (!is_valid) {
    min = parPoint;
    max = parPoint;
    is_valid = true;
    return;
  }

  min = glm::min(min, parPoint);
  max = glm::max(max, parPoint);
}

glm::vec3 AssetBounds::getSize() const {
  if (!is_valid) {
    return glm::vec3(0.0f);
  }

  return max - min;
}

StaticModel GltfAssetLoader::loadStaticModel(
    const std::filesystem::path& parPath) const {
  tinygltf::TinyGLTF loader;
  tinygltf::Model gltf_model;
  std::string error;
  std::string warning;

  const bool loaded = loader.LoadBinaryFromFile(
      &gltf_model, &error, &warning, parPath.string());
  if (!loaded) {
    throw makeImportError(parPath, error.empty() ? "unknown loader error"
                                                : error);
  }

  StaticModel output;
  output.source_path = parPath;
  output.import_warning = warning;
  output.stats.scene_count = gltf_model.scenes.size();
  output.stats.node_count = gltf_model.nodes.size();
  output.stats.mesh_count = gltf_model.meshes.size();
  output.stats.material_count = gltf_model.materials.size();
  output.stats.texture_count = gltf_model.textures.size();
  output.stats.image_count = gltf_model.images.size();
  output.stats.skin_count = gltf_model.skins.size();
  output.stats.animation_count = gltf_model.animations.size();

  const int scene_index =
      gltf_model.defaultScene >= 0 ? gltf_model.defaultScene
                                   : DEFAULT_SCENE_INDEX;
  if (scene_index < 0 ||
      static_cast<std::size_t>(scene_index) >= gltf_model.scenes.size()) {
    throw makeImportError(parPath, "scene index is out of range");
  }

  const tinygltf::Scene& scene =
      gltf_model.scenes[static_cast<std::size_t>(scene_index)];
  output.scene_name = scene.name.empty() ? "default" : scene.name;

  for (const int node_index : scene.nodes) {
    importNode(gltf_model, node_index, glm::mat4(1.0f), parPath, output);
  }

  output.stats.primitive_count = output.primitives.size();
  return output;
}

}  // namespace kage::assets
