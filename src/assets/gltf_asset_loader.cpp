#include "assets/gltf_asset_loader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int DEFAULT_SCENE_INDEX = 0;

[[nodiscard]] std::runtime_error makeImportError(
    const std::filesystem::path& parPath, const std::string& parMessage) {
  return std::runtime_error("Failed to import " + parPath.string() + ": " +
                            parMessage);
}

[[nodiscard]] bool isGitLfsPointer(const std::filesystem::path& parPath) {
  std::ifstream input(parPath, std::ios::binary);
  if (!input) {
    return false;
  }

  std::array<char, 43> header{};
  input.read(header.data(), static_cast<std::streamsize>(header.size()));
  constexpr std::string_view LFS_HEADER =
      "version https://git-lfs.github.com/spec/v1";
  return std::string_view(header.data(),
                          static_cast<std::size_t>(input.gcount()))
      .starts_with(LFS_HEADER);
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

[[nodiscard]] bool rangeFits(std::size_t parOffset, std::size_t parByteSize,
                             std::size_t parContainerSize) {
  return parOffset <= parContainerSize &&
         parByteSize <= parContainerSize - parOffset;
}

[[nodiscard]] std::size_t getComponentByteSize(
    int parComponentType, const std::filesystem::path& parPath) {
  switch (parComponentType) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return 2;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return 8;
    default:
      throw makeImportError(parPath, "unsupported accessor component type");
  }
}

[[nodiscard]] std::size_t getAccessorComponentCount(
    int parType, const std::filesystem::path& parPath) {
  switch (parType) {
    case TINYGLTF_TYPE_SCALAR:
      return 1;
    case TINYGLTF_TYPE_VEC2:
      return 2;
    case TINYGLTF_TYPE_VEC3:
      return 3;
    case TINYGLTF_TYPE_VEC4:
      return 4;
    case TINYGLTF_TYPE_MAT2:
      return 4;
    case TINYGLTF_TYPE_MAT3:
      return 9;
    case TINYGLTF_TYPE_MAT4:
      return 16;
    default:
      throw makeImportError(parPath, "unsupported accessor value type");
  }
}

[[nodiscard]] std::size_t getAccessorElementByteSize(
    const tinygltf::Accessor& parAccessor,
    const std::filesystem::path& parPath) {
  return getComponentByteSize(parAccessor.componentType, parPath) *
         getAccessorComponentCount(parAccessor.type, parPath);
}

[[nodiscard]] const unsigned char* getAccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parElementIndex >= parAccessor.count) {
    throw makeImportError(parPath, "accessor element index is out of range");
  }

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
  if (!rangeFits(buffer_view.byteOffset, buffer_view.byteLength,
                 buffer.data.size())) {
    throw makeImportError(parPath, "buffer view byte range is out of range");
  }

  const std::size_t element_byte_size =
      getAccessorElementByteSize(parAccessor, parPath);
  const std::size_t byte_stride_size = static_cast<std::size_t>(byte_stride);
  if (byte_stride_size < element_byte_size) {
    throw makeImportError(parPath, "accessor byte stride is too small");
  }

  if (parElementIndex >
      (std::numeric_limits<std::size_t>::max() - parAccessor.byteOffset) /
          byte_stride_size) {
    throw makeImportError(parPath, "accessor byte offset overflow");
  }

  const std::size_t byte_offset_in_view =
      parAccessor.byteOffset + byte_stride_size * parElementIndex;
  if (!rangeFits(byte_offset_in_view, element_byte_size,
                 buffer_view.byteLength)) {
    throw makeImportError(parPath, "accessor element range is out of range");
  }

  const std::size_t byte_offset =
      buffer_view.byteOffset + byte_offset_in_view;
  if (!rangeFits(byte_offset, element_byte_size, buffer.data.size())) {
    throw makeImportError(parPath, "accessor byte range is out of range");
  }

  return buffer.data.data() + byte_offset;
}

void validateAttributeCount(const tinygltf::Accessor& parAccessor,
                            std::size_t parExpectedCount,
                            std::string_view parAttributeName,
                            const std::filesystem::path& parPath) {
  if (parAccessor.count != parExpectedCount) {
    throw makeImportError(parPath, std::string(parAttributeName) +
                                       " accessor count does not match "
                                       "POSITION");
  }
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

[[nodiscard]] glm::vec4 readVec4AccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      parAccessor.type != TINYGLTF_TYPE_VEC4) {
    throw makeImportError(parPath, "expected a float vec4 accessor");
  }

  glm::vec4 value{};
  std::memcpy(glm::value_ptr(value),
              getAccessorElement(parModel, parAccessor, parElementIndex,
                                 parPath),
              sizeof(float) * 4);
  return value;
}

[[nodiscard]] float readFloatScalarAccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      parAccessor.type != TINYGLTF_TYPE_SCALAR) {
    throw makeImportError(parPath, "expected a float scalar accessor");
  }

  float value = 0.0f;
  std::memcpy(&value,
              getAccessorElement(parModel, parAccessor, parElementIndex,
                                 parPath),
              sizeof(float));
  return value;
}

