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

#include "src/debugrenderer.h"
#include "src/glincludes.h"
#include "src/log.h"
#include "src/program.h"
#include "src/render.h"
#include "src/texture.h"
#include "src/ubermaterial.h"
#include "src/ubershader.h"
#include "src/util.h"
#include "src/vertexbuffer.h"

namespace hyper {

static std::shared_ptr<BufferObject> BuildWireframeIndexBuffer(const VertexArrayObject& vao) {
  // create new wireframe index buffer, using the original triangle indices.
  auto triangle_buffer = vao.GetElementBuffer();
  assert(triangle_buffer);
  assert(triangle_buffer->element_size() == 1);
  assert((triangle_buffer->num_elements() % 3) == 0);
  std::vector<uint32_t> triangle_vec(triangle_buffer->num_elements());
  triangle_buffer->Read(triangle_vec);
  uint32_t num_triangles = triangle_buffer->num_elements() / 3;
  std::vector<uint32_t> line_vec;
  line_vec.reserve(triangle_buffer->num_elements() * 2);
  for (uint32_t i = 0; i < num_triangles; i++) {
    uint32_t i0 = triangle_vec[i * 3 + 0];
    uint32_t i1 = triangle_vec[i * 3 + 1];
    uint32_t i2 = triangle_vec[i * 3 + 2];
    line_vec.push_back(i0);
    line_vec.push_back(i1);
    line_vec.push_back(i1);
    line_vec.push_back(i2);
    line_vec.push_back(i2);
    line_vec.push_back(i0);
  }
  return std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, line_vec);
}

Mesh::Mesh(std::shared_ptr<VertexArrayObject> vao,
           std::shared_ptr<UberMaterial> mat,
           std::shared_ptr<Node> node)
    : vao_(vao), mat_(mat), wireframe_vao_(), wireframe_prog_(), node_(node) {
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

  DrawTrianglesWithOffset();
}

void Mesh::DrawTrianglesWithOffset() const {
  // Push the filled triangles slightly away from the camera so that wireframe
  // lines, which share the same vertices, win the depth test.
  static const float kPolygonOffsetFactor = 1.0f;
  static const float kPolygonOffsetUnits = 1.0f;
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(kPolygonOffsetFactor, kPolygonOffsetUnits);
  vao_->DrawElements(GL_TRIANGLES);
  glDisable(GL_POLYGON_OFFSET_FILL);
}

void Mesh::InitWireframe() {
  if (!wireframe_prog_) {
    auto prog = std::make_shared<Program>();
    AddWireframeMacros(*prog);
    if (!prog->LoadVertFrag("shader/wireframe_mesh_vert.glsl",
                            "shader/wireframe_mesh_frag.glsl")) {
      Log::E("Error loading Wireframe Mesh shader!\n");
      assert(false);
      return;
    }
    wireframe_prog_ = prog;
  }
  if (!wireframe_vao_) {
    auto wireframe_vao = std::make_shared<VertexArrayObject>();

    // share the position attrib buffer with the original vao.
    auto position_buffer = vao_->GetAttribBuffer(mat_->GetProg()->GetAttribLoc("position"));
    assert(position_buffer);
    assert(position_buffer->element_size() == 3);
    wireframe_vao->SetAttribBuffer(wireframe_prog_->GetAttribLoc("position"), position_buffer);
    AddWireframeAttribs(*wireframe_vao);

    wireframe_vao->SetElementBuffer(BuildWireframeIndexBuffer(*vao_));
    wireframe_vao_ = wireframe_vao;
  }
}

void Mesh::DrawWireframe(const RenderParams& r_params, const glm::mat4& model_mat,
                         glm::vec4 color) {
  glm::mat4 view_mat = glm::inverse(r_params.camera_mat);
  glm::mat4 model_view_proj_mat = r_params.proj_mat * view_mat * model_mat;

  wireframe_prog_->Bind();
  wireframe_prog_->SetUniform("modelViewProjMat", model_view_proj_mat);
  wireframe_prog_->SetUniform("color", color);

  // Lines share vertices with the shaded mesh, so allow equal depths to pass
  // to avoid z-fighting with the filled triangles.
  GLint prev_depth_func = GL_LESS;
  glGetIntegerv(GL_DEPTH_FUNC, &prev_depth_func);
  glDepthFunc(GL_LEQUAL);
  wireframe_vao_->DrawElements(GL_LINES);
  glDepthFunc(static_cast<GLenum>(prev_depth_func));
}

void Mesh::RenderWireframe(const RenderParams& r_params, glm::vec4 color) {
  InitWireframe();
  if (!wireframe_prog_ || !wireframe_vao_) {
    return;
  }
  DrawWireframe(r_params, node_->abs_xform(), color);
}

}  // namespace hyper
