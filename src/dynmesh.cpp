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
#include "src/program.h"
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
}

void DynMesh::Clear() {
  batched_attribs_.Clear();
  dirty_ = true;
}

void DynMesh::Push(const MeshAttribs& mesh_attribs) {
  batched_attribs_.Push(mesh_attribs);
  dirty_ = true;
}

void DynMesh::Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                     const glm::vec4& viewport, const glm::vec2& near_far) {
  glm::mat4 model_mat = node_->abs_xform();
  glm::mat4 view_mat = glm::inverse(camera_mat);
  glm::mat3 normal_model_mat = glm::transpose(glm::inverse(glm::mat3(model_mat)));
  glm::vec3 camera_pos = glm::vec3(camera_mat[3]);

  if (dirty_) {
    if (pos_buffer_ && pos_buffer_->num_elements() >= batched_attribs_.pos_vec.size()) {
      pos_buffer_->Update(batched_attribs_.pos_vec);
    } else {
      pos_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, batched_attribs_.pos_vec);
    }
    if (uv_buffer_ && uv_buffer_->num_elements() >= batched_attribs_.uv_vec.size()) {
      uv_buffer_->Update(batched_attribs_.uv_vec);
    } else {
      uv_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, batched_attribs_.uv_vec);
    }
    if (norm_buffer_ && norm_buffer_->num_elements() >= batched_attribs_.norm_vec.size()) {
      norm_buffer_->Update(batched_attribs_.norm_vec);
    } else {
      norm_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, batched_attribs_.norm_vec);
    }
    if (mat_->key() & UberShaderVariantFlags::HAS_VERTEX_COLORS) {
      if (color_buffer_ && color_buffer_->num_elements() >= batched_attribs_.color_vec.size()) {
        color_buffer_->Update(batched_attribs_.color_vec);
      } else {
        color_buffer_ = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, batched_attribs_.color_vec);
      }
    }
    if (index_buffer_ && index_buffer_->num_elements() >= batched_attribs_.index_vec.size()) {
      index_buffer_->Update(batched_attribs_.index_vec);
    } else {
      index_buffer_ = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, batched_attribs_.index_vec);
    }
    dirty_ = false;
  }

  mat_->Bind();

  mat_->prog()->SetUniform("camera_pos", camera_pos);
  mat_->prog()->SetUniform("model_mat", model_mat);
  mat_->prog()->SetUniform("view_mat", view_mat);
  mat_->prog()->SetUniform("proj_mat", proj_mat);
  mat_->prog()->SetUniform("normal_model_mat", normal_model_mat);

  mat_->prog()->SetUniform("light_direct_dir", glm::normalize(kLightDir));
  mat_->prog()->SetUniform("light_direct_color", kLightColor);
  mat_->prog()->SetUniform("light_ambient_color", kAmbientColor);

  int pos_loc = mat_->prog()->GetAttribLoc("position");
  SetAttribBuffer(pos_loc, pos_buffer_);

  if (mat_->key() & UberShaderVariantFlags::HAS_UV0) {
    int uv_loc = mat_->prog()->GetAttribLoc("uv0");
    SetAttribBuffer(uv_loc, uv_buffer_);
  }

  int norm_loc = mat_->prog()->GetAttribLoc("position");
  SetAttribBuffer(norm_loc, norm_buffer_);

  if (mat_->key() & UberShaderVariantFlags::HAS_VERTEX_COLORS) {
    int color_loc = mat_->prog()->GetAttribLoc("color");
    SetAttribBuffer(color_loc, color_buffer_);
  }

  index_buffer_->Bind();
  glDrawElements(GL_TRIANGLES, index_buffer_->num_elements(), GL_UNSIGNED_INT, nullptr);
}

void DynMesh::SetAttribBuffer(int loc, const std::shared_ptr<BufferObject>& attrib_buffer) {
  attrib_buffer->Bind();
  glVertexAttribPointer(loc, attrib_buffer->element_size(), GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(loc);
  attrib_buffer->Unbind();
}

}  // namespace hyper
