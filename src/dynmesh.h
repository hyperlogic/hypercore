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

#include "src/geom.h"

namespace hyper {

class BufferObject;
class Node;
class UberMaterial;
class VertexArrayObject;

class DynMesh {
 public:
  DynMesh(std::shared_ptr<UberMaterial> mat,
          std::shared_ptr<Node> node);

  void Clear();
  void Push(const Geom& geom, const glm::mat4& xform, glm::vec4 color);

  virtual void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far);

 protected:
  void SetAttribBuffer(int loc, const std::shared_ptr<BufferObject>& attrib_buffer);

  std::shared_ptr<UberMaterial> mat_;
  std::shared_ptr<Node> node_;
  Geom geom_batch_;
  std::vector<glm::vec4> color_batch_;
  bool dirty_;
  std::shared_ptr<BufferObject> pos_buffer_;
  std::shared_ptr<BufferObject> uv_buffer_;
  std::shared_ptr<BufferObject> norm_buffer_;
  std::shared_ptr<BufferObject> color_buffer_;
  std::shared_ptr<BufferObject> index_buffer_;
  std::shared_ptr<VertexArrayObject> vao_;
};

}  // namespace hyper
