/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/debugrenderer.h"

#include <memory>
#include <vector>

#include "src/glincludes.h"
#include "src/log.h"
#include "src/util.h"
#include "src/program.h"
#include "src/vertexbuffer.h"

namespace hyper {

const size_t kMaxVertexCount = 32768;

DebugRenderer::DebugRenderer() {
}

bool DebugRenderer::Init() {
  dd_prog_ = std::make_shared<Program>();
  if (!dd_prog_->LoadVertFrag("shader/debugdraw_vert.glsl",
                               "shader/debugdraw_frag.glsl")) {
    Log::E("Error loading DebugRenderer shader!\n");
    return false;
  }

  line_position_vec_.resize(kMaxVertexCount);
  line_color_vec_.resize(kMaxVertexCount);
  line_position_buffer_ = std::make_shared<BufferObject>(
      GL_ARRAY_BUFFER, line_position_vec_, GL_DYNAMIC_STORAGE_BIT);
  line_color_buffer_ = std::make_shared<BufferObject>(
      GL_ARRAY_BUFFER, line_color_vec_, GL_DYNAMIC_STORAGE_BIT);

  line_position_vec_.clear();
  line_color_vec_.clear();

  vao_ = std::make_shared<VertexArrayObject>();
  vao_->SetAttribBuffer(dd_prog_->GetAttribLoc("position"),
                        line_position_buffer_);
  vao_->SetAttribBuffer(dd_prog_->GetAttribLoc("color"), line_color_buffer_);

  return true;
}

void DebugRenderer::Line(const glm::vec3& start_pos, const glm::vec3& end_pos,
                         const glm::vec3& color) {
  Line(start_pos, end_pos, color, color);
}

void DebugRenderer::Line(const glm::vec3& start_pos,
                         const glm::vec3& end_pos,
                         const glm::vec3& start_color,
                         const glm::vec3& end_color) {
  // AJT(TODO): dynamically resize buffers
  if (line_position_vec_.size() + 2 <= kMaxVertexCount) {
    line_position_vec_.push_back(start_pos);
    line_position_vec_.push_back(end_pos);
    line_color_vec_.push_back(start_color);
    line_color_vec_.push_back(end_color);
  } else if (frame_num_ != warn_frame_num_) {
    Log::W("DebugRenderer overflow!\n");
    warn_frame_num_ = frame_num_;
  }
}

void DebugRenderer::Transform(const glm::mat4& m, float axis_len) {
  glm::vec3 x = glm::vec3(m[0]);
  glm::vec3 y = glm::vec3(m[1]);
  glm::vec3 z = glm::vec3(m[2]);
  x = axis_len * glm::normalize(x);
  y = axis_len * glm::normalize(y);
  z = axis_len * glm::normalize(z);
  glm::vec3 p = glm::vec3(m[3]);

  Line(p, p + x, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
  Line(p, p + y, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
  Line(p, p + z, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugRenderer::Box(const glm::mat4& m, float radius) {
  // Construct box corners in local space
  // radius is distance from center to corner, so for a cube:
  // corner = (±a, ±a, ±a) where sqrt(3a²) = radius
  float a = radius / sqrtf(3.0f);

  // 8 corners in local space
  glm::vec4 local_corners[8] = {
      glm::vec4(-a, -a, -a, 1.0f),  // 0: ---
      glm::vec4(+a, -a, -a, 1.0f),  // 1: +--
      glm::vec4(+a, +a, -a, 1.0f),  // 2: ++-
      glm::vec4(-a, +a, -a, 1.0f),  // 3: -+-
      glm::vec4(-a, -a, +a, 1.0f),  // 4: --+
      glm::vec4(+a, -a, +a, 1.0f),  // 5: +-+
      glm::vec4(+a, +a, +a, 1.0f),  // 6: +++
      glm::vec4(-a, +a, +a, 1.0f)   // 7: -++
  };

  // Transform to world space
  glm::vec3 world_corners[8];
  for (int i = 0; i < 8; i++) {
    glm::vec4 world_pos = m * local_corners[i];
    world_corners[i] = glm::vec3(world_pos);
  }

  glm::vec3 box_color(1.0f, 1.0f, 1.0f);  // White color for box

  // Draw bottom face (4 edges)
  Line(world_corners[0], world_corners[1], box_color);
  Line(world_corners[1], world_corners[2], box_color);
  Line(world_corners[2], world_corners[3], box_color);
  Line(world_corners[3], world_corners[0], box_color);

  // Draw top face (4 edges)
  Line(world_corners[4], world_corners[5], box_color);
  Line(world_corners[5], world_corners[6], box_color);
  Line(world_corners[6], world_corners[7], box_color);
  Line(world_corners[7], world_corners[4], box_color);

  // Draw vertical edges (4 edges)
  Line(world_corners[0], world_corners[4], box_color);
  Line(world_corners[1], world_corners[5], box_color);
  Line(world_corners[2], world_corners[6], box_color);
  Line(world_corners[3], world_corners[7], box_color);
}

void DebugRenderer::Render(const glm::mat4& camera_mat,
                           const glm::mat4& proj_mat,
                           const glm::vec4& viewport,
                           const glm::vec2& near_far) {
  frame_num_++;

#ifdef __linux__
  glLineWidth(2.0f);  // Set line width
#endif

  dd_prog_->Bind();
  glm::mat4 model_view_proj_mat = proj_mat * glm::inverse(camera_mat);
  dd_prog_->SetUniform("modelViewProjMat", model_view_proj_mat);

  vao_->Bind();

  line_position_buffer_->Update(line_position_vec_);
  line_color_buffer_->Update(line_color_vec_);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(line_position_vec_.size()));

  vao_->Unbind();

#ifdef __linux__
  glLineWidth(1.0f);
#endif
}

void DebugRenderer::EndFrame() {
  line_position_vec_.clear();
  line_color_vec_.clear();
}

}  // namespace hyper
