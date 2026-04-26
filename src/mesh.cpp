/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/mesh.h"

#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "src/glincludes.h"
#include "src/program.h"
#include "src/render.h"
#include "src/texture.h"
#include "src/ubermaterial.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

Mesh::Mesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<UberMaterial> mat,
           std::shared_ptr<Node> node)
    : vao_(vao), mat_(mat), node_(node) {
}

void Mesh::Render(const RenderParams& r_params, const LightingParams& l_params) {
  glm::mat4 model_mat = node_->abs_xform();
  glm::mat4 view_mat = glm::inverse(r_params.camera_mat);
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(model_mat)));
  glm::vec3 camera_pos = glm::vec3(r_params.camera_mat[3]);

  mat_->Bind();

  mat_->prog()->SetUniform("camera_pos", camera_pos);
  mat_->prog()->SetUniform("model_mat", model_mat);
  mat_->prog()->SetUniform("view_mat", view_mat);
  mat_->prog()->SetUniform("proj_mat", r_params.proj_mat);
  mat_->prog()->SetUniform("normal_model_mat", normal_model_mat);

  mat_->prog()->SetUniform("light_direct_dir", glm::normalize(l_params.direct_dir));
  mat_->prog()->SetUniform("light_direct_color", l_params.direct_color);
  mat_->prog()->SetUniform("light_ambient_color", l_params.ambient_color);

  vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
