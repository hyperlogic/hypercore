/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace hyper {

class DebugRenderer;

class Node : public std::enable_shared_from_this<Node> {
 public:
  Node(size_t idx,
       const std::string name,
       std::shared_ptr<Node> parent,
       const glm::mat4& rel_xform)
      : idx_(idx),
        rel_xform_(rel_xform),
        abs_xform_(rel_xform),
        name_(name),
        parent_(parent) {}
  size_t idx() const { return idx_; }
  const std::string& name() const { return name_; }
  std::shared_ptr<Node> parent() { return parent_.lock(); }
  std::shared_ptr<const Node> parent() const { return parent_.lock(); }
  const glm::mat4& rel_xform() const { return rel_xform_; }
  void set_rel_xform(const glm::mat4& rel_xform) {
    rel_xform_ = rel_xform;
  }
  const glm::mat4& abs_xform() const { return abs_xform_; }
  const std::vector<std::shared_ptr<Node>>& child_vec() const {
    return child_vec_;
  }
  std::vector<std::shared_ptr<Node>>& child_vec() {
    return child_vec_;
  }

  void Update();
  void BuildDepthFirstAbsXformVec(std::vector<glm::mat4>& result);
  void BuildDepthFirstNodeVec(std::vector<std::shared_ptr<Node>>& result);
  size_t GetSubtreeSize() const;
  void DebugDraw(std::shared_ptr<DebugRenderer>, const glm::mat4& root_mat,
                 float axis_line = 0.1f);
  void Print() const;

 protected:
  glm::mat4 rel_xform_;
  glm::mat4 abs_xform_;
  std::string name_;
  std::weak_ptr<Node> parent_;
  std::vector<std::shared_ptr<Node>> child_vec_;
  size_t idx_;
};

}  // namespace hyper
