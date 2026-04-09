/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#include "src/shapepicker.h"

#include <cassert>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/util.h"

namespace hyper {

ShapePicker::ShapePicker() {}

ShapePicker::~ShapePicker() {}

void ShapePicker::Clear() {
  sphere_vec_.clear();
  cylinder_vec_.clear();
  cone_vec_.clear();
}

void ShapePicker::AddSphere(const Sphere& sphere) {
  sphere_vec_.push_back(sphere);
}

void ShapePicker::AddCylinder(const Cylinder& cylinder) {
  cylinder_vec_.push_back(cylinder);
}

void ShapePicker::AddCone(const Cone& cone) {
  cone_vec_.push_back(cone);
}

bool ShapePicker::Pick(glm::vec3 ray_point, glm::vec3 ray_dir, PickResult* result) {
  assert(result);
  bool found = false;
  float nearest_t = std::numeric_limits<float>::max();
  uint32_t nearest_user_data = 0;
  float result_1, result_2;
  for (auto& sphere : sphere_vec_) {
    int count = RaySphereIntersect(ray_point, ray_dir,
                                   sphere.center, sphere.radius,
                                   &result_1, &result_2);
    if (count > 0) {
      if (result_1 < nearest_t) {
        nearest_t = result_1;
        nearest_user_data = sphere.user_data;
        found = true;
      }
      if (count == 1 && result_2 < nearest_t) {
        nearest_t = result_2;
        nearest_user_data = sphere.user_data;
        found = true;
      }
    }
  }
  for (auto& cylinder : cylinder_vec_) {
    int count = RayCylinderIntersect(ray_point, ray_dir,
                                     cylinder.start, cylinder.end, cylinder.radius,
                                     &result_1, &result_2);
    if (count > 0) {
      if (result_1 < nearest_t) {
        nearest_t = result_1;
        nearest_user_data = cylinder.user_data;
        found = true;
      }
      if (count == 1 && result_2 < nearest_t) {
        nearest_t = result_2;
        nearest_user_data = cylinder.user_data;
        found = true;
      }
    }
  }
  for (auto& cone : cone_vec_) {
    int count = RayConeIntersect(ray_point, ray_dir,
                                 cone.base, cone.tip, cone.base_radius,
                                 &result_1, &result_2);
    if (count > 0) {
      if (result_1 < nearest_t) {
        nearest_t = result_1;
        nearest_user_data = cone.user_data;
        found = true;
      }
      if (count == 1 && result_2 < nearest_t) {
        nearest_t = result_2;
        nearest_user_data = cone.user_data;
        found = true;
      }
    }
  }
  if (found) {
    result->pos = ray_point + nearest_t * ray_dir;
    result->t = nearest_t;
    result->user_data = nearest_user_data;
    result->valid = true;
  } else {
    result->pos = glm::vec3(0.0f, 0.0f, 0.0f);
    result->t = std::numeric_limits<float>::max();
    result->user_data = -1;
    result->valid = false;
  }
  return found;
}

}  // namespace hyper
