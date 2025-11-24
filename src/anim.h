/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Anim {
 public:
  Anim(const std::string& name_in, size_t num_frames_in,
       size_t num_joints_in, float fps);
  void SetKey(size_t frame, size_t joint, const glm::mat4& value);
  const glm::mat4& GetKey(size_t frame, size_t joint) const;
  const std::string& name() const { return name_; }
  size_t num_frames() const { return num_frames_; }
  size_t num_joints() const { return num_joints_; }
  void SetJointName(size_t i, const std::string& name) {
    joint_name_vec_[i] = name;
  }
  const std::string& GetJointName(size_t i) const { return joint_name_vec_[i]; }
  float fps() const { return fps_; }
 protected:
  std::string name_;
  std::vector<std::string> joint_name_vec_;
  std::vector<glm::mat4> frame_vec_;
  size_t num_frames_;
  size_t num_joints_;
  float fps_;
};