[[nodiscard]] glm::mat4 readMat4AccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
      parAccessor.type != TINYGLTF_TYPE_MAT4) {
    throw makeImportError(parPath, "expected a float mat4 accessor");
  }

  glm::mat4 value{1.0f};
  std::memcpy(glm::value_ptr(value),
              getAccessorElement(parModel, parAccessor, parElementIndex,
                                 parPath),
              sizeof(float) * 16);
  return value;
}

[[nodiscard]] glm::uvec4 readJointAccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.type != TINYGLTF_TYPE_VEC4) {
    throw makeImportError(parPath, "expected a vec4 joint accessor");
  }

  const unsigned char* element =
      getAccessorElement(parModel, parAccessor, parElementIndex, parPath);

  switch (parAccessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return glm::uvec4(element[0], element[1], element[2], element[3]);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      glm::uvec4 value{};
      std::uint16_t components[4]{};
      std::memcpy(components, element, sizeof(components));
      value.x = components[0];
      value.y = components[1];
      value.z = components[2];
      value.w = components[3];
      return value;
    }
    default:
      throw makeImportError(parPath, "unsupported joint component type");
  }
}

[[nodiscard]] glm::vec4 readWeightAccessorElement(
    const tinygltf::Model& parModel, const tinygltf::Accessor& parAccessor,
    std::size_t parElementIndex, const std::filesystem::path& parPath) {
  if (parAccessor.type != TINYGLTF_TYPE_VEC4) {
    throw makeImportError(parPath, "expected a vec4 weight accessor");
  }

  const unsigned char* element =
      getAccessorElement(parModel, parAccessor, parElementIndex, parPath);
  glm::vec4 weights{};

  switch (parAccessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      std::memcpy(glm::value_ptr(weights), element, sizeof(float) * 4);
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      weights = glm::vec4(element[0], element[1], element[2], element[3]) /
                255.0f;
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      std::uint16_t components[4]{};
      std::memcpy(components, element, sizeof(components));
      weights = glm::vec4(components[0], components[1], components[2],
                          components[3]) /
                65535.0f;
      break;
    }
    default:
      throw makeImportError(parPath, "unsupported weight component type");
  }

  const float weight_sum = weights.x + weights.y + weights.z + weights.w;
  if (weight_sum > 0.0f) {
    weights /= weight_sum;
  }

  return weights;
}

