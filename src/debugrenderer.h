
/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hyper {

struct RenderParams;

class Program;
class BufferObject;
class VertexArrayObject;

class DebugRenderer {
 public:
  DebugRenderer();

  bool Init();

  // viewport = (x, y, width, height)
  void Render(const RenderParams& render_params);

  // call at end of frame.
  void EndFrame();

  void Line(glm::vec3 start_pos, glm::vec3 end_pos, glm::vec3 color);
  void Line(glm::vec3 start_pos, glm::vec3 end_pos,
            glm::vec3 start_color, glm::vec3 end_color);
  void Point(glm::vec3 pos, glm::vec3 color, float radius = 0.1f);
  void Transform(const glm::mat4& m, float axis_len = 1.0f);
  void Box(const glm::mat4& m, float radius, glm::vec3 color);

 protected:
  std::shared_ptr<Program> dd_prog_;
  std::vector<glm::vec3> line_position_vec_;
  std::vector<glm::vec3> line_color_vec_;

  std::shared_ptr<BufferObject> line_position_buffer_;
  std::shared_ptr<BufferObject> line_color_buffer_;

  std::shared_ptr<VertexArrayObject> vao_;
  uint64_t frame_num_ = 0;
  uint64_t warn_frame_num_ = 0;
};

}  // namespace hyper
