/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/import.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <GL/glew.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif

#include <assimp/color4.h>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>
#include <assimp/Logger.hpp>

#include "src/anim.h"
#include "src/bonemesh.h"
#include "src/image.h"
#include "src/log.h"
#include "src/mesh.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/ubermaterial.h"
#include "src/ubershader.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const char* shading_models[0xc] = {
  "Unknown",
  "Flat",
  "Gouraud",
  "Phong",
  "Blinn",
  "Toon",
  "OrenNayar",
  "Minnaert",
  "CookTorrance",
  "Unlit",
  "Fresnel",
  "PBR"
};

// FBX TimeMode enum values (matches Assimp's FileGlobalSettings::FrameRate)
enum FbxTimeMode {
  kFbxTimeMode_Default = 0,
  kFbxTimeMode_120 = 1,
  kFbxTimeMode_100 = 2,
  kFbxTimeMode_60 = 3,
  kFbxTimeMode_50 = 4,
  kFbxTimeMode_48 = 5,
  kFbxTimeMode_30 = 6,
  kFbxTimeMode_30_Drop = 7,
  kFbxTimeMode_NtscDropFrame = 8,
  kFbxTimeMode_NtscFullFrame = 9,
  kFbxTimeMode_Pal = 10,
  kFbxTimeMode_Cinema = 11,
  kFbxTimeMode_1000 = 12,
  kFbxTimeMode_CinemaNd = 13,
  kFbxTimeMode_Custom = 14,
};

void ToGlmMat4(const aiMatrix4x4& from, glm::mat4& to) {
  to[0][0] = from.a1;
  to[1][0] = from.a2;
  to[2][0] = from.a3;
  to[3][0] = from.a4;
  to[0][1] = from.b1;
  to[1][1] = from.b2;
  to[2][1] = from.b3;
  to[3][1] = from.b4;
  to[0][2] = from.c1;
  to[1][2] = from.c2;
  to[2][2] = from.c3;
  to[3][2] = from.c4;
  to[0][3] = from.d1;
  to[1][3] = from.d2;
  to[2][3] = from.d3;
  to[3][3] = from.d4;
}

class MyLogStream : public Assimp::LogStream {
 public:
  // Write something using your own functionality
  void write(const char* message) {
    Log::E("%s\n", message);
  }
};

MyLogStream myLogStream;

static void PrintNode(const aiNode* node, const std::string& indent) {
  assert(node);
  Log::D("%snode \"%s\"\n", indent.c_str(), node->mName.C_Str());

  std::string newIndent = indent + "  ";
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    PrintNode(node->mChildren[i], newIndent);
  }
}

static std::shared_ptr<Image> BuildTextureImage(const aiTexture* texture) {
  assert(texture);
  auto img = std::make_shared<Image>();
  img->filename = texture->mFilename.C_Str();
  if (texture->mHeight == 0) {
    assert(texture->pcData);
    // this is a compressed texture, try loading it.
    if (!img->LoadBytes(reinterpret_cast<uint8_t*>(texture->pcData),
                       texture->mWidth)) {
      Log::E("Failed to load compressed texture \"%s\" format = %s\n",
             texture->mFilename.C_Str(), texture->achFormatHint);
    }
    Log::D("Success loading compressed texture \"%s\" format = %s, (%d, %d)\n",
           texture->mFilename.C_Str(), texture->achFormatHint, img->width,
           img->height);
  } else {
    img->width = texture->mWidth;
    img->height = texture->mHeight;
    img->pixel_format = PixelFormat::RGBA;
    img->data.resize(img->width * img->height * 4);
    // swizzle bgra to rgba
    for (size_t i = 0; i < img->width * img->height; i++) {
      img->data[i * 4 + 0] = texture->pcData[i].r;
      img->data[i * 4 + 1] = texture->pcData[i].g;
      img->data[i * 4 + 2] = texture->pcData[i].b;
      img->data[i * 4 + 3] = texture->pcData[i].a;
    }
  }
  return img;
}

struct TextureInfo {
  TextureInfo()
      : texture(),
        uv_index(0),
        has_uv_transform(false),
        uv_offset(0.0f, 0.0f),
        uv_scale(0.0f, 0.0f),
        uv_rotation(0.0f) {}
  std::shared_ptr<Texture> texture;
  int uv_index;
  bool has_uv_transform;
  glm::vec2 uv_offset;
  glm::vec2 uv_scale;
  float uv_rotation;
};

WrapType ConvertToWrapType(aiTextureMapMode map_mode) {
  switch (map_mode) {
    default:
    case aiTextureMapMode_Wrap:
      return WrapType::Repeat;
    case aiTextureMapMode_Clamp:
    case aiTextureMapMode_Decal:
      return WrapType::ClampToEdge;
    case aiTextureMapMode_Mirror:
      return WrapType::Repeat;
  }
}