[[nodiscard]] std::vector<std::uint32_t> readIndices(
    const tinygltf::Model& parModel, const tinygltf::Primitive& parPrimitive,
    std::size_t parVertexCount, const std::filesystem::path& parPath) {
  if (parVertexCount >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    throw makeImportError(parPath, "primitive has too many vertices");
  }

  std::vector<std::uint32_t> indices;
  if (parPrimitive.indices < 0) {
    indices.resize(parVertexCount);
    for (std::size_t index = 0; index < parVertexCount; ++index) {
      indices[index] = static_cast<std::uint32_t>(index);
    }
  } else {
    const tinygltf::Accessor& accessor =
        getAccessor(parModel, parPrimitive.indices, parPath);
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
      throw makeImportError(parPath, "expected scalar index accessor");
    }

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
  }

  if (indices.empty()) {
    throw makeImportError(parPath, "primitive has no triangle indices");
  }

  if (indices.size() % 3 != 0) {
    throw makeImportError(parPath, "triangle index count is not divisible by 3");
  }

  for (const std::uint32_t index : indices) {
    if (static_cast<std::size_t>(index) >= parVertexCount) {
      throw makeImportError(parPath, "index value is out of vertex range");
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

[[nodiscard]] kage::math::Transform getNodeTransform(
    const tinygltf::Node& parNode) {
  kage::math::Transform transform;

  if (parNode.translation.size() == 3) {
    transform.translation =
        glm::vec3(static_cast<float>(parNode.translation[0]),
                  static_cast<float>(parNode.translation[1]),
                  static_cast<float>(parNode.translation[2]));
  }

  if (parNode.rotation.size() == 4) {
    transform.rotation =
        glm::normalize(glm::quat(static_cast<float>(parNode.rotation[3]),
                                 static_cast<float>(parNode.rotation[0]),
                                 static_cast<float>(parNode.rotation[1]),
                                 static_cast<float>(parNode.rotation[2])));
  }

  if (parNode.scale.size() == 3) {
    transform.scale = glm::vec3(static_cast<float>(parNode.scale[0]),
                                static_cast<float>(parNode.scale[1]),
                                static_cast<float>(parNode.scale[2]));
  }

  return transform;
}

[[nodiscard]] float readColorComponent(double parValue) {
  return static_cast<float>(std::clamp(parValue, 0.0, 1.0));
}

[[nodiscard]] std::uint32_t checkedTextureIndex(
    int parTextureIndex, const tinygltf::Model& parModel,
    const std::filesystem::path& parPath) {
  if (parTextureIndex < 0) {
    return kage::assets::INVALID_TEXTURE_INDEX;
  }

  if (static_cast<std::size_t>(parTextureIndex) >= parModel.textures.size()) {
    throw makeImportError(parPath, "texture index is out of range");
  }

  return static_cast<std::uint32_t>(parTextureIndex);
}

[[nodiscard]] float readTinyNumber(const tinygltf::Value& parValue,
                                   float parFallback) {
  return parValue.IsNumber() ? static_cast<float>(parValue.GetNumberAsDouble())
                             : parFallback;
}

[[nodiscard]] glm::vec2 readTinyVec2(const tinygltf::Value& parValue,
                                     const glm::vec2& parFallback) {
  if (!parValue.IsArray()) {
    return parFallback;
  }

  glm::vec2 value = parFallback;
  if (parValue.ArrayLen() > 0) {
    value.x = readTinyNumber(parValue.Get(0), value.x);
  }
  if (parValue.ArrayLen() > 1) {
    value.y = readTinyNumber(parValue.Get(1), value.y);
  }
  return value;
}

[[nodiscard]] kage::assets::TextureTransform readTextureTransform(
    const std::map<std::string, tinygltf::Value>& parExtensions) {
  kage::assets::TextureTransform transform;
  const auto extension = parExtensions.find("KHR_texture_transform");
  if (extension == parExtensions.end() || !extension->second.IsObject()) {
    return transform;
  }

  const tinygltf::Value& value = extension->second;
  transform.offset = readTinyVec2(value.Get("offset"), transform.offset);
  transform.scale = readTinyVec2(value.Get("scale"), transform.scale);
  transform.rotation = readTinyNumber(value.Get("rotation"), transform.rotation);
  return transform;
}

[[nodiscard]] kage::assets::MaterialTextureSlot readTextureSlot(
    const tinygltf::TextureInfo& parTextureInfo,
    const tinygltf::Model& parModel, const std::filesystem::path& parPath) {
  kage::assets::MaterialTextureSlot slot;
  if (parTextureInfo.index < 0) {
    return slot;
  }
  if (parTextureInfo.texCoord != 0) {
    throw makeImportError(parPath, "only TEXCOORD_0 material textures are "
                                  "supported");
  }

  slot.texture_index =
      checkedTextureIndex(parTextureInfo.index, parModel, parPath);
  slot.transform = readTextureTransform(parTextureInfo.extensions);
  return slot;
}

[[nodiscard]] kage::assets::MaterialTextureSlot readNormalTextureSlot(
    const tinygltf::NormalTextureInfo& parTextureInfo,
    const tinygltf::Model& parModel, const std::filesystem::path& parPath) {
  kage::assets::MaterialTextureSlot slot;
  if (parTextureInfo.index < 0) {
    return slot;
  }
  if (parTextureInfo.texCoord != 0) {
    throw makeImportError(parPath, "only TEXCOORD_0 normal textures are "
                                  "supported");
  }

  slot.texture_index =
      checkedTextureIndex(parTextureInfo.index, parModel, parPath);
  slot.transform = readTextureTransform(parTextureInfo.extensions);
  return slot;
}

void importImages(const tinygltf::Model& parModel,
                  const std::filesystem::path& parPath,
                  kage::assets::StaticModel& parOutput) {
  parOutput.images.reserve(parModel.images.size());

  for (const tinygltf::Image& image : parModel.images) {
    if (image.width <= 0 || image.height <= 0 || image.component <= 0 ||
        image.image.empty()) {
      throw makeImportError(parPath, "image data is incomplete");
    }

    kage::assets::StaticImage output_image;
    output_image.name = image.name;
    output_image.pixels = image.image;
    output_image.width = image.width;
    output_image.height = image.height;
    output_image.component_count = image.component;
    output_image.pixel_type = image.pixel_type;
    parOutput.images.push_back(std::move(output_image));
  }
}

void importTextures(const tinygltf::Model& parModel,
                    const std::filesystem::path& parPath,
                    kage::assets::StaticModel& parOutput) {
  parOutput.textures.reserve(parModel.textures.size());

  for (const tinygltf::Texture& texture : parModel.textures) {
    if (texture.source < 0 ||
        static_cast<std::size_t>(texture.source) >= parOutput.images.size()) {
      throw makeImportError(parPath, "texture source image is out of range");
    }

    kage::assets::StaticTexture output_texture;
    output_texture.name = texture.name;
    output_texture.image_index = static_cast<std::uint32_t>(texture.source);

    if (texture.sampler >= 0) {
      if (static_cast<std::size_t>(texture.sampler) >=
          parModel.samplers.size()) {
        throw makeImportError(parPath, "texture sampler is out of range");
      }

      const tinygltf::Sampler& sampler =
          parModel.samplers[static_cast<std::size_t>(texture.sampler)];
      output_texture.min_filter =
          sampler.minFilter > 0
              ? sampler.minFilter
              : kage::assets::DEFAULT_STATIC_TEXTURE_MIN_FILTER;
      output_texture.mag_filter =
          sampler.magFilter > 0
              ? sampler.magFilter
              : kage::assets::DEFAULT_STATIC_TEXTURE_MAG_FILTER;
      output_texture.wrap_s =
          sampler.wrapS > 0 ? sampler.wrapS
                            : kage::assets::DEFAULT_STATIC_TEXTURE_WRAP;
      output_texture.wrap_t =
          sampler.wrapT > 0 ? sampler.wrapT
                            : kage::assets::DEFAULT_STATIC_TEXTURE_WRAP;
    }

    parOutput.textures.push_back(output_texture);
  }
}

void importMaterials(const tinygltf::Model& parModel,
                     const std::filesystem::path& parPath,
                     kage::assets::StaticModel& parOutput) {
  parOutput.materials.reserve(parModel.materials.size());

  for (const tinygltf::Material& material : parModel.materials) {
    kage::assets::StaticMaterial output_material;
    output_material.name = material.name;

    const std::vector<double>& base_color_factor =
        material.pbrMetallicRoughness.baseColorFactor;
    if (base_color_factor.size() == 4) {
      output_material.base_color_factor =
          glm::vec4(readColorComponent(base_color_factor[0]),
                    readColorComponent(base_color_factor[1]),
                    readColorComponent(base_color_factor[2]),
                    readColorComponent(base_color_factor[3]));
    }
    output_material.metallic_factor = static_cast<float>(
        std::clamp(material.pbrMetallicRoughness.metallicFactor, 0.0, 1.0));
    output_material.roughness_factor = static_cast<float>(
        std::clamp(material.pbrMetallicRoughness.roughnessFactor, 0.0, 1.0));
    output_material.alpha_cutoff =
        static_cast<float>(std::max(material.alphaCutoff, 0.0));
    output_material.alpha_blend = material.alphaMode == "BLEND";
    output_material.alpha_mask = material.alphaMode == "MASK";
    output_material.double_sided = material.doubleSided;

    output_material.base_color_texture = readTextureSlot(
        material.pbrMetallicRoughness.baseColorTexture, parModel, parPath);
    output_material.metallic_roughness_texture =
        readTextureSlot(material.pbrMetallicRoughness.metallicRoughnessTexture,
                        parModel, parPath);
    output_material.normal_texture =
        readNormalTextureSlot(material.normalTexture, parModel, parPath);
    output_material.normal_scale =
        static_cast<float>(std::max(material.normalTexture.scale, 0.0));
    output_material.emissive_texture =
        readTextureSlot(material.emissiveTexture, parModel, parPath);
    if (material.emissiveFactor.size() == 3) {
      output_material.emissive_factor =
          glm::vec3(readColorComponent(material.emissiveFactor[0]),
                    readColorComponent(material.emissiveFactor[1]),
                    readColorComponent(material.emissiveFactor[2]));
    }

    parOutput.materials.push_back(output_material);
  }
}

void generateTangents(kage::assets::StaticPrimitive& parPrimitive) {
  std::vector<glm::vec3> tangents(parPrimitive.vertices.size(), glm::vec3(0.0f));

  for (std::size_t index = 0; index + 2 < parPrimitive.indices.size();
       index += 3) {
    const std::uint32_t i0 = parPrimitive.indices[index];
    const std::uint32_t i1 = parPrimitive.indices[index + 1];
    const std::uint32_t i2 = parPrimitive.indices[index + 2];
    if (i0 >= parPrimitive.vertices.size() || i1 >= parPrimitive.vertices.size() ||
        i2 >= parPrimitive.vertices.size()) {
      continue;
    }

    const kage::assets::StaticVertex& v0 = parPrimitive.vertices[i0];
    const kage::assets::StaticVertex& v1 = parPrimitive.vertices[i1];
    const kage::assets::StaticVertex& v2 = parPrimitive.vertices[i2];
    const glm::vec3 edge1 = v1.position - v0.position;
    const glm::vec3 edge2 = v2.position - v0.position;
    const glm::vec2 uv1 = v1.tex_coord - v0.tex_coord;
    const glm::vec2 uv2 = v2.tex_coord - v0.tex_coord;
    const float determinant = uv1.x * uv2.y - uv2.x * uv1.y;
    if (std::abs(determinant) <= 0.000001f) {
      continue;
    }

    const glm::vec3 tangent =
        (edge1 * uv2.y - edge2 * uv1.y) / determinant;
    tangents[i0] += tangent;
    tangents[i1] += tangent;
    tangents[i2] += tangent;
  }

  for (std::size_t vertex_index = 0; vertex_index < parPrimitive.vertices.size();
       ++vertex_index) {
    kage::assets::StaticVertex& vertex = parPrimitive.vertices[vertex_index];
    glm::vec3 tangent = tangents[vertex_index];
    if (glm::length(tangent) <= 0.000001f) {
      tangent = glm::abs(vertex.normal.y) < 0.99f
                    ? glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f),
                                                vertex.normal))
                    : glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
      tangent = glm::normalize(tangent - vertex.normal *
                                             glm::dot(vertex.normal, tangent));
    }

    vertex.tangent = glm::vec4(tangent, 1.0f);
  }
}

