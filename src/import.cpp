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

#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
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
#include "src/util.h"
#include "src/vertexbuffer.h"

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

static std::shared_ptr<Material> BuildPbrMaterial(const aiMaterial* material) {
  int32_t shadingMode = -1;
  material->Get(AI_MATKEY_SHADING_MODEL, &shadingMode, nullptr);
  assert(shadingMode == aiShadingMode_PBR_BRDF);

  auto mat = std::make_shared<Material>();
  mat->name = material->GetName().C_Str();

  // #define AI_MATKEY_USE_COLOR_MAP "$mat.useColorMap", 0, 0

  // albedo
  // aiTextureType_BASE_COLOR = 12,
  // #define AI_MATKEY_BASE_COLOR "$clr.base", 0, 0
  // #define AI_MATKEY_BASE_COLOR_TEXTURE aiTextureType_BASE_COLOR, 0

  // aiTextureType_NORMAL_CAMERA = 13,
  // aiTextureType_EMISSION_COLOR = 14,
  // aiTextureType_METALNESS = 15,
  // aiTextureType_DIFFUSE_ROUGHNESS = 16,
  // aiTextureType_AMBIENT_OCCLUSION = 17,

  // metallic vs glossiness

  return mat;
}

static std::shared_ptr<Material> BuildMaterial(const aiMaterial* material) {
  assert(material);
  // AI_MATKEY_SHADING_MODEL

  int32_t shadingMode = -1;
  material->Get(AI_MATKEY_SHADING_MODEL, &shadingMode, nullptr);
  switch (shadingMode) {
    case aiShadingMode_Flat:
      Log::W("Flat shading unsupported\n");
      return nullptr;
    case aiShadingMode_Gouraud:
      Log::W("Gouraud shading unsupported\n");
      return nullptr;
    case aiShadingMode_Phong:
      Log::W("Phong shading unsupported\n");
      return nullptr;
    case aiShadingMode_Blinn:
      Log::W("Blinn shading unsupported\n");
      return nullptr;
    case aiShadingMode_Toon:
      Log::W("Toon shading unsupported\n");
      return nullptr;
    case aiShadingMode_OrenNayar:
      Log::W("orenNayar shading unsupported\n");
      return nullptr;
    case aiShadingMode_NoShading:
      Log::W("No shading unsupported\n");
      return nullptr;
    case aiShadingMode_Fresnel:
      Log::W("Fresnel shading unsupported\n");
      return nullptr;
    default:
      Log::W("Unknown shading mode\n");
      return nullptr;
    case aiShadingMode_PBR_BRDF:
      Log::W("PBR shading unsupported\n");
      return BuildPbrMaterial(material);
  }
}

static std::shared_ptr<Node> BuildNodeTree(
    const aiNode* ai_node,
    std::map<std::string, std::shared_ptr<Node>>& name_to_node_map,
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
  for (uint32_t i = 0; i < ai_node->mNumChildren; i++) {
    auto child = BuildNodeTree(ai_node->mChildren[i], name_to_node_map,
                               node_vec);
    node->child_vec().push_back(child);
  }
  return node;
}

struct MeshBuffers {
  MeshBuffers(std::shared_ptr<BufferObject> posBufferIn,
              std::shared_ptr<BufferObject> uvBufferIn,
              std::shared_ptr<BufferObject> normBufferIn,
              std::shared_ptr<BufferObject> indexBufferIn) :
      posBuffer(posBufferIn),
      uvBuffer(uvBufferIn),
      normBuffer(normBufferIn),
      indexBuffer(indexBufferIn) {}

  std::shared_ptr<BufferObject> posBuffer;
  std::shared_ptr<BufferObject> uvBuffer;
  std::shared_ptr<BufferObject> normBuffer;
  std::shared_ptr<BufferObject> indexBuffer;
};

static std::shared_ptr<MeshBuffers> ImportMeshBuffers(const aiMesh* mesh) {
  assert(mesh->HasPositions() &&
         mesh->HasTextureCoords(0) &&
         mesh->HasNormals());

  std::vector<glm::vec3> posVec;
  std::vector<glm::vec2> uvVec;
  std::vector<glm::vec3> normVec;
  posVec.reserve(mesh->mNumVertices);
  uvVec.reserve(mesh->mNumVertices);
  for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
    const aiVector3D& v = mesh->mVertices[i];
    posVec.push_back(glm::vec3(v.x, v.y, v.z));

    const aiVector3D& uv = mesh->mTextureCoords[0][i];
    uvVec.push_back(glm::vec2(uv.x, uv.y));

    const aiVector3D& n = mesh->mNormals[i];
    normVec.push_back(glm::vec3(n.x, n.y, n.z));
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
  auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER,
                                                    indexVec);

  return std::make_shared<MeshBuffers>(posBuffer, uvBuffer, normBuffer,
                                       indexBuffer);
}