static TextureInfo LoadTextureInfoFromMat(const aiMaterial* material, aiTextureType texture_type,
                                          const std::vector<std::shared_ptr<Image>>& image_vec,
                                          const std::string& asset_filename) {
  assert(material);

  TextureInfo result;
  material->Get(AI_MATKEY_UVWSRC(texture_type, 0), result.uv_index);

  aiUVTransform uvTransform;
  unsigned int max = sizeof(aiUVTransform) / sizeof(ai_real);
  if (aiGetMaterialFloatArray(material, AI_MATKEY_UVTRANSFORM(texture_type, 0),
                              reinterpret_cast<float*>(&uvTransform), &max) == AI_SUCCESS) {
    result.has_uv_transform = true;
    result.uv_offset.x = uvTransform.mTranslation.x;
    result.uv_offset.y = uvTransform.mTranslation.y;
    result.uv_scale.x = uvTransform.mScaling.x;
    result.uv_scale.y = uvTransform.mScaling.y;
    result.uv_rotation = uvTransform.mRotation;
  }

  if (material->GetTextureCount(texture_type) > 0) {
    bool found_image = false;
    aiString path;
    aiTextureMapping mapping;
    aiTextureMapMode map_mode[2];
    unsigned int uv_index;
    float blend;
    aiTextureOp op;

    if (material->GetTexture(texture_type, 0, &path, &mapping, &uv_index, &blend, &op, map_mode) > 0) {
      Log::W("Failed to find texture_type %d in material \"%s\"\n", texture_type, material->GetName().C_Str());
    } else {
      Texture::Params tex_params = {
        FilterType::LinearMipmapLinear,
        FilterType::Linear,
        ConvertToWrapType(map_mode[0]),
        ConvertToWrapType(map_mode[1])
      };
      if (path.data[0] == '*') {  // embedded
        int idx = std::atoi(path.data + 1);
        result.texture = std::make_shared<Texture>(*image_vec[idx], tex_params);
        found_image = true;
      } else {
        for (auto& img : image_vec) {
          if (img->filename == path.data) {
            result.texture = std::make_shared<Texture>(*img, tex_params);
            found_image = true;
          }
        }
        if (!found_image) {
          // Search for texture in asset_filename path.
          std::string asset_path = asset_filename.substr(0, asset_filename.find_last_of("/\\"));
          Image img;
          if (img.Load(asset_path + "/" + path.C_Str())) {
            result.texture = std::make_shared<Texture>(img, tex_params);
            found_image = true;
          }
        }
      }
    }

    if (!found_image) {
      Log::W("Failed to find texture (%d) \"%s\" in material \"%s\", fallback to checkerboard.\n",
             texture_type, path.C_Str(), material->GetName().C_Str());
      Image img;
      if (!img.Load("texture/checkerboard.png")) {
        Log::E("Error loading checkerboard.png\n");
        return result;
      }
      img.is_srgb = false;  // isFramebufferSRGBEnabledIn;
      Texture::Params tex_params = {
        FilterType::LinearMipmapLinear,
        FilterType::Linear,
        WrapType::Repeat,
        WrapType::Repeat
      };
      result.texture = std::make_shared<Texture>(img, tex_params);
    }
  }
  return result;
}

