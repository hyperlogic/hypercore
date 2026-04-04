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
#include "src/ubermaterial.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 1.0f, 1.0f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.2f);

BoneMesh::BoneMesh(std::shared_ptr<VertexArrayObject> vao,
                   std::shared_ptr<UberMaterial> mat,
                   std::shared_ptr<Node> node,
                   std::vector<glm::mat4> inv_bind_pose_vec)
    : Mesh(vao, mat, node),
      inv_bind_pose_vec_(inv_bind_pose_vec) {
  InitTbo();
}

BoneMesh::~BoneMesh() {
  if (bone_tbo_) {
    glDeleteTextures(1, &bone_tbo_);
  }
  if (bone_buf_) {
    glDeleteBuffers(1, &bone_buf_);
  }
}

void BoneMesh::InitTbo() {
  glGenBuffers(1, &bone_buf_);
  glGenTextures(1, &bone_tbo_);

  // Allocate initial storage for the buffer.
  glBindBuffer(GL_TEXTURE_BUFFER, bone_buf_);
  glBufferData(GL_TEXTURE_BUFFER,
               inv_bind_pose_vec_.size() * sizeof(glm::mat4),
               nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  // Attach the buffer to the texture.
  glBindTexture(GL_TEXTURE_BUFFER, bone_tbo_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, bone_buf_);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void BoneMesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far) {
  // AJT: TODO what is the proper model view and normal mat for this mesh?
  glm::mat4 model_mat = glm::mat4(1.0f);
  glm::mat4 view_mat = glm::inverse(camera_mat);
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(model_mat)));
  glm::vec3 camera_pos = glm::vec3(camera_mat[3]);

  // build the abs_xform_vec_ aka boneMats
  node_->BuildDepthFirstAbsXformVec(abs_xform_vec_);
  assert(abs_xform_vec_.size() == inv_bind_pose_vec_.size());
  // apply inv_bind_pose_vec_ for rendering
  for (size_t i = 0; i < abs_xform_vec_.size(); i++) {
    // AJT: TODO wtf, manny renders incorrectly, even in bind pose!
    abs_xform_vec_[i] *= inv_bind_pose_vec_[i];
  }

  // Upload bone matrices to the TBO.
  glBindBuffer(GL_TEXTURE_BUFFER, bone_buf_);
  glBufferSubData(GL_TEXTURE_BUFFER, 0,
                  abs_xform_vec_.size() * sizeof(glm::mat4),
                  abs_xform_vec_.data());
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  mat_->Bind();

  mat_->prog()->SetUniform("camera_pos", camera_pos);
  mat_->prog()->SetUniform("model_mat", model_mat);
  mat_->prog()->SetUniform("view_mat", view_mat);
  mat_->prog()->SetUniform("proj_mat", proj_mat);
  mat_->prog()->SetUniform("normal_model_mat", normal_model_mat);

  mat_->prog()->SetUniform("light_direct_dir", glm::normalize(kLightDir));
  mat_->prog()->SetUniform("light_direct_color", kLightColor);
  mat_->prog()->SetUniform("light_ambient_color", kAmbientColor);

  // Bind the bone matrix TBO to texture unit 4 (units 0-1 used by material).
  static const int32_t kBoneTexUnit = 4;
  glActiveTexture(GL_TEXTURE0 + kBoneTexUnit);
  glBindTexture(GL_TEXTURE_BUFFER, bone_tbo_);
  mat_->prog()->SetUniform("boneMats", kBoneTexUnit);

  vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
