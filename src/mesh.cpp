/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/mesh.h"

#include <memory>
#include <vector>

#include "src/glincludes.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/ubermaterial.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 1.0f, 1.0f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.2f);

Mesh::Mesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<UberMaterial> mat,
           std::shared_ptr<Node> node)
    : vao_(vao), mat_(mat), node_(node) {
}

void Mesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                  const glm::vec4& viewport, const glm::vec2& near_far) {
  glm::mat4 model_mat = node_->abs_xform();
  glm::mat4 view_mat = glm::inverse(camera_mat);
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(model_mat)));
  glm::vec3 camera_pos = glm::vec3(camera_mat[3]);

  mat_->Bind();

  mat_->prog()->SetUniform("camera_pos", camera_pos);
  mat_->prog()->SetUniform("model_mat", model_mat);
  mat_->prog()->SetUniform("view_mat", view_mat);
  mat_->prog()->SetUniform("proj_mat", proj_mat);
  mat_->prog()->SetUniform("normal_model_mat", normal_model_mat);

  mat_->prog()->SetUniform("light_direct_dir", glm::normalize(kLightDir));
  mat_->prog()->SetUniform("light_direct_color", kLightColor);
  mat_->prog()->SetUniform("light_ambient_color", kAmbientColor);

  vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