static std::shared_ptr<UberMaterial> BuildPbrMaterial(
    UberShaderCache& shader_cache,
    const aiMaterial* material,
    bool has_bones,
    bool has_vertex_colors,
    const std::vector<std::shared_ptr<Image>>& image_vec,
    const std::string& asset_filename) {
  std::string mat_name = material->GetName().C_Str();
  auto base_color_tex_info = LoadTextureInfoFromMat(material, aiTextureType_BASE_COLOR, image_vec, asset_filename);
  auto emissive_color_tex_info = LoadTextureInfoFromMat(material, aiTextureType_EMISSIVE, image_vec, asset_filename);
  auto metallic_roughness_tex_info = LoadTextureInfoFromMat(material, aiTextureType_GLTF_METALLIC_ROUGHNESS,
                                                              image_vec, asset_filename);
  UberShaderVariantKey key = 0;
  if (base_color_tex_info.texture) {
    key |= UberShaderVariantFlags::HAS_BASE_TEXTURE;
    if (base_color_tex_info.uv_index == 0) {
      key |= UberShaderVariantFlags::HAS_UV0;
    } else if (base_color_tex_info.uv_index == 1) {
      key |= UberShaderVariantFlags::HAS_UV1;
    } else {
      Log::W("base_texture using more then two texcoords (%d)\n", base_color_tex_info.uv_index);
    }
    if (base_color_tex_info.has_uv_transform) {
      key |= UberShaderVariantFlags::HAS_BASE_TEXTURE_UV_TRANSFORM;
    }
  }
  if (emissive_color_tex_info.texture) {
    key |= UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE;
    if (emissive_color_tex_info.uv_index == 0) {
      key |= UberShaderVariantFlags::HAS_UV0;
    } else if (emissive_color_tex_info.uv_index == 1) {
      key |= UberShaderVariantFlags::HAS_UV1;
    } else {
      Log::W("emissive_texture using more then two texcoords (%d)\n", emissive_color_tex_info.uv_index);
    }
    if (emissive_color_tex_info.has_uv_transform) {
      key |= UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE_UV_TRANSFORM;
    }
  }
  if (metallic_roughness_tex_info.texture) {
    key |= UberShaderVariantFlags::HAS_METALLIC_ROUGHNESS_TEXTURE;
    if (metallic_roughness_tex_info.uv_index == 0) {
      key |= UberShaderVariantFlags::HAS_UV0;
    } else if (emissive_color_tex_info.uv_index == 1) {
      key |= UberShaderVariantFlags::HAS_UV1;
    } else {
      Log::W("metallic_roughness_texture using more then two texcoords (%d)\n", metallic_roughness_tex_info.uv_index);
    }
    if (metallic_roughness_tex_info.has_uv_transform) {
      key |= UberShaderVariantFlags::HAS_METALLIC_ROUGHNESS_TEXTURE_UV_TRANSFORM;
    }
  }
  if (has_bones) {
    key |= UberShaderVariantFlags::HAS_BONES;
  }
  if (has_vertex_colors) {
    key |= UberShaderVariantFlags::HAS_VERTEX_COLORS;
  }

  auto prog = shader_cache.GetOrCreate(key);
  auto mat = std::make_shared<UberMaterial>(mat_name, prog, key);

  float opacity = 1.0f;
  material->Get(AI_MATKEY_OPACITY, opacity);

  aiColor3D base_color_factor(1.0f, 1.0f, 1.0f);
  material->Get(AI_MATKEY_BASE_COLOR, base_color_factor);
  glm::vec4 v4(base_color_factor.r, base_color_factor.g, base_color_factor.b, opacity);
  mat->SetBaseColorFactor(v4);

  if (base_color_tex_info.texture) {
    mat->SetBaseColorTexture(base_color_tex_info.texture);
    mat->SetBaseColorUvIndex(base_color_tex_info.uv_index);
    if (base_color_tex_info.has_uv_transform) {
      mat->SetBaseColorUvOffset(base_color_tex_info.uv_offset);
      mat->SetBaseColorUvScale(base_color_tex_info.uv_scale);
      mat->SetBaseColorUvRotation(base_color_tex_info.uv_rotation);
    }
  }

  if (emissive_color_tex_info.texture) {
    mat->SetEmissiveColorTexture(emissive_color_tex_info.texture);
    mat->SetEmissiveColorUvIndex(emissive_color_tex_info.uv_index);
    if (emissive_color_tex_info.has_uv_transform) {
      mat->SetEmissiveColorUvOffset(emissive_color_tex_info.uv_offset);
      mat->SetEmissiveColorUvScale(emissive_color_tex_info.uv_scale);
      mat->SetEmissiveColorUvRotation(emissive_color_tex_info.uv_rotation);
    }
  }

  float metallic_factor(0.0f);
  material->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor);
  mat->SetMetallicFactor(metallic_factor);

  float roughness_factor(1.0f);
  material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor);
  mat->SetRoughnessFactor(roughness_factor);

  if (metallic_roughness_tex_info.texture) {
    mat->SetMetallicRoughnessTexture(metallic_roughness_tex_info.texture);
    mat->SetMetallicRoughnessUvIndex(metallic_roughness_tex_info.uv_index);
    if (metallic_roughness_tex_info.has_uv_transform) {
      mat->SetMetallicRoughnessUvOffset(metallic_roughness_tex_info.uv_offset);
      mat->SetMetallicRoughnessUvScale(metallic_roughness_tex_info.uv_scale);
      mat->SetMetallicRoughnessUvRotation(metallic_roughness_tex_info.uv_rotation);
    }
  }

  aiColor3D emissive_color_factor(0.0f, 0.0f, 0.0f);
  material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_color_factor);
  glm::vec3 v3(emissive_color_factor.r, emissive_color_factor.g, emissive_color_factor.b);
  mat->SetEmissiveColorFactor(v3);

  return mat;
}

static std::shared_ptr<UberMaterial> BuildDefaultMaterial(
    UberShaderCache& shader_cache,
    const aiMaterial* material,
    bool has_bones,
    bool has_vertex_colors,
    const std::vector<std::shared_ptr<Image>>& image_vec,
    const std::string& asset_filename) {
  std::string mat_name = material->GetName().C_Str();
  UberShaderVariantKey key = 0;
  if (has_bones) {
    key |= UberShaderVariantFlags::HAS_BONES;
  }
  if (has_vertex_colors) {
    key |= UberShaderVariantFlags::HAS_VERTEX_COLORS;
  }
  auto prog = shader_cache.GetOrCreate(key);
  auto mat = std::make_shared<UberMaterial>(mat_name, prog, key);

  mat->SetBaseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
  mat->SetEmissiveColorFactor(glm::vec3(1.0f, 1.0f, 1.0f));

  return mat;
}

static std::shared_ptr<UberMaterial> BuildMaterial(
    UberShaderCache& shader_cache,
    const aiMaterial* material,
    bool has_bones,
    bool has_vertex_colors,
    const std::vector<std::shared_ptr<Image>>& image_vec,
    const std::string& asset_filename) {
  assert(material);

  int32_t shading_model = -1;
  material->Get(AI_MATKEY_SHADING_MODEL, &shading_model, nullptr);

  switch (shading_model) {
    case aiShadingMode_PBR_BRDF:
      return BuildPbrMaterial(shader_cache, material, has_bones, has_vertex_colors, image_vec, asset_filename);
    default: {
      const char* shading_model_str = "????";
      if (shading_model >= 0 && shading_model <= aiShadingMode_PBR_BRDF) {
        shading_model_str = shading_models[shading_model];
      }
      Log::W("unsupported shading model %s (%d)\n", shading_model_str, shading_model);
      return BuildDefaultMaterial(shader_cache, material, has_bones, has_vertex_colors, image_vec, asset_filename);
    }
  }
}

