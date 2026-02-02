/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/mesh.h"

#include <memory>
#include <vector>

#include "src/glincludes.h"
#include "src/material.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 0.9f, 0.9f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.3f);

Mesh::Mesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<Material> mat,
           std::shared_ptr<Node> node)
    : vao_(vao), mat_(mat), node_(node) {
}

void Mesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                  const glm::vec4& viewport, const glm::vec2& near_far) {
  glm::mat4 xform = node_->abs_xform();

  mat_->Bind();

  glm::mat4 model_view_mat = glm::inverse(camera_mat) * xform;
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(xform)));

  mat_->prog()->SetUniform("modelViewProjMat", proj_mat * model_view_mat);
  mat_->prog()->SetUniform("normalModelMat", normal_model_mat);
  mat_->prog()->SetUniform("lightDir", glm::normalize(kLightDir));
  mat_->prog()->SetUniform("lightColor", kLightColor);

  vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
