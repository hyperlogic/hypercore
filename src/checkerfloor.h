/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hyper {

struct RenderParams;

class Texture;
class Program;
class VertexArrayObject;

class CheckerFloor {
 public:
  explicit CheckerFloor(const glm::mat4& floor_mat_in);

  bool Init(bool is_framebuffer_srgb_enabled_in);

  void Render(const RenderParams& render_params);

  glm::mat4 floor_mat_;

  std::shared_ptr<Texture> floor_tex_;
  std::shared_ptr<Program> floor_prog_;
  std::shared_ptr<VertexArrayObject> floor_vao_;
  bool is_framebuffer_srgb_enabled_;
};

}  // namespace hyper
