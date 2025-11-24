/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class FlyCam {
 public:
  FlyCam(const glm::vec3& world_up_in, const glm::vec3& pos_in,
         const glm::quat& rot_in, float speed_in, float rot_speed_in);

  void Process(const glm::vec2& left_stick_in,
               const glm::vec2& right_stick_in, float roll_amount_in,
               float up_amount_in, float dt);
  const glm::mat4& GetCameraMat() const { return camera_mat_; }
  void SetCameraMat(const glm::mat4& camera_mat);

 protected:
  float speed_;       // units per sec
  float rot_speed_;   // radians per sec
  glm::vec3 world_up_;
  glm::vec3 pos_;
  glm::vec3 vel_;
  glm::quat rot_;
  glm::mat4 camera_mat_;
};