void importPrimitive(const tinygltf::Model& parModel,
                     const tinygltf::Primitive& parPrimitive,
                     const glm::mat4& parNodeTransform,
                     std::uint32_t parNodeIndex,
                     std::uint32_t parSkinIndex,
                     const std::string& parName,
                     const std::filesystem::path& parPath,
                     kage::assets::StaticModel& parOutput) {
  const int primitive_mode =
      parPrimitive.mode >= 0 ? parPrimitive.mode : TINYGLTF_MODE_TRIANGLES;
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
  const tinygltf::Accessor* tangent_accessor = nullptr;
  const tinygltf::Accessor* tex_coord_accessor = nullptr;
  const tinygltf::Accessor* joint_accessor = nullptr;
  const tinygltf::Accessor* weight_accessor = nullptr;
  if (position_accessor.count == 0) {
    throw makeImportError(parPath, "primitive has no vertices");
  }

  const auto normal_attribute = parPrimitive.attributes.find("NORMAL");
  if (normal_attribute != parPrimitive.attributes.end()) {
    normal_accessor = &getAccessor(parModel, normal_attribute->second, parPath);
    validateAttributeCount(*normal_accessor, position_accessor.count, "NORMAL",
                           parPath);
  }

  const auto tangent_attribute = parPrimitive.attributes.find("TANGENT");
  if (tangent_attribute != parPrimitive.attributes.end()) {
    tangent_accessor =
        &getAccessor(parModel, tangent_attribute->second, parPath);
    validateAttributeCount(*tangent_accessor, position_accessor.count,
                           "TANGENT", parPath);
  }

  const auto tex_coord_attribute = parPrimitive.attributes.find("TEXCOORD_0");
  if (tex_coord_attribute != parPrimitive.attributes.end()) {
    tex_coord_accessor =
        &getAccessor(parModel, tex_coord_attribute->second, parPath);
    validateAttributeCount(*tex_coord_accessor, position_accessor.count,
                           "TEXCOORD_0", parPath);
  }

  const auto joint_attribute = parPrimitive.attributes.find("JOINTS_0");
  const auto weight_attribute = parPrimitive.attributes.find("WEIGHTS_0");
  if (joint_attribute != parPrimitive.attributes.end() ||
      weight_attribute != parPrimitive.attributes.end()) {
    if (joint_attribute == parPrimitive.attributes.end() ||
        weight_attribute == parPrimitive.attributes.end()) {
      throw makeImportError(parPath,
                            "skinned primitive needs both JOINTS_0 and "
                            "WEIGHTS_0");
    }

    joint_accessor = &getAccessor(parModel, joint_attribute->second, parPath);
    weight_accessor = &getAccessor(parModel, weight_attribute->second, parPath);
    validateAttributeCount(*joint_accessor, position_accessor.count, "JOINTS_0",
                           parPath);
    validateAttributeCount(*weight_accessor, position_accessor.count,
                           "WEIGHTS_0", parPath);
  }

  kage::assets::StaticPrimitive output_primitive;
  output_primitive.name = parName;
  output_primitive.transform = parNodeTransform;
  output_primitive.node_index = parNodeIndex;
  output_primitive.skin_index = parSkinIndex;
  if (parPrimitive.material >= 0) {
    if (static_cast<std::size_t>(parPrimitive.material) >=
        parOutput.materials.size()) {
      throw makeImportError(parPath, "primitive material index is out of range");
    }
    output_primitive.material_index =
        static_cast<std::uint32_t>(parPrimitive.material);
    const kage::assets::StaticMaterial& material =
        parOutput.materials[static_cast<std::size_t>(
            output_primitive.material_index)];
    if ((material.base_color_texture.isValid() ||
         material.normal_texture.isValid() ||
         material.metallic_roughness_texture.isValid() ||
         material.emissive_texture.isValid()) &&
        tex_coord_accessor == nullptr) {
      throw makeImportError(parPath,
                            "textured primitive has no TEXCOORD_0 attribute");
    }
  }
  output_primitive.vertices.reserve(position_accessor.count);
  if (joint_accessor != nullptr && weight_accessor != nullptr) {
    output_primitive.skin_influences.reserve(position_accessor.count);
  }

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
    if (tangent_accessor != nullptr) {
      vertex.tangent =
          readVec4AccessorElement(parModel, *tangent_accessor, vertex_index,
                                  parPath);
    }
    if (tex_coord_accessor != nullptr) {
      vertex.tex_coord =
          readVec2AccessorElement(parModel, *tex_coord_accessor, vertex_index,
                                  parPath);
    }

    output_primitive.vertices.push_back(vertex);
    if (joint_accessor != nullptr && weight_accessor != nullptr) {
      kage::assets::SkinInfluence influence;
      influence.joints =
          readJointAccessorElement(parModel, *joint_accessor, vertex_index,
                                   parPath);
      influence.weights =
          readWeightAccessorElement(parModel, *weight_accessor, vertex_index,
                                    parPath);
      output_primitive.skin_influences.push_back(influence);
    }

    const glm::vec3 world_position =
        glm::vec3(parNodeTransform * glm::vec4(vertex.position, 1.0f));
    output_primitive.bounds.includePoint(world_position);
    parOutput.bounds.includePoint(world_position);
  }

  output_primitive.indices = readIndices(parModel, parPrimitive,
                                         output_primitive.vertices.size(),
                                         parPath);
  if (tangent_accessor == nullptr &&
      output_primitive.material_index != kage::assets::INVALID_MATERIAL_INDEX) {
    const kage::assets::StaticMaterial& material =
        parOutput.materials[static_cast<std::size_t>(
            output_primitive.material_index)];
    if (material.normal_texture.isValid()) {
      generateTangents(output_primitive);
    }
  }

  parOutput.stats.vertex_count += output_primitive.vertices.size();
  if (output_primitive.hasSkinInfluences()) {
    parOutput.stats.skinned_vertex_count += output_primitive.vertices.size();
  }
  parOutput.stats.index_count += output_primitive.indices.size();
  parOutput.stats.triangle_count += output_primitive.indices.size() / 3;
  parOutput.primitives.push_back(std::move(output_primitive));
}