static std::shared_ptr<Node> BuildNodeTree(
    const aiScene* scene,
    const aiNode* ai_node,
    std::map<std::string, std::shared_ptr<Node>>& name_to_node_map,
    std::map<uint32_t, std::shared_ptr<Node>>& mesh_index_to_node_map,
    std::vector<std::shared_ptr<Node>>& node_vec) {
  assert(ai_node);

  std::string name = ai_node->mName.C_Str();
  glm::mat4 xform;
  ToGlmMat4(ai_node->mTransformation, xform);
  std::shared_ptr<Node> parent;
  if (ai_node->mParent) {
    std::string parent_name = ai_node->mParent->mName.C_Str();
    auto iter = name_to_node_map.find(parent_name);
    if (iter != name_to_node_map.end()) {
      parent = iter->second;
    }
  }
  auto node = std::make_shared<Node>(node_vec.size(), name, parent, xform);
  auto iter = name_to_node_map.find(name);
  node_vec.push_back(node);
  if (iter != name_to_node_map.end()) {
    Log::W("duplicate node name \"%s\" detected.\n", name.c_str());
  }

  name_to_node_map[name] = node;

  for (uint32_t i = 0; i < ai_node->mNumMeshes; i++) {
    mesh_index_to_node_map[ai_node->mMeshes[i]] = node;
  }

  for (uint32_t i = 0; i < ai_node->mNumChildren; i++) {
    auto child = BuildNodeTree(scene, ai_node->mChildren[i], name_to_node_map,
                               mesh_index_to_node_map, node_vec);
    node->child_vec().push_back(child);
  }
  return node;
}

class MeshBuffers {
 public:
  MeshBuffers(std::shared_ptr<BufferObject> posBufferIn,
              std::shared_ptr<BufferObject> uvBufferIn,
              std::shared_ptr<BufferObject> normBufferIn,
              std::shared_ptr<BufferObject> colorBufferIn,
              std::shared_ptr<BufferObject> indexBufferIn) :
      posBuffer(posBufferIn),
      uvBuffer(uvBufferIn),
      normBuffer(normBufferIn),
      colorBuffer(colorBufferIn),
      indexBuffer(indexBufferIn) {}

  std::shared_ptr<BufferObject> posBuffer;
  std::shared_ptr<BufferObject> uvBuffer;
  std::shared_ptr<BufferObject> normBuffer;
  std::shared_ptr<BufferObject> colorBuffer;
  std::shared_ptr<BufferObject> indexBuffer;
};

static std::shared_ptr<MeshBuffers> ImportMeshBuffers(const aiMesh* mesh) {
  assert(mesh->HasPositions() && mesh->HasNormals());

  std::vector<glm::vec3> posVec;
  std::vector<glm::vec2> uvVec;
  std::vector<glm::vec3> normVec;
  std::vector<glm::vec4> colorVec;
  posVec.reserve(mesh->mNumVertices);
  uvVec.reserve(mesh->mNumVertices);
  normVec.reserve(mesh->mNumVertices);
  colorVec.reserve(mesh->mNumVertices);

  for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
    const aiVector3D& v = mesh->mVertices[i];
    posVec.push_back(glm::vec3(v.x, v.y, v.z));

    if (mesh->HasTextureCoords(0)) {
      const aiVector3D& uv = mesh->mTextureCoords[0][i];
      uvVec.push_back(glm::vec2(uv.x, uv.y));
    } else {
      uvVec.push_back(glm::vec2(0.0f, 0.0f));
    }

    const aiVector3D& n = mesh->mNormals[i];
    normVec.push_back(glm::vec3(n.x, n.y, n.z));

    if (mesh->HasVertexColors(0)) {
      const aiColor4D& c = mesh->mColors[0][i];
      colorVec.push_back(glm::vec4(c.r, c.g, c.b, c.a));
    }
  }

  std::vector<uint32_t> indexVec;
  indexVec.reserve(mesh->mNumFaces * 3);
  for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
    const aiFace& f = mesh->mFaces[i];
    assert(f.mNumIndices == 3);
    for (uint32_t j = 0; j < f.mNumIndices; j++) {
      indexVec.push_back(f.mIndices[j]);
    }
  }

  auto posBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, posVec);
  auto uvBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uvVec);
  auto normBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, normVec);
  std::shared_ptr<BufferObject> colorBuffer;
  if (mesh->HasVertexColors(0)) {
    colorBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, colorVec);
  }
  auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec);

  return std::make_shared<MeshBuffers>(posBuffer, uvBuffer, normBuffer, colorBuffer, indexBuffer);
}

static std::shared_ptr<Mesh> BuildMesh(
    UberShaderCache& shader_cache,
    const aiMesh* ai_mesh,
    const aiMaterial* ai_mat,
    std::shared_ptr<Node> node,
    std::shared_ptr<MeshBuffers> buffers,
    const std::vector<std::shared_ptr<Image>>& image_vec,
    const std::string& asset_filename) {
  auto mat = BuildMaterial(shader_cache, ai_mat, ai_mesh->HasBones(), ai_mesh->HasVertexColors(0),
                           image_vec, asset_filename);

  // setup vertex array object with buffers
  auto vao = std::make_shared<VertexArrayObject>();
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("position"), buffers->posBuffer);
  if (mat->HasBaseColorTexture() || mat->HasEmissiveColorTexture()) {
    vao->SetAttribBuffer(mat->prog()->GetAttribLoc("uv0"), buffers->uvBuffer);
  }
  if (ai_mesh->HasVertexColors(0) && buffers->colorBuffer) {
    vao->SetAttribBuffer(mat->prog()->GetAttribLoc("color"), buffers->colorBuffer);
  }
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("normal"), buffers->normBuffer);
  vao->SetElementBuffer(buffers->indexBuffer);
  return std::make_shared<Mesh>(vao, mat, node);
}

