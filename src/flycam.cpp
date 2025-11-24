/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#include "src/flycam.h"

#include "src/log.h"
#include "src/util.h"

FlyCam::FlyCam(const glm::vec3& world_up_in, const glm::vec3& pos_in,
               const glm::quat& rot_in, float speed_in, float rot_speed_in) :
    world_up_(world_up_in), pos_(pos_in), vel_(0.0f, 0.0f, 0.0f),
    rot_(rot_in), camera_mat_(MakeMat4(rot_in, pos_in)), speed_(speed_in),
    rot_speed_(rot_speed_in) {
}

void FlyCam::Process(const glm::vec2& left_stick_in,
                     const glm::vec2& right_stick_in, float roll_amount_in,
                     float up_amount_in, float dt) {
  glm::vec2 left_stick = left_stick_in;
  glm::vec2 right_stick = right_stick_in;
  float roll_amount = roll_amount_in;

  const float kMaxSpeed = speed_;
  const float kRiseTime = 0.1f;  // time to reach ~95% of terminal speed

  const float kK = 3.0f / kRiseTime;
  const float kExp = exp(-kK * dt);

  // Get the input direction
  glm::vec3 target_vel = rot_ * (kMaxSpeed * glm::vec3(left_stick.x,
                                                        up_amount_in,
                                                        -left_stick.y));

  // Exponential blend toward target velocity
  glm::vec3 v = vel_ * kExp + target_vel * (1.0f - kExp);

  // Closed-form integration of position
  pos_ += (vel_ - target_vel) * (1.0f - kExp) / kK + target_vel * dt;
  vel_ = v;

  // right stick controls orientation
  // glm::vec3 up = rot_ * glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = rot_ * glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 forward = rot_ * glm::vec3(0.0f, 0.0f, -1.0f);
  glm::quat yaw = glm::angleAxis(rot_speed_ * dt * -right_stick.x,
                                 world_up_);
  glm::quat pitch = glm::angleAxis(rot_speed_ * dt * right_stick.y, right);
  rot_ = (yaw * pitch) * rot_;

  // axes of new camera_mat_
  glm::vec3 x = rot_ * glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 y = rot_ * glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 z = rot_ * glm::vec3(0.0f, 0.0f, 1.0f);

  // apply roll to world_up_
  if (fabs(roll_amount_in) > 0.1f) {
    world_up_ = glm::vec3(camera_mat_[1]);
    glm::quat roll = glm::angleAxis(rot_speed_ * dt * roll_amount, forward);
    world_up_ = roll * world_up_;
  }

  // make sure that camera_mat_ will be orthogonal, and aligned with world up.
  if (glm::dot(z, world_up_) < 0.999f) {  // if we aren't looking straight up
    glm::vec3 xx = glm::normalize(glm::cross(world_up_, z));
    glm::vec3 yy = glm::normalize(glm::cross(z, xx));
    camera_mat_ = glm::mat4(glm::vec4(xx, 0.0f), glm::vec4(yy, 0.0f),
                            glm::vec4(z, 0.0f), glm::vec4(pos_, 1.0f));
  } else {
    camera_mat_ = glm::mat4(glm::vec4(x, 0.0f), glm::vec4(y, 0.0f),
                            glm::vec4(z, 0.0f), glm::vec4(pos_, 1.0f));
  }
  glm::vec3 unused_scale;
  Decompose(camera_mat_, &unused_scale, &rot_);
}

void FlyCam::SetCameraMat(const glm::mat4& camera_mat) {
  pos_ = glm::vec3(camera_mat[3]);
  rot_ = glm::normalize(glm::quat(glm::mat3(camera_mat)));
  vel_ = glm::vec3(0.0f, 0.0f, 0.0f);
}
