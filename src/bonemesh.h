/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <string>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "src/node.h"
#include "src/mesh.h"

namespace hyper {

class UberMaterial;

class BoneMesh : public Mesh {
 public:
  BoneMesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<UberMaterial> mat,
           std::shared_ptr<Node> node,
           std::vector<glm::mat4> inv_bind_pose_vec_in);
  ~BoneMesh() override;

  void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
              const glm::vec4& viewport, const glm::vec2& near_far) override;

 protected:
  void InitTbo();

  std::vector<glm::mat4> inv_bind_pose_vec_;
  std::vector<glm::mat4> abs_xform_vec_;
  uint32_t bone_tbo_ = 0;   // texture object for bone matrices
  uint32_t bone_buf_ = 0;   // buffer object backing the TBO
};

}  // namespace hyper