static std::shared_ptr<Mesh> BuildBoneMesh(
    UberShaderCache& shader_cache,
    const aiMesh* ai_mesh,
    const aiMaterial* ai_mat,
    std::shared_ptr<Node> node,
    std::shared_ptr<MeshBuffers> buffers,
    const std::vector<std::shared_ptr<Image>>& image_vec,
    const std::string& asset_filename) {
  assert(node);
  assert(ai_mesh->HasBones());
  assert(AI_LMW_MAX_WEIGHTS == 4);

  auto mat = BuildMaterial(shader_cache, ai_mat, ai_mesh->HasBones(), ai_mesh->HasVertexColors(0),
                           image_vec, asset_filename);

  // the order of mBones array is NOT the same as the order of the nodes.
  // we use node_vec and name_to_idx_map to help re-order the indices.
  std::vector<std::shared_ptr<Node>> node_vec;
  node->BuildDepthFirstNodeVec(node_vec);
  std::map<std::string, int32_t> name_to_idx_map;
  for (size_t i = 0; i < node_vec.size(); i++) {
    const std::string& name = node_vec[i]->name();
    auto iter = name_to_idx_map.find(name);
    if (iter != name_to_idx_map.end()) {
      Log::W("duplicate bone name \"%s\" detected\n", name.c_str());
    }
    name_to_idx_map[name] = static_cast<int32_t>(i);
  }

  std::vector<glm::vec4> bone_weights_vec(ai_mesh->mNumVertices, glm::vec4(0.0f));
  std::vector<glm::vec4> bone_indices_vec(ai_mesh->mNumVertices, glm::vec4(0.0f));
  std::vector<int> bone_count_vec(ai_mesh->mNumVertices, 0);
  std::vector<glm::mat4> inv_bind_pose_vec(node_vec.size(), glm::mat4(1.0f));

  for (uint32_t i = 0; i < ai_mesh->mNumBones; i++) {
    const aiBone* ai_bone = ai_mesh->mBones[i];
    std::string bone_name = ai_bone->mName.C_Str();
    // the order of mBones might not be the same as the order of the nodes.
    // use bone_idx instead of i.
    auto iter = name_to_idx_map.find(bone_name);
    int32_t node_idx = -1;
    if (iter != name_to_idx_map.end()) {
      node_idx = iter->second;
    }
    if (node_idx >= 0) {
      // copy the offset matrix into the invBindPoseVec.
      ToGlmMat4(ai_bone->mOffsetMatrix, inv_bind_pose_vec[node_idx]);
    } else {
      Log::W("could not find node_idx for \"%s\", bound to %d verts\n",
             bone_name.c_str(), ai_mesh->mBones[i]->mNumWeights);
    }
    // copy and update the boneWeightsVec and boneIndicesVecs
    // to match bone_idx.
    for (uint32_t j = 0; j < ai_mesh->mBones[i]->mNumWeights; j++) {
      float weight = ai_bone->mWeights[j].mWeight;
      int vertex_idx = ai_bone->mWeights[j].mVertexId;
      int k = bone_count_vec[vertex_idx];
      assert(k <= 4);
      if (node_idx >= 0) {
        bone_weights_vec[vertex_idx][k] = weight;
        bone_indices_vec[vertex_idx][k] = static_cast<float>(node_idx);
      } else {
        // this bone has no associated node, so map it to the root joint.
        bone_weights_vec[vertex_idx][k] = weight;
        bone_indices_vec[vertex_idx][k] = static_cast<float>(0);
      }
      bone_count_vec[vertex_idx]++;
    }
  }

  auto boneWeightsBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, bone_weights_vec);
  auto boneIndicesBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, bone_indices_vec);

  // setup vertex array object with buffers
  auto vao = std::make_shared<VertexArrayObject>();
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("position"), buffers->posBuffer);
  if (mat->HasBaseColorTexture() || mat->HasEmissiveColorTexture()) {
    vao->SetAttribBuffer(mat->prog()->GetAttribLoc("uv0"), buffers->uvBuffer);
  }
  if (ai_mesh->HasVertexColors(0) && buffers->colorBuffer) {
    vao->SetAttribBuffer(mat->prog()->GetAttribLoc("color"), buffers->colorBuffer);
  }
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("normal"), buffers->normBuffer);
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("boneWeights"), boneWeightsBuffer);
  vao->SetAttribBuffer(mat->prog()->GetAttribLoc("boneIndices"), boneIndicesBuffer);
  vao->SetElementBuffer(buffers->indexBuffer);

  auto boneMesh = std::make_shared<BoneMesh>(vao, mat, node, inv_bind_pose_vec);
  return boneMesh;
}


