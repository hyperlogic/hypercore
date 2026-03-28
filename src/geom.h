/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hyper {

class Geom {
 public:
  Geom();
  virtual ~Geom();
  Geom(const Geom& other);
  Geom& operator=(const Geom& other);
  Geom(Geom&& other) noexcept;
  Geom& operator=(Geom&& other) noexcept;

  static Geom MakeSphere(glm::vec3 center, float radius, int num_subdivs);
  static Geom MakeCylinder(glm::vec3 start, glm::vec3 end, float radius,
                           int num_circle_subdivs, int num_length_subdivs);
  static Geom MakeCone(glm::vec3 start, glm::vec3 end, float radius,
                       int num_circle_subdivs, int num_length_subdivs);
  static Geom MakeBoneOctahedron(glm::vec3 start, glm::vec3 end, float radius);

  static Geom MoveFromBuffers(std::vector<glm::vec3> pos,
                              std::vector<glm::vec3> norm,
                              std::vector<uint32_t> indices);
  void Clear();
  void Push(const Geom& geom);
  const std::vector<glm::vec3> pos_vec() const { return pos_vec_; }
  const std::vector<glm::vec3> norm_vec() const { return norm_vec_; }
  const std::vector<uint32_t> index_vec() const { return index_vec_; }

 protected:
  std::vector<glm::vec3> pos_vec_;
  std::vector<glm::vec3> norm_vec_;
  std::vector<uint32_t> index_vec_;
};

}  // namespace hyper
