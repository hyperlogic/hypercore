/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/dynmesh.h"

#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "src/glincludes.h"
#include "src/node.h"
#include "src/program.h"
#include "src/render.h"
#include "src/texture.h"
#include "src/ubermaterial.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

const glm::vec3 kLightDir(1.0f, 1.0f, 0.0f);
const glm::vec3 kLightColor(1.0f, 1.0f, 1.0f);
const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.2f);

DynMesh::DynMesh(std::shared_ptr<UberMaterial> mat,
                 std::shared_ptr<Node> node)
    : mat_(mat), node_(node) {
  // only supported key at the moment.
  assert(mat_->key() == static_cast<UberShaderVariantKey>(UberShaderVariantFlags::HAS_VERTEX_COLORS |
                                                          UberShaderVariantFlags::HAS_EMISSIVE_VERTEX_COLORS));
}

void DynMesh::Clear() {
  geom_batch_.Clear();
  color_batch_.clear();
  emissive_color_batch_.clear();
  dirty_ = true;
}

void DynMesh::Push(const Geom& geom, const glm::mat4& xform, glm::vec4 color, glm::vec3 emissive_color) {
  geom_batch_.Push(geom, xform);
  color_batch_.insert(color_batch_.end(), geom.pos_vec().size(), color);
  emissive_color_batch_.insert(emissive_color_batch_.end(), geom.pos_vec().size(), emissive_color);
  dirty_ = true;
}

void DynMesh::Render(const RenderParams& r_params, const LightingParams& l_params) {
  glm::mat4 model_mat = node_->abs_xform();
  glm::mat4 view_mat = glm::inverse(r_params.camera_mat);
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(model_mat)));
  glm::vec3 camera_pos = glm::vec3(r_params.camera_mat[3]);

  if (geom_batch_.pos_vec().size() == 0 ||
      geom_batch_.index_vec().size() == 0) {
    return;
  }

  if (dirty_) {
    if (pos_buffer_ && pos_buffer_->num_elements() >= geom_batch_.pos_vec().size()) {
      pos_buffer_->Update(geom_batch_.pos_vec());
    } else {
      pos_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, geom_batch_.pos_vec(), GL_DYNAMIC_STORAGE_BIT);
    }
    if (norm_buffer_ && norm_buffer_->num_elements() >= geom_batch_.norm_vec().size()) {
      norm_buffer_->Update(geom_batch_.norm_vec());
    } else {
      norm_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, geom_batch_.norm_vec(), GL_DYNAMIC_STORAGE_BIT);
    }
    if (mat_->key() & UberShaderVariantFlags::HAS_VERTEX_COLORS) {
      if (color_buffer_ && color_buffer_->num_elements() >= color_batch_.size()) {
        color_buffer_->Update(color_batch_);
      } else {
        color_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, color_batch_, GL_DYNAMIC_STORAGE_BIT);
      }
    }
    if (mat_->key() & UberShaderVariantFlags::HAS_EMISSIVE_VERTEX_COLORS) {
      if (emissive_color_buffer_ && emissive_color_buffer_->num_elements() >= emissive_color_batch_.size()) {
        emissive_color_buffer_->Update(emissive_color_batch_);
      } else {
        emissive_color_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, emissive_color_batch_,
                                                                GL_DYNAMIC_STORAGE_BIT);
      }
    }
    if (index_buffer_ && index_buffer_->num_elements() >= geom_batch_.index_vec().size()) {
      index_buffer_->Update(geom_batch_.index_vec());
    } else {
      index_buffer_ = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, geom_batch_.index_vec(),
                                                     GL_DYNAMIC_STORAGE_BIT);
    }
    dirty_ = false;
  }

  if (!vao_) {
    vao_ = std::make_shared<VertexArrayObject>();
  }

  mat_->Bind();

  mat_->prog()->SetUniform("camera_pos", camera_pos);
  mat_->prog()->SetUniform("model_mat", model_mat);
  mat_->prog()->SetUniform("view_mat", view_mat);
  mat_->prog()->SetUniform("proj_mat", r_params.proj_mat);
  mat_->prog()->SetUniform("normal_model_mat", normal_model_mat);

  mat_->prog()->SetUniform("light_direct_dir", glm::normalize(l_params.direct_dir));
  mat_->prog()->SetUniform("light_direct_color", l_params.direct_color);
  if (mat_->key() & UberShaderVariantFlags::HAS_ENV_IRRADIANCE_SH) {
    mat_->prog()->SetUniform("env_irr_r_sh0", l_params.env_irr_sh.r_sh0);
    mat_->prog()->SetUniform("env_irr_r_sh1", l_params.env_irr_sh.r_sh1);
    mat_->prog()->SetUniform("env_irr_r_sh2", l_params.env_irr_sh.r_sh2);
    mat_->prog()->SetUniform("env_irr_r_sh3", l_params.env_irr_sh.r_sh3);
    mat_->prog()->SetUniform("env_irr_g_sh0", l_params.env_irr_sh.g_sh0);
    mat_->prog()->SetUniform("env_irr_g_sh1", l_params.env_irr_sh.g_sh1);
    mat_->prog()->SetUniform("env_irr_g_sh2", l_params.env_irr_sh.g_sh2);
    mat_->prog()->SetUniform("env_irr_g_sh3", l_params.env_irr_sh.g_sh3);
    mat_->prog()->SetUniform("env_irr_b_sh0", l_params.env_irr_sh.b_sh0);
    mat_->prog()->SetUniform("env_irr_b_sh1", l_params.env_irr_sh.b_sh1);
    mat_->prog()->SetUniform("env_irr_b_sh2", l_params.env_irr_sh.b_sh2);
    mat_->prog()->SetUniform("env_irr_b_sh3", l_params.env_irr_sh.b_sh3);
  } else {
    mat_->prog()->SetUniform("light_ambient_color", l_params.ambient_color);
  }

  vao_->Bind();

  int pos_loc = mat_->prog()->GetAttribLoc("position");
  SetAttribBuffer(pos_loc, pos_buffer_);

  if (mat_->key() & UberShaderVariantFlags::HAS_UV0) {
    int uv_loc = mat_->prog()->GetAttribLoc("uv0");
    SetAttribBuffer(uv_loc, uv_buffer_);
  }

  int norm_loc = mat_->prog()->GetAttribLoc("normal");
  SetAttribBuffer(norm_loc, norm_buffer_);

  if (mat_->key() & UberShaderVariantFlags::HAS_VERTEX_COLORS) {
    int loc = mat_->prog()->GetAttribLoc("color");
    SetAttribBuffer(loc, color_buffer_);
  }

  if (mat_->key() & UberShaderVariantFlags::HAS_EMISSIVE_VERTEX_COLORS) {
    int loc = mat_->prog()->GetAttribLoc("emissive_color");
    SetAttribBuffer(loc, emissive_color_buffer_);
  }

  index_buffer_->Bind();
  glDrawElements(GL_TRIANGLES, index_buffer_->num_elements(), GL_UNSIGNED_INT, nullptr);

  vao_->Unbind();
}

void DynMesh::SetAttribBuffer(int loc, const std::shared_ptr<BufferObject>& attrib_buffer) {
  attrib_buffer->Bind();
  glVertexAttribPointer(loc, attrib_buffer->element_size(), GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(loc);
  attrib_buffer->Unbind();
}

}  // namespace hyper