// Convert FBX TimeMode to frame rate (matches Assimp's FrameRateToDouble)
static double TimeModeToFrameRate(int time_mode, double custom_fps) {
  switch (time_mode) {
    case kFbxTimeMode_120: return 120.0;
    case kFbxTimeMode_100: return 100.0;
    case kFbxTimeMode_60: return 60.0;
    case kFbxTimeMode_50: return 50.0;
    case kFbxTimeMode_48: return 48.0;
    case kFbxTimeMode_30:
    case kFbxTimeMode_30_Drop: return 30.0;
    case kFbxTimeMode_NtscDropFrame:
    case kFbxTimeMode_NtscFullFrame: return 29.9700262;
    case kFbxTimeMode_Pal: return 25.0;
    case kFbxTimeMode_Cinema: return 24.0;
    case kFbxTimeMode_1000: return 1000.0;
    case kFbxTimeMode_CinemaNd: return 23.976;
    case kFbxTimeMode_Custom: return custom_fps;
    default: return 0.0;
  }
}

// Try to get the sample rate from scene metadata
// Returns the rate if found, or 0.0 if not available.
static double GetSampleRateFromMetadata(const aiScene* scene) {
  if (!scene || !scene->mMetaData) {
    return 0.0;
  }

  // FBX files store FrameRate as TimeMode (int) and CustomFrameRate (float)
  int frame_rate_mode = 0;
  float custom_frame_rate = 0.0f;  // Assimp stores this as float, not double

  scene->mMetaData->Get("FrameRate", frame_rate_mode);
  scene->mMetaData->Get("CustomFrameRate", custom_frame_rate);

  double fps = TimeModeToFrameRate(frame_rate_mode,
                                   static_cast<double>(custom_frame_rate));
  if (fps > 0.0) {
    return fps;
  }

  return 0.0;
}

// Detect the native sample rate of an animation by analyzing keyframe times.
// Returns a rate clamped to [min_rate, max_rate].
static double DetectAnimationSampleRate(const std::string& filename,
                                        const aiScene* scene,
                                        const aiAnimation* anim,
                                        double min_rate, double max_rate) {
  if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".bvh") {
    // BVH files only support constant frame rates,
    // so ticks per second is frames per second!
    double sample_rate = anim->mTicksPerSecond;
    return std::clamp(min_rate, sample_rate, max_rate);
  } else if (filename.size() > 4 &&
             filename.substr(filename.size() - 4) == ".fbx") {
    // for fbx determine frame rate from metadata.
    double sample_rate = GetSampleRateFromMetadata(scene);
    if (sample_rate != 0.0) {
      return sample_rate;
    }
  }

  // try to detect sample_rate by analyzing tracks.
  double ticks_per_sec = anim->mTicksPerSecond;

  // Calculate minimum meaningful delta. Any delta smaller than this would
  // imply a sample rate higher than max_rate, and is likely an interpolation
  // artifact from Assimp's format conversion (especially Euler-to-quaternion).
  // Apply 1% tolerance to handle floating point precision issues.
  double min_meaningful_delta = ticks_per_sec / max_rate * 0.99;

  // Find the smallest delta that represents a real frame boundary.
  double min_delta = std::numeric_limits<double>::max();

  for (uint32_t c = 0; c < anim->mNumChannels; c++) {
    const aiNodeAnim* channel = anim->mChannels[c];

    // Check position keys
    for (uint32_t k = 1; k < channel->mNumPositionKeys; k++) {
      double delta = channel->mPositionKeys[k].mTime -
                     channel->mPositionKeys[k - 1].mTime;
      if (delta > 0.0 && delta >= min_meaningful_delta) {
        min_delta = std::min(min_delta, delta);
      }
    }

    // Check rotation keys
    for (uint32_t k = 1; k < channel->mNumRotationKeys; k++) {
      double delta = channel->mRotationKeys[k].mTime -
                     channel->mRotationKeys[k - 1].mTime;
      if (delta > 0.0 && delta >= min_meaningful_delta) {
        min_delta = std::min(min_delta, delta);
      }
    }
  }

  // If we found valid deltas, compute rate; otherwise use min_rate
  if (min_delta < std::numeric_limits<double>::max()) {
    double detected_rate = ticks_per_sec / min_delta;

    if (filename.size() > 4 && (
            filename.substr(filename.size() - 4) == ".glb" ||
            filename.substr(filename.size() - 4) == ".gltf" ||
            filename.substr(filename.size() - 4) == ".vrm")) {
      // Snap to nearest integer to handle floating point precision issues
      // in glTF and other formats.
      detected_rate = std::round(detected_rate);
    }
    return std::clamp(detected_rate, min_rate, max_rate);
  }

  return min_rate;
}

