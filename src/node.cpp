/*
  Copyright (c) 2025 Anthony J. Thibault
  This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/node.h"

#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "src/debugrenderer.h"
#include "src/log.h"

static void PrintNode(const Node* node, size_t indent_level, int32_t& count) {
  std::string indent = std::string(indent_level * 4, ' ');
  Log::D("%s%s [%d]\n", indent.c_str(), node->name().c_str(), count);
  const glm::mat4& m4 = node->rel_xform();
  Log::D("%s    | %10.5f, %10.5f, %10.5f, %10.5f |\n",
         indent.c_str(), m4[0][0], m4[1][0], m4[2][0], m4[3][0]);
  Log::D("%s    | %10.5f, %10.5f, %10.5f, %10.5f |\n",
         indent.c_str(), m4[0][1], m4[1][1], m4[2][1], m4[3][1]);
  Log::D("%s    | %10.5f, %10.5f, %10.5f, %10.5f |\n",
         indent.c_str(), m4[0][2], m4[1][2], m4[2][2], m4[3][2]);
  Log::D("%s    | %10.5f, %10.5f, %10.5f, %10.5f |\n",
         indent.c_str(), m4[0][3], m4[1][3], m4[2][3], m4[3][3]);
  count++;
  for (const auto& child : node->child_vec()) {
    PrintNode(child.get(), indent_level + 1, count);
  }
}

void Node::Update() {
  auto parent = parent_.lock();
  if (parent) {
    abs_xform_ = parent->abs_xform_ * rel_xform_;
  } else {
    abs_xform_ = rel_xform_;
  }
  for (auto& child : child_vec_) {
    child->Update();
  }
}

void Node::BuildDepthFirstAbsXformVec(std::vector<glm::mat4>& result) {
  result.clear();
  std::stack<std::shared_ptr<Node>> stack;
  stack.push(shared_from_this());
  while (!stack.empty()) {
    auto node = stack.top();
    stack.pop();
    result.push_back(node->abs_xform_);
    // push children in reverse order for DepthFirst order
    for (int i = static_cast<int>(node->child_vec_.size()) - 1; i >= 0; i--) {
      stack.push(node->child_vec_[i]);
    }
  }
}

void Node::BuildDepthFirstNodeVec(std::vector<std::shared_ptr<Node>>& result) {
  result.clear();
  std::stack<std::shared_ptr<Node>> stack;
  stack.push(shared_from_this());
  while (!stack.empty()) {
    auto node = stack.top();
    stack.pop();
    result.push_back(node);
    // push children in reverse order for DepthFirst order
    for (int i = static_cast<int>(node->child_vec_.size()) - 1; i >= 0; i--) {
      stack.push(node->child_vec_[i]);
    }
  }
}

size_t Node::GetSubtreeSize() const {
  size_t count = 1;
  for (auto& child : child_vec_) {
    count += child->GetSubtreeSize();
  }
  return count;
}

void Node::DebugDraw(std::shared_ptr<DebugRenderer> debug_renderer,
                     const glm::mat4& root_mat,
                     float axis_len) {
  assert(debug_renderer);
  std::vector<std::shared_ptr<Node>> node_vec;
  BuildDepthFirstNodeVec(node_vec);
  const glm::vec3 bone_color(0.9f, 0.9f, 0.9f);
  for (size_t i = 0; i < node_vec.size(); i++) {
    glm::mat4 node_m = root_mat * node_vec[i]->abs_xform();
    debug_renderer->Transform(node_m, axis_len);
    if (node_vec[i]->parent()) {
      glm::vec3 pos(node_m[3][0], node_m[3][1], node_m[3][2]);
      glm::mat4 parent_m = root_mat * node_vec[i]->parent()->abs_xform();
      glm::vec3 parent_pos(parent_m[3][0], parent_m[3][1], parent_m[3][2]);
      debug_renderer->Line(pos, parent_pos, bone_color);
    }
  }
}

void Node::Print() const {
  int32_t count = 0;
  PrintNode(this, 0, count);
}
