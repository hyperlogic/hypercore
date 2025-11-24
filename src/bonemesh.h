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

class BoneMesh : public Mesh {
 public:
  BoneMesh(std::shared_ptr<VertexArrayObject> vao_in,
           std::shared_ptr<Program> prog_in,
           std::shared_ptr<Texture> tex_in,
           std::shared_ptr<Node> node,
           std::vector<glm::mat4> inv_bind_pose_vec_in);

  void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
              const glm::vec4& viewport, const glm::vec2& near_far) override;

 protected:
  std::vector<glm::mat4> inv_bind_pose_vec_;
  std::vector<glm::mat4> abs_xform_vec_;
};
