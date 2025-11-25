/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace hyper {

class Anim;
class Mesh;
class Node;

struct Asset {
  std::vector<std::shared_ptr<Mesh>> mesh_vec;
  std::vector<std::shared_ptr<Anim>> anim_vec;
  std::vector<std::shared_ptr<Node>> node_vec;
  std::shared_ptr<Node> root_node;
  std::map<std::string, std::shared_ptr<Node>> string_to_node_map;
  std::shared_ptr<Node> FindNode(const std::string& name) {
    auto iter = string_to_node_map.find(name);
    if (iter != string_to_node_map.end()) {
      return iter->second;
    } else {
      return nullptr;
    }
  }
  std::shared_ptr<const Node> FindNode(const std::string& name) const {
    auto iter = string_to_node_map.find(name);
    if (iter != string_to_node_map.end()) {
      return iter->second;
    } else {
      return nullptr;
    }
  }
};

std::shared_ptr<Asset> AssetImport(const std::string& filename);
std::shared_ptr<Asset> AssetImportAbs(const std::string& filename);

}  // namespace hyper
