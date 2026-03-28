/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <string>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/node.h"

namespace hyper {

class UberMaterial;
class BufferObject;
class VertexArrayObject;

class MeshAttribs {
 public:
  std::vector<glm::vec3> pos_vec;
  std::vector<glm::vec2> uv_vec;
  std::vector<glm::vec3> norm_vec;
  std::vector<glm::vec4> color_vec;
  std::vector<uint32_t> index_vec;

  void Clear() {
    pos_vec.clear();
    uv_vec.clear();
    norm_vec.clear();
    color_vec.clear();
    index_vec.clear();
  }

  void Push(const MeshAttribs& mesh_attribs) {
    size_t vertex_start = pos_vec.size();
    pos_vec.insert(pos_vec.end(), mesh_attribs.pos_vec.begin(), mesh_attribs.pos_vec.end());
    uv_vec.insert(uv_vec.end(), mesh_attribs.uv_vec.begin(), mesh_attribs.uv_vec.end());
    norm_vec.insert(norm_vec.end(), mesh_attribs.norm_vec.begin(), mesh_attribs.norm_vec.end());
    color_vec.insert(color_vec.end(), mesh_attribs.color_vec.begin(), mesh_attribs.color_vec.end());
    size_t index_start = index_vec.size();
    index_vec.insert(index_vec.end(), mesh_attribs.index_vec.begin(), mesh_attribs.index_vec.end());
    for (size_t i = index_start; i < index_vec.size(); i++) {
      index_vec[i] += vertex_start;
    }
  }

  static MeshAttribs MakeSphere(glm::vec4 color, glm::vec3 center, float radius, int num_subdivs);
};

class DynMesh {
 public:
  DynMesh(std::shared_ptr<UberMaterial> mat,
          std::shared_ptr<Node> node);

  void Clear();
  void Push(const MeshAttribs& mesh_attribs);

  virtual void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far);

 protected:
  void SetAttribBuffer(int loc, const std::shared_ptr<BufferObject>& attrib_buffer);

  std::shared_ptr<UberMaterial> mat_;
  std::shared_ptr<Node> node_;
  MeshAttribs batched_attribs_;
  bool dirty_;
  std::shared_ptr<BufferObject> pos_buffer_;
  std::shared_ptr<BufferObject> uv_buffer_;
  std::shared_ptr<BufferObject> norm_buffer_;
  std::shared_ptr<BufferObject> color_buffer_;
  std::shared_ptr<BufferObject> index_buffer_;
  std::shared_ptr<VertexArrayObject> vao_;
};

}  // namespace hyper
