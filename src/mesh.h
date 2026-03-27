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
class UberMaterial;
class VertexArrayObject;

class Mesh {
 public:
  Mesh(std::shared_ptr<VertexArrayObject> vao,
       std::shared_ptr<UberMaterial> mat,
       std::shared_ptr<Node> node);

  static std::shared_ptr<Mesh> MakeSphere(const std::shared_ptr<UberMaterial>& mat,
                                          const std::shared_ptr<Node>& node,
                                          glm::vec3 center, float radius, int num_subdivs);
  static std::shared_ptr<Mesh> MakeCylinder(const std::shared_ptr<UberMaterial>& mat,
                                            const std::shared_ptr<Node>& node,
                                            glm::vec3 start, glm::vec3 end, float radius,
                                            int num_circle_subdivs, int num_length_subdivs);
  static std::shared_ptr<Mesh> MakeCone(const std::shared_ptr<UberMaterial>& mat,
                                        const std::shared_ptr<Node>& node,
                                        glm::vec3 start, glm::vec3 end, float radius,
                                        int num_circle_subdivs, int num_length_subdivs);
  static std::shared_ptr<Mesh> MakeBoneOctahedron(const std::shared_ptr<UberMaterial>& mat,
                                                  const std::shared_ptr<Node>& node,
                                                  glm::vec3 start, glm::vec3 end, float radius);

  virtual void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                      const glm::vec4& viewport, const glm::vec2& near_far);

 protected:
  std::shared_ptr<VertexArrayObject> vao_;
  std::shared_ptr<UberMaterial> mat_;
  std::shared_ptr<Node> node_;
};

}  // namespace hyper