void importNodeRecords(const tinygltf::Model& parModel,
                       const std::filesystem::path& parPath,
                       kage::assets::GltfDocument& parOutput) {
  parOutput.nodes.resize(parModel.nodes.size());

  for (std::size_t node_index = 0; node_index < parModel.nodes.size();
       ++node_index) {
    const tinygltf::Node& node = parModel.nodes[node_index];
    kage::assets::GltfNode& output_node = parOutput.nodes[node_index];
    output_node.name = node.name;
    output_node.local_transform = getNodeTransform(node);
    output_node.global_transform = getNodeMatrix(node);
    if (node.mesh >= 0) {
      if (static_cast<std::size_t>(node.mesh) >= parModel.meshes.size()) {
        throw makeImportError(parPath, "mesh index is out of range");
      }
      output_node.mesh_index = static_cast<std::uint32_t>(node.mesh);
    }
    if (node.skin >= 0) {
      if (static_cast<std::size_t>(node.skin) >= parModel.skins.size()) {
        throw makeImportError(parPath, "skin index is out of range");
      }
      output_node.skin_index = static_cast<std::uint32_t>(node.skin);
    }
  }

  for (std::size_t node_index = 0; node_index < parModel.nodes.size();
       ++node_index) {
    const tinygltf::Node& node = parModel.nodes[node_index];
    kage::assets::GltfNode& output_node = parOutput.nodes[node_index];
    output_node.children.reserve(node.children.size());

    for (const int child_index : node.children) {
      if (child_index < 0 ||
          static_cast<std::size_t>(child_index) >= parModel.nodes.size()) {
        throw makeImportError(parPath, "child node index is out of range");
      }

      output_node.children.push_back(static_cast<std::uint32_t>(child_index));
      parOutput.nodes[static_cast<std::size_t>(child_index)].parent_index =
          static_cast<std::uint32_t>(node_index);
    }
  }
}

