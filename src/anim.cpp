/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/anim.h"

#include <string>

Anim::Anim(const std::string& name, size_t num_frames,
           size_t num_joints, float fps)
    : name_(name),
      joint_name_vec_(num_joints),
      frame_vec_(num_frames * num_joints, glm::mat4(1.0f)),
      num_frames_(num_frames),
      num_joints_(num_joints),
      fps_(fps) {
}

void Anim::SetKey(size_t frame, size_t joint, const glm::mat4& value) {
  frame_vec_[frame * num_joints_ + joint] = value;
}

const glm::mat4& Anim::GetKey(size_t frame, size_t joint) const {
  return frame_vec_[frame * num_joints_ + joint];
}
