/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/node.h"

namespace hyper {

class Hierarchy;
class Program;
class Texture;
class VertexArrayObject;

class Material {
 public:
  enum class Mode {
    Pbr = 0
  };

  std::string name;
  Mode mode;
};


class Mesh {
 public:
  Mesh(std::shared_ptr<VertexArrayObject> vao,
       std::shared_ptr<Program> prog,
       std::shared_ptr<Texture> tex,
       std::shared_ptr<Node> node);

  virtual void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far);
 protected:
  std::shared_ptr<VertexArrayObject> vao_;
  std::shared_ptr<Program> prog_;
  std::shared_ptr<Texture> tex_;
  std::shared_ptr<Node> node_;
};

}  // namespace hyper
