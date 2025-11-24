/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct Texture;
class Program;
class VertexArrayObject;

class CheckerFloor {
 public:
  explicit CheckerFloor(const glm::mat4& floor_mat_in);

  bool Init(bool is_framebuffer_srgb_enabled_in);

  void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
              const glm::vec4& viewport, const glm::vec2& near_far);

  glm::mat4 floor_mat_;

  std::shared_ptr<Texture> floor_tex_;
  std::shared_ptr<Program> floor_prog_;
  std::shared_ptr<VertexArrayObject> floor_vao_;
  bool is_framebuffer_srgb_enabled_;
};
