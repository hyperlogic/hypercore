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

struct RenderParams;
struct LightingParams;

class DebugRenderer;
class Program;
class UberMaterial;
class VertexArrayObject;

class Mesh {
 public:
  Mesh(std::shared_ptr<VertexArrayObject> vao,
       std::shared_ptr<UberMaterial> mat,
       std::shared_ptr<Node> node);
  virtual ~Mesh() = default;

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

  virtual void Render(const RenderParams& r_params, const LightingParams& l_params);
  virtual void RenderWireframe(const RenderParams& r_params, glm::vec4 color);

 protected:
  // Lazily builds wireframe_prog_ and wireframe_vao_.
  void InitWireframe();
  // Override to add defines (e.g. skinning) before the shader is compiled.
  virtual void AddWireframeMacros(Program& prog) const {}
  // Override to attach extra attribs (e.g. bone weights) to the wireframe vao.
  virtual void AddWireframeAttribs(VertexArrayObject& wireframe_vao) const {}
  // Draws the shaded triangles with a small polygon offset so wireframe lines
  // drawn over the mesh do not z-fight with it.
  void DrawTrianglesWithOffset() const;
  // Draws the wireframe with an already computed model matrix.
  void DrawWireframe(const RenderParams& r_params, const glm::mat4& model_mat, glm::vec4 color);

  std::shared_ptr<VertexArrayObject> vao_;
  std::shared_ptr<UberMaterial> mat_;
  std::shared_ptr<VertexArrayObject> wireframe_vao_;
  std::shared_ptr<Program> wireframe_prog_;
  std::shared_ptr<Node> node_;
};

}  // namespace hyper
