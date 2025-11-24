/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/bonemesh.h"

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "src/glincludes.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

#define MAX_BONES 200  // same as in bone_mesh_vert.glsl

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 0.9f, 0.9f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.3f);

BoneMesh::BoneMesh(std::shared_ptr<VertexArrayObject> vao,
                   std::shared_ptr<Program> prog,
                   std::shared_ptr<Texture> tex,
                   std::shared_ptr<Node> node,
                   std::vector<glm::mat4> inv_bind_pose_vec)
    : Mesh(vao, prog, tex, node),
      inv_bind_pose_vec_(inv_bind_pose_vec) {
}

void BoneMesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far) {
  node_->BuildDepthFirstAbsXformVec(abs_xform_vec_);
  assert(abs_xform_vec_.size() == inv_bind_pose_vec_.size());
  assert(abs_xform_vec_.size() <= MAX_BONES);

  // AJT: TODO what is the proper model view and normal mat for this mesh?
  glm::mat4 m = glm::mat4(1.0f);  // node_->abs_xform();

  // apply inv_bind_pose_vec_ for rendering
  for (size_t i = 0; i < abs_xform_vec_.size(); i++) {
    // AJT: TODO wtf, manny renders incorrectly, even in bind pose!
    abs_xform_vec_[i] *= inv_bind_pose_vec_[i];
  }

  glm::mat4 model_view_mat = glm::inverse(camera_mat) * m;
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(m)));

  prog_->Bind();
  prog_->SetUniform("modelViewProjMat", proj_mat * model_view_mat);
  prog_->SetUniform("normalModelMat", normal_model_mat);
  prog_->SetUniform("lightDir", glm::normalize(kLightDir));
  prog_->SetUniform("lightColor", kLightColor);
  prog_->SetUniform("ambientColor", kAmbientColor);
  prog_->SetUniform("boneMats[0]", abs_xform_vec_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_->texture);
  prog_->SetUniform("colorTex", 0);
  vao_->DrawElements(GL_TRIANGLES);
}


