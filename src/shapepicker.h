/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace hyper {

struct Sphere {
  glm::vec3 center;
  float radius;
  uint32_t user_data;
};

struct Cylinder {
  glm::vec3 start;
  glm::vec3 end;
  float radius;
  uint32_t user_data;
};

struct Cone {
  glm::vec3 base;
  glm::vec3 tip;
  float base_radius;
  uint32_t user_data;
};

struct PickResult {
  glm::vec3 pos;
  float t;
  uint32_t user_data;
  bool valid;
};

class ShapePicker {
 public:
  ShapePicker();
  ~ShapePicker();

  void Clear();
  void AddSphere(const Sphere& sphere);
  void AddCylinder(const Cylinder& cylinder);
  void AddCone(const Cone& cone);
  bool Pick(glm::vec3 ray_point, glm::vec3 ray_dir, PickResult* result);

 protected:
  std::vector<Sphere> sphere_vec_;
  std::vector<Cylinder> cylinder_vec_;
  std::vector<Cone> cone_vec_;
};

}  // namespace hyper