void importSkins(const tinygltf::Model& parModel,
                 const std::filesystem::path& parPath,
                 kage::assets::GltfDocument& parOutput) {
  parOutput.skins.reserve(parModel.skins.size());

  for (const tinygltf::Skin& skin : parModel.skins) {
    kage::assets::GltfSkin output_skin;
    output_skin.name = skin.name;
    if (skin.skeleton >= 0) {
      if (static_cast<std::size_t>(skin.skeleton) >= parModel.nodes.size()) {
        throw makeImportError(parPath, "skin skeleton root is out of range");
      }
      output_skin.skeleton_root = static_cast<std::uint32_t>(skin.skeleton);
    }

    output_skin.joints.reserve(skin.joints.size());
    for (const int joint_index : skin.joints) {
      if (joint_index < 0 ||
          static_cast<std::size_t>(joint_index) >= parModel.nodes.size()) {
        throw makeImportError(parPath, "skin joint index is out of range");
      }
      output_skin.joints.push_back(static_cast<std::uint32_t>(joint_index));
    }

    output_skin.inverse_bind_matrices.resize(output_skin.joints.size(),
                                             glm::mat4(1.0f));
    if (skin.inverseBindMatrices >= 0) {
      const tinygltf::Accessor& inverse_bind_accessor =
          getAccessor(parModel, skin.inverseBindMatrices, parPath);
      if (inverse_bind_accessor.count != output_skin.joints.size()) {
        throw makeImportError(
            parPath, "inverse bind matrix count does not match skin joints");
      }

      for (std::size_t matrix_index = 0;
           matrix_index < inverse_bind_accessor.count; ++matrix_index) {
        output_skin.inverse_bind_matrices[matrix_index] =
            readMat4AccessorElement(parModel, inverse_bind_accessor,
                                    matrix_index, parPath);
      }
    }

    parOutput.stats.joint_count += output_skin.joints.size();
    parOutput.skins.push_back(std::move(output_skin));
  }
}