std::shared_ptr<Anim> BuildAnim(std::shared_ptr<const Asset> asset,
                                const aiAnimation* aiAnim, double sample_rate) {
  size_t num_nodes = aiAnim->mNumChannels;
  double anim_length = (aiAnim->mDuration / aiAnim->mTicksPerSecond);  // sec
  size_t num_frames;
  if (anim_length == 0) {
    num_frames = 1;
  } else {
    num_frames = static_cast<size_t>(glm::ceil(anim_length * sample_rate));
  }

  // Log::D("    num_frames = %zu\n", num_frames);
  // Log::D("    num_nodes = %zu\n", num_nodes);
  auto anim = std::make_shared<Anim>(std::string(aiAnim->mName.C_Str()),
                                     num_frames, num_nodes,
                                     static_cast<float>(sample_rate));
  for (uint32_t j = 0; j < aiAnim->mNumChannels; j++) {
    const aiNodeAnim* channel = aiAnim->mChannels[j];
    anim->SetJointName(j, channel->mNodeName.C_Str());
    auto node = asset->FindNode(channel->mNodeName.C_Str());
    glm::mat4 rest_mat;
    if (node) {
      rest_mat = node->rel_xform();
    }
    glm::vec3 rest_scale;
    glm::quat rest_rot;
    glm::vec3 rest_pos;
    Decompose(rest_mat, &rest_scale, &rest_rot, &rest_pos);

    // Sample the animation at a fixed frame rate and store in the Anim object.
    uint32_t pos_i = 0;
    uint32_t rot_i = 0;
    uint32_t scale_i = 0;
    double t = 0.0;
    const double dt = aiAnim->mTicksPerSecond / sample_rate;

    for (size_t i = 0; i < num_frames; i++) {
      // Interpolate position
      glm::vec3 pos = rest_pos;
      if (channel->mNumPositionKeys > 0) {
        uint32_t next_pos_i = std::min(pos_i + 1,
                                       channel->mNumPositionKeys - 1);
        while (t >= channel->mPositionKeys[next_pos_i].mTime) {
          pos_i = next_pos_i;
          next_pos_i = std::min(pos_i + 1, channel->mNumPositionKeys - 1);
          if (next_pos_i == pos_i) {
            break;
          }
        }

        if (pos_i == next_pos_i) {
          // Use the single key value
          const aiVector3D& v = channel->mPositionKeys[pos_i].mValue;
          pos = glm::vec3(v.x, v.y, v.z);
        } else {
          // Interpolate between keys
          double alpha = (t - channel->mPositionKeys[pos_i].mTime) /
              (channel->mPositionKeys[next_pos_i].mTime -
               channel->mPositionKeys[pos_i].mTime);
          const aiVector3D& v0 = channel->mPositionKeys[pos_i].mValue;
          const aiVector3D& v1 = channel->mPositionKeys[next_pos_i].mValue;
          glm::vec3 p0(v0.x, v0.y, v0.z);
          glm::vec3 p1(v1.x, v1.y, v1.z);
          pos = glm::mix(p0, p1, static_cast<float>(alpha));
        }
      }

      // Interpolate rotation
      glm::quat rot = rest_rot;
      if (channel->mNumRotationKeys > 0) {
        uint32_t next_rot_i = std::min(rot_i + 1,
                                       channel->mNumRotationKeys - 1);
        while (t >= channel->mRotationKeys[next_rot_i].mTime) {
          rot_i = next_rot_i;
          next_rot_i = std::min(rot_i + 1, channel->mNumRotationKeys - 1);
          if (next_rot_i == rot_i) {
            break;
          }
        }

        if (rot_i == next_rot_i) {
          // Use the single key value
          const aiQuaternion& q = channel->mRotationKeys[rot_i].mValue;
          rot = glm::quat(q.w, q.x, q.y, q.z);
        } else {
          // Interpolate between keys using slerp
          double alpha = (t - channel->mRotationKeys[rot_i].mTime) /
              (channel->mRotationKeys[next_rot_i].mTime -
               channel->mRotationKeys[rot_i].mTime);
          const aiQuaternion& q0 = channel->mRotationKeys[rot_i].mValue;
          const aiQuaternion& q1 = channel->mRotationKeys[next_rot_i].mValue;
          glm::quat r0(q0.w, q0.x, q0.y, q0.z);
          glm::quat r1(q1.w, q1.x, q1.y, q1.z);
          rot = glm::slerp(r0, r1, static_cast<float>(alpha));
        }
      }

      // Interpolate scale
      glm::vec3 scale = rest_scale;
      if (channel->mNumScalingKeys > 0) {
        uint32_t next_scale_i = std::min(scale_i + 1,
                                         channel->mNumScalingKeys - 1);
        while (t >= channel->mScalingKeys[next_scale_i].mTime) {
          scale_i = next_scale_i;
          next_scale_i = std::min(scale_i + 1, channel->mNumScalingKeys - 1);
          if (next_scale_i == scale_i) {
            break;
          }
        }

        if (scale_i == next_scale_i) {
          // Use the single key value
          const aiVector3D& s = channel->mScalingKeys[scale_i].mValue;
          scale = glm::vec3(s.x, s.y, s.z);
        } else {
          // Interpolate between keys
          double alpha = (t - channel->mScalingKeys[scale_i].mTime) /
              (channel->mScalingKeys[next_scale_i].mTime -
               channel->mScalingKeys[scale_i].mTime);
          const aiVector3D& s0 = channel->mScalingKeys[scale_i].mValue;
          const aiVector3D& s1 = channel->mScalingKeys[next_scale_i].mValue;
          glm::vec3 sc0(s0.x, s0.y, s0.z);
          glm::vec3 sc1(s1.x, s1.y, s1.z);
          scale = glm::mix(sc0, sc1, static_cast<float>(alpha));
        }
      }

      // Compose the transformation matrix from position, rotation, and scale
      glm::mat4 trans_mat = glm::translate(glm::mat4(1.0f), pos);
      glm::mat4 rot_mat = glm::mat4_cast(rot);
      glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);
      glm::mat4 transform = trans_mat * rot_mat * scale_mat;

      // Set the key in the animation
      anim->SetKey(i, j, transform);
      t += dt;
    }
  }

  // Log::D("anim->name = \"%s\"\n", anim->name().c_str());
  // Log::D("anim->num_frames = %zu\n", anim->num_frames());
  // Log::D("anim->num_joints = %zu\n", anim->num_joints());

  return anim;
}

