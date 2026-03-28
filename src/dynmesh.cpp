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

MeshAttribs MeshAttribs::MakeSphere(glm::vec4 color, glm::vec3 center, float radius, int num_subdivs) {
  // Build icosahedron vertices (unit sphere).
  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  std::vector<glm::vec3> verts = {
      glm::normalize(glm::vec3(-1, t, 0)),  glm::normalize(glm::vec3(1, t, 0)),
      glm::normalize(glm::vec3(-1, -t, 0)), glm::normalize(glm::vec3(1, -t, 0)),
      glm::normalize(glm::vec3(0, -1, t)),  glm::normalize(glm::vec3(0, 1, t)),
      glm::normalize(glm::vec3(0, -1, -t)), glm::normalize(glm::vec3(0, 1, -t)),
      glm::normalize(glm::vec3(t, 0, -1)),  glm::normalize(glm::vec3(t, 0, 1)),
      glm::normalize(glm::vec3(-t, 0, -1)), glm::normalize(glm::vec3(-t, 0, 1)),
  };

  std::vector<uint32_t> indices = {
      0, 11, 5,  0, 5, 1,   0, 1, 7,   0, 7, 10,  0, 10, 11,
      1, 5, 9,   5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
      3, 9, 4,   3, 4, 2,   3, 2, 6,   3, 6, 8,   3, 8, 9,
      4, 9, 5,   2, 4, 11,  6, 2, 10,  8, 6, 7,   9, 8, 1,
  };

  // Subdivide: split each triangle into 4 by inserting edge midpoints.
  using EdgeKey = std::pair<uint32_t, uint32_t>;
  auto make_edge_key = [](uint32_t a, uint32_t b) -> EdgeKey {
    return a < b ? EdgeKey(a, b) : EdgeKey(b, a);
  };

  for (int s = 0; s < num_subdivs; s++) {
    std::map<EdgeKey, uint32_t> mid_cache;
    std::vector<uint32_t> new_indices;
    new_indices.reserve(indices.size() * 4);

    auto get_mid = [&](uint32_t a, uint32_t b) -> uint32_t {
      EdgeKey key = make_edge_key(a, b);
      auto it = mid_cache.find(key);
      if (it != mid_cache.end()) return it->second;
      uint32_t idx = static_cast<uint32_t>(verts.size());
      verts.push_back(glm::normalize((verts[a] + verts[b]) * 0.5f));
      mid_cache[key] = idx;
      return idx;
    };

    size_t num_tris = indices.size() / 3;
    for (size_t i = 0; i < num_tris; i++) {
      uint32_t v0 = indices[i * 3 + 0];
      uint32_t v1 = indices[i * 3 + 1];
      uint32_t v2 = indices[i * 3 + 2];
      uint32_t m01 = get_mid(v0, v1);
      uint32_t m12 = get_mid(v1, v2);
      uint32_t m20 = get_mid(v2, v0);
      new_indices.insert(new_indices.end(), {v0, m01, m20, v1, m12, m01, v2, m20, m12, m01, m12, m20});
    }
    indices = std::move(new_indices);
  }

  // Normals are the unit sphere positions; scale for final positions.
  std::vector<glm::vec3> pos_vec(verts.size());
  std::vector<glm::vec2> uv_vec(verts.size(), glm::vec2(0.0f, 0.0f));
  std::vector<glm::vec3> norm_vec(verts.size());
  std::vector<glm::vec4> color_vec(verts.size(), color);
  for (size_t i = 0; i < verts.size(); i++) {
    norm_vec[i] = verts[i];
    pos_vec[i] = (verts[i] * radius) + center;
  }

  // AJT(TODO): need a move

  MeshAttribs result;
  result.pos_vec = std::move(pos_vec);
  result.uv_vec = std::move(uv_vec);
  result.norm_vec = std::move(norm_vec);
  result.color_vec = std::move(color_vec);
  result.index_vec = std::move(indices);
  return result;
}

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

  if (batched_attribs_.pos_vec.size() == 0 ||
      batched_attribs_.index_vec.size() == 0) {
    return;
  }

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

  if (!vao_) {
    vao_ = std::make_shared<VertexArrayObject>();
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
    int color_loc = mat_->prog()->GetAttribLoc("color");
    SetAttribBuffer(color_loc, color_buffer_);
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