void importMarkers(kage::assets::GltfDocument& parOutput) {
  std::vector<bool> joint_nodes(parOutput.nodes.size(), false);
  for (const kage::assets::GltfSkin& skin : parOutput.skins) {
    for (const std::uint32_t joint_index : skin.joints) {
      if (static_cast<std::size_t>(joint_index) < joint_nodes.size()) {
        joint_nodes[static_cast<std::size_t>(joint_index)] = true;
      }
    }
  }

  for (std::size_t node_index = 0; node_index < parOutput.nodes.size();
       ++node_index) {
    const kage::assets::GltfNode& node = parOutput.nodes[node_index];
    if (node.name.empty() || node.mesh_index != kage::assets::INVALID_MESH_INDEX ||
        joint_nodes[node_index]) {
      continue;
    }

    kage::assets::GltfMarker marker;
    marker.name = node.name;
    marker.node_index = static_cast<std::uint32_t>(node_index);
    marker.transform = node.global_transform;
    parOutput.markers.push_back(marker);
  }
}

[[nodiscard]] kage::assets::AnimationInterpolation getInterpolation(
    const tinygltf::AnimationSampler& parSampler) {
  if (parSampler.interpolation == "STEP") {
    return kage::assets::AnimationInterpolation::Step;
  }

  return kage::assets::AnimationInterpolation::Linear;
}

[[nodiscard]] kage::assets::AnimationTargetPath getAnimationTargetPath(
    const std::string& parPath, const std::filesystem::path& parAssetPath) {
  if (parPath == "translation") {
    return kage::assets::AnimationTargetPath::Translation;
  }
  if (parPath == "rotation") {
    return kage::assets::AnimationTargetPath::Rotation;
  }
  if (parPath == "scale") {
    return kage::assets::AnimationTargetPath::Scale;
  }

  throw makeImportError(parAssetPath, "unsupported animation target path");
}

void importAnimations(const tinygltf::Model& parModel,
                      const std::filesystem::path& parPath,
                      kage::assets::GltfDocument& parOutput) {
  parOutput.animation_clips.reserve(parModel.animations.size());

  for (std::size_t animation_index = 0;
       animation_index < parModel.animations.size(); ++animation_index) {
    const tinygltf::Animation& animation = parModel.animations[animation_index];
    kage::assets::AnimationClip clip;
    clip.name = animation.name.empty()
                    ? "Animation " + std::to_string(animation_index)
                    : animation.name;
    clip.samplers.resize(animation.samplers.size());
    clip.channels.reserve(animation.channels.size());

    for (std::size_t sampler_index = 0;
         sampler_index < animation.samplers.size(); ++sampler_index) {
      const tinygltf::AnimationSampler& source_sampler =
          animation.samplers[sampler_index];
      if (source_sampler.input < 0 || source_sampler.output < 0) {
        throw makeImportError(parPath, "animation sampler is incomplete");
      }

      const tinygltf::Accessor& input_accessor =
          getAccessor(parModel, source_sampler.input, parPath);
      kage::assets::AnimationSampler& sampler = clip.samplers[sampler_index];
      sampler.interpolation = getInterpolation(source_sampler);
      sampler.input_times.reserve(input_accessor.count);
      for (std::size_t time_index = 0; time_index < input_accessor.count;
           ++time_index) {
        const float time =
            readFloatScalarAccessorElement(parModel, input_accessor,
                                           time_index, parPath);
        sampler.input_times.push_back(time);
        clip.duration_seconds = std::max(clip.duration_seconds, time);
      }
    }

    for (const tinygltf::AnimationChannel& source_channel :
         animation.channels) {
      if (source_channel.sampler < 0 ||
          static_cast<std::size_t>(source_channel.sampler) >=
              animation.samplers.size()) {
        throw makeImportError(parPath, "animation channel sampler is out of range");
      }
      if (source_channel.target_node < 0 ||
          static_cast<std::size_t>(source_channel.target_node) >=
              parModel.nodes.size()) {
        throw makeImportError(parPath, "animation target node is out of range");
      }

      const std::size_t sampler_index =
          static_cast<std::size_t>(source_channel.sampler);
      const tinygltf::Accessor& output_accessor =
          getAccessor(parModel, animation.samplers[sampler_index].output,
                      parPath);
      kage::assets::AnimationSampler& sampler = clip.samplers[sampler_index];
      if (output_accessor.count != sampler.input_times.size()) {
        throw makeImportError(
            parPath, "animation output count does not match input times");
      }

      kage::assets::AnimationChannel channel;
      channel.target_node =
          static_cast<std::uint32_t>(source_channel.target_node);
      channel.sampler_index = static_cast<std::uint32_t>(sampler_index);
      channel.target_path =
          getAnimationTargetPath(source_channel.target_path, parPath);

      switch (channel.target_path) {
        case kage::assets::AnimationTargetPath::Translation:
          sampler.translations.reserve(output_accessor.count);
          for (std::size_t value_index = 0; value_index < output_accessor.count;
               ++value_index) {
            sampler.translations.push_back(readVec3AccessorElement(
                parModel, output_accessor, value_index, parPath));
          }
          break;
        case kage::assets::AnimationTargetPath::Rotation:
          sampler.rotations.reserve(output_accessor.count);
          for (std::size_t value_index = 0; value_index < output_accessor.count;
               ++value_index) {
            const glm::vec4 rotation = readVec4AccessorElement(
                parModel, output_accessor, value_index, parPath);
            sampler.rotations.push_back(glm::normalize(glm::quat(
                rotation.w, rotation.x, rotation.y, rotation.z)));
          }
          break;
        case kage::assets::AnimationTargetPath::Scale:
          sampler.scales.reserve(output_accessor.count);
          for (std::size_t value_index = 0; value_index < output_accessor.count;
               ++value_index) {
            sampler.scales.push_back(readVec3AccessorElement(
                parModel, output_accessor, value_index, parPath));
          }
          break;
      }

      clip.channels.push_back(channel);
    }

    parOutput.stats.animation_channel_count += clip.channels.size();
    parOutput.animation_clips.push_back(std::move(clip));
  }
}