std::shared_ptr<Mesh> BuildMesh(std::shared_ptr<Node> node,
                                std::shared_ptr<MeshBuffers> buffers) {
  // AJT(TODO) maybe create an actual material?!?
  auto prog = std::make_shared<Program>();
  if (!prog->LoadVertFrag("shader/mesh_vert.glsl", "shader/mesh_frag.glsl")) {
    Log::E("Error loading mesh shader!\n");
    return nullptr;
  }

  // AJT(TODO) use the texture from the mesh?!?
  Image img;
  if (!img.Load("texture/checkerboard.png")) {
    Log::E("Error loading checkerboard.png\n");
    return nullptr;
  }
  img.is_srgb = false;  // isFramebufferSRGBEnabledIn;
  Texture::Params texParams = {FilterType::LinearMipmapLinear,
                               FilterType::Linear,
                               WrapType::Repeat,
                               WrapType::Repeat};
  auto tex = std::make_shared<Texture>(img, texParams);

  // setup vertex array object with buffers
  auto vao = std::make_shared<VertexArrayObject>();
  vao->SetAttribBuffer(prog->GetAttribLoc("position"), buffers->posBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("uv"), buffers->uvBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("normal"), buffers->normBuffer);
  vao->SetElementBuffer(buffers->indexBuffer);
  return std::make_shared<Mesh>(vao, prog, tex, node);
}

std::shared_ptr<Mesh> BuildBoneMesh(const aiMesh* ai_mesh,
                                    std::shared_ptr<Node> node,
                                    std::shared_ptr<MeshBuffers> buffers) {
  assert(node);
  assert(ai_mesh->HasBones());
  assert(AI_LMW_MAX_WEIGHTS == 4);

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

  std::vector<glm::vec4> bone_weights_vec(ai_mesh->mNumVertices,
                                          glm::vec4(0.0f));
  std::vector<glm::vec4> bone_indices_vec(ai_mesh->mNumVertices,
                                          glm::vec4(0.0f));
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

  auto boneWeightsBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER,
                                                          bone_weights_vec);
  auto boneIndicesBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER,
                                                          bone_indices_vec);

  // AJT(TODO): maybe create an actual material?!?
  auto prog = std::make_shared<Program>();
  if (!prog->LoadVertFrag("shader/bone_mesh_vert.glsl",
                          "shader/bone_mesh_frag.glsl")) {
    Log::E("Error loading mesh shader!\n");
    return nullptr;
  }

  // AJT(TODO): use the texture from the mesh?!?
  Image img;
  if (!img.Load("texture/checkerboard.png")) {
    Log::E("Error loading checkerboard.png\n");
    return nullptr;
  }
  img.is_srgb = false;  // isFramebufferSRGBEnabledIn;
  Texture::Params texParams = {FilterType::LinearMipmapLinear,
                               FilterType::Linear,
                               WrapType::Repeat,
                               WrapType::Repeat};
  auto tex = std::make_shared<Texture>(img, texParams);

  // setup vertex array object with buffers
  auto vao = std::make_shared<VertexArrayObject>();
  vao->SetAttribBuffer(prog->GetAttribLoc("position"), buffers->posBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("uv"), buffers->uvBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("normal"), buffers->normBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("boneWeights"), boneWeightsBuffer);
  vao->SetAttribBuffer(prog->GetAttribLoc("boneIndices"), boneIndicesBuffer);
  vao->SetElementBuffer(buffers->indexBuffer);

  auto boneMesh = std::make_shared<BoneMesh>(vao, prog, tex, node,
                                             inv_bind_pose_vec);
  return boneMesh;
}

std::shared_ptr<Anim> BuildAnim(std::shared_ptr<const Asset> asset,
                                const aiAnimation* aiAnim, double sample_rate) {
  // AJT: TODO: HACK: don't have time to do a full animation import
  // just want the first frame for now.
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
  return AssetImportAbs(GetRootPath() + filename);
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
  asset->root_node = BuildNodeTree(scene->mRootNode, asset->string_to_node_map,
                                   asset->node_vec);
  Log::D("num nodes = %zu\n", asset->string_to_node_map.size());
  Log::D("num meshes = %d\n", scene->mNumMeshes);
  Log::D("num skeletons = %d\n", scene->mNumSkeletons);
  Log::D("num animations = %d\n", scene->mNumAnimations);

  asset->mesh_vec.reserve(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
    const aiMesh* mesh = scene->mMeshes[i];
    if (mesh->HasPositions() && mesh->HasTextureCoords(0) &&
        mesh->HasNormals()) {
      auto mat = BuildMaterial(scene->mMaterials[mesh->mMaterialIndex]);
      auto buffers = ImportMeshBuffers(mesh);
      aiNode* ai_node = mesh->mBones[0]->mNode;
      auto node = asset->FindNode(ai_node->mName.C_Str());
      if (!node) {
        Log::E("could not find \"%s\" in map!\n", ai_node->mName.C_Str());
      }
      Log::I("loading mesh %s -> %s...\n", mesh->mName.C_Str(),
             ai_node->mName.C_Str());
      if (mesh->HasBones()) {
        asset->mesh_vec.push_back(BuildBoneMesh(mesh, node, buffers));
      } else {
        asset->mesh_vec.push_back(BuildMesh(node, buffers));
      }
    }
  }

  asset->anim_vec.reserve(scene->mNumAnimations);

  const double kSampleRate = 20.0f;  // frames per second
  for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
    const aiAnimation* aiAnim = scene->mAnimations[i];

    Log::D("  anim[%d]\n", i);
    Log::D("    mName = %s\n", aiAnim->mName.C_Str());
    Log::D("    mDuration = %f\n", (float)aiAnim->mDuration);
    Log::D("    mTicksPerSecond = %f\n", (float)aiAnim->mTicksPerSecond);
    Log::D("    mNumChannels = %u\n", aiAnim->mNumChannels);

    auto anim = BuildAnim(asset, aiAnim, kSampleRate);
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
