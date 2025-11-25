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
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 0.9f, 0.9f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.3f);

Mesh::Mesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<Program> prog,
           std::shared_ptr<Texture> tex,
           std::shared_ptr<Node> node)
    : vao_(vao), prog_(prog), tex_(tex), node_(node) {
}

void Mesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                  const glm::vec4& viewport, const glm::vec2& near_far) {
  glm::mat4 xform = node_->abs_xform();
  prog_->Bind();
  glm::mat4 model_view_mat = glm::inverse(camera_mat) * xform;
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(xform)));

  prog_->SetUniform("modelViewProjMat", proj_mat * model_view_mat);
  prog_->SetUniform("normalModelMat", normal_model_mat);
  prog_->SetUniform("lightDir", glm::normalize(kLightDir));
  prog_->SetUniform("lightColor", kLightColor);
  prog_->SetUniform("ambientColor", kAmbientColor);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_->texture);
  prog_->SetUniform("colorTex", 0);
  vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