std::shared_ptr<Asset> AssetImport(const std::string& filename) {
  return AssetImportAbs(FindFile(filename));
}

std::shared_ptr<Asset> AssetImportAbs(const std::string& filename) {
  std::shared_ptr<Asset> asset = std::make_shared<Asset>();
  const uint32_t severityFlags = (Assimp::Logger::Debugging |
                                  Assimp::Logger::Info |
                                  Assimp::Logger::Err |
                                  Assimp::Logger::Warn);
  Assimp::DefaultLogger::get()->attachStream(&myLogStream, severityFlags);

  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
  uint32_t flags = (aiProcess_Triangulate | aiProcess_GenNormals |
                    aiProcess_GenUVCoords | aiProcess_PopulateArmatureData |
                    aiProcess_LimitBoneWeights | aiProcess_GlobalScale |
                    aiProcess_ValidateDataStructure);
  const aiScene* scene = importer.ReadFile(filename, flags);

  if (!scene) {
    Log::E("Failed read \"%s\"\n", filename.c_str());
    return asset;
  }

  if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
    Log::W("Incomplete scene \"%s\"\n", filename.c_str());
  }

  // PrintNode(scene->mRootNode, "");

  std::map<uint32_t, std::shared_ptr<Node>> mesh_index_to_node_map;
  asset->root_node = BuildNodeTree(scene, scene->mRootNode,
                                   asset->string_to_node_map,
                                   mesh_index_to_node_map, asset->node_vec);
  asset->root_node->Update();  // update all the abs xforms in the tree.
  Log::D("num nodes = %zu\n", asset->string_to_node_map.size());
  Log::D("num meshes = %d\n", scene->mNumMeshes);
  Log::D("num skeletons = %d\n", scene->mNumSkeletons);
  Log::D("num animations = %d\n", scene->mNumAnimations);
  Log::D("num materials = %d\n", scene->mNumMaterials);
  Log::D("num textures = %d\n", scene->mNumTextures);

  std::vector<std::shared_ptr<Image>> image_vec;
  image_vec.reserve(scene->mNumTextures);
  for (uint32_t i = 0; i < scene->mNumTextures; i++) {
    image_vec.push_back(BuildTextureImage(scene->mTextures[i]));
  }

  UberShaderCache& shader_cache = UberShaderCache::Get();

  asset->mesh_vec.reserve(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
    const aiMesh* mesh = scene->mMeshes[i];
    if (mesh->HasPositions() && mesh->HasNormals()) {
      auto buffers = ImportMeshBuffers(mesh);
      aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
      if (mesh->HasBones()) {
        aiNode* armature = mesh->mBones[0]->mArmature;
        if (!armature) {
          Log::E("mesh \"%s\"has no armature", mesh->mName.C_Str());
          continue;
        }
        auto node = asset->FindNode(armature->mName.C_Str());
        if (!node) {
          Log::E("could not find armature \"%s\" in asset map!\n", armature->mName.C_Str());
          continue;
        }
        Log::I("loading mesh %s -> %s...\n", mesh->mName.C_Str(), armature->mName.C_Str());
        asset->mesh_vec.push_back(BuildBoneMesh(shader_cache, mesh, mat, node, buffers, image_vec, filename));
      } else {
        auto iter = mesh_index_to_node_map.find(i);
        if (iter == mesh_index_to_node_map.end()) {
          Log::E("could not find mesh (%d) \"%s\" in node map!\n", i, mesh->mName.C_Str());
          continue;
        }
        asset->mesh_vec.push_back(BuildMesh(shader_cache, mesh, mat, iter->second, buffers, image_vec, filename));
      }
    } else {
      Log::W("mesh \"%s\" skipped, HasPositions = %s, HasNormals = %s\n",
             mesh->mName.C_Str(),
             mesh->HasPositions() ? "true" : "false",
             mesh->HasNormals() ? "true" : "false");
    }
  }

  asset->anim_vec.reserve(scene->mNumAnimations);

  for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
    const aiAnimation* aiAnim = scene->mAnimations[i];

    static const double kMinSampleRate = 10.0;
    static const double kMaxSampleRate = 240.0;
    double sample_rate = DetectAnimationSampleRate(filename, scene, aiAnim,
                                                   kMinSampleRate,
                                                   kMaxSampleRate);

    Log::D("  anim[%d]\n", i);
    Log::D("    mName = %s\n", aiAnim->mName.C_Str());
    Log::D("    mDuration = %f\n", (float)aiAnim->mDuration);
    Log::D("    mTicksPerSecond = %f\n", (float)aiAnim->mTicksPerSecond);
    Log::D("    mNumChannels = %u\n", aiAnim->mNumChannels);
    Log::D("    DetectAnimationSampleRate() = %.3f\n", sample_rate);

    auto anim = BuildAnim(asset, aiAnim, sample_rate);
    asset->anim_vec.push_back(anim);
  }

  // by convention bvh files are in cm, so scale into meters.
  /*
  std::filesystem::path path(filename);
  if (path.extension() == ".bvh") {
    float kCmToM = 0.01f;
    glm::mat4 scale_mat = MakeMat4(kCmToM, glm::quat(), glm::vec3(0.0f));
    asset->root_node->set_rel_xform(scale_mat * asset->root_node->rel_xform());
  }
  */

  return asset;
}
}  // namespace hyper