void importSceneNode(const tinygltf::Model& parModel, int parNodeIndex,
                     const glm::mat4& parParentTransform,
                     const std::filesystem::path& parPath,
                     kage::assets::GltfDocument& parOutput) {
  if (parNodeIndex < 0 ||
      static_cast<std::size_t>(parNodeIndex) >= parModel.nodes.size()) {
    throw makeImportError(parPath, "node index is out of range");
  }

  const tinygltf::Node& node =
      parModel.nodes[static_cast<std::size_t>(parNodeIndex)];
  const glm::mat4 node_transform = parParentTransform * getNodeMatrix(node);
  parOutput.nodes[static_cast<std::size_t>(parNodeIndex)].global_transform =
      node_transform;

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
                      node_transform,
                      static_cast<std::uint32_t>(parNodeIndex),
                      node.skin >= 0
                          ? static_cast<std::uint32_t>(node.skin)
                          : kage::assets::INVALID_SKIN_INDEX,
                      primitive_name, parPath,
                      parOutput.static_model);
    }
  }

  for (const int child_index : node.children) {
    importSceneNode(parModel, child_index, node_transform, parPath, parOutput);
  }
}

}  // namespace

namespace kage::assets {

GltfDocument GltfAssetLoader::loadDocument(
    const std::filesystem::path& parPath) const {
  if (isGitLfsPointer(parPath)) {
    throw makeImportError(
        parPath,
        "file is a Git LFS pointer; install Git LFS and pull the binary asset");
  }

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

  GltfDocument output;
  output.source_path = parPath;
  output.import_warning = warning;
  output.static_model.source_path = parPath;
  output.static_model.import_warning = warning;
  output.stats.scene_count = gltf_model.scenes.size();
  output.stats.node_count = gltf_model.nodes.size();
  output.stats.mesh_count = gltf_model.meshes.size();
  output.stats.material_count = gltf_model.materials.size();
  output.stats.texture_count = gltf_model.textures.size();
  output.stats.image_count = gltf_model.images.size();
  output.stats.skin_count = gltf_model.skins.size();
  output.stats.animation_count = gltf_model.animations.size();

  output.static_model.stats = output.stats;
  importImages(gltf_model, parPath, output.static_model);
  importTextures(gltf_model, parPath, output.static_model);
  importMaterials(gltf_model, parPath, output.static_model);
  importNodeRecords(gltf_model, parPath, output);
  importSkins(gltf_model, parPath, output);

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
  output.static_model.scene_name = output.scene_name;
  output.root_nodes.reserve(scene.nodes.size());

  for (const int node_index : scene.nodes) {
    if (node_index < 0 ||
        static_cast<std::size_t>(node_index) >= gltf_model.nodes.size()) {
      throw makeImportError(parPath, "scene root node index is out of range");
    }
    output.root_nodes.push_back(static_cast<std::uint32_t>(node_index));
    importSceneNode(gltf_model, node_index, glm::mat4(1.0f), parPath, output);
  }

  importMarkers(output);
  importAnimations(gltf_model, parPath, output);

  output.bounds = output.static_model.bounds;
  output.stats.primitive_count = output.static_model.primitives.size();
  output.stats.vertex_count = output.static_model.stats.vertex_count;
  output.stats.skinned_vertex_count =
      output.static_model.stats.skinned_vertex_count;
  output.stats.index_count = output.static_model.stats.index_count;
  output.stats.triangle_count = output.static_model.stats.triangle_count;
  output.stats.marker_count = output.markers.size();
  output.static_model.stats = output.stats;
  return output;
}

StaticModel GltfAssetLoader::loadStaticModel(
    const std::filesystem::path& parPath) const {
  return loadDocument(parPath).static_model;
}

}  // namespace kage::assets
