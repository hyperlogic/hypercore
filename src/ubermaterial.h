/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/program.h"
#include "src/util.h"

namespace hyper {

class Texture;

class UberMaterial {
 public:
  union Value {
    float f32[4];
    int32_t i32[4];
    uint32_t u32[4];
  };
  UberMaterial(const std::string& name, std::shared_ptr<Program>& prog);
  ~UberMaterial();

  DISABLE_COPY_AND_MOVE(UberMaterial);

  void SetUniform(const std::string& name, const Value& val);
  void AddTexture(const std::shared_ptr<Texture>& texture);
  void Bind() const;
  const std::shared_ptr<Program>& prog() const { return prog_; }
  const std::string& name() const { return name_; }
  bool HasTextures() const { return textures_.size() > 0; }

  // material properties aka uniforms
  void SetBaseColorFactor(glm::vec4 base_color_factor);
  glm::vec4 GetBaseColorFactor() const { return base_color_factor_; }
  void SetMetallicFactor(float metallic_factor);
  float GetMetallicFactor() const { return metallic_factor_; }
  void SetRoughnessFactor(float roughness_factor);
  float GetRoughnessFactor() const { return roughness_factor_; }

 protected:
  std::string name_;
  std::shared_ptr<Program> prog_;
  std::unordered_map<std::string, std::pair<Program::Variable, Value>> uniforms_;
  std::vector<std::shared_ptr<Texture>> textures_;

  glm::vec4 base_color_factor_;
  float metallic_factor_;
  float roughness_factor_;
};

}  // namespace hyper
