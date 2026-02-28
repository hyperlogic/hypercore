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

  void Bind() const;
  const std::shared_ptr<Program>& prog() const { return prog_; }
  const std::string& name() const { return name_; }

  void SetBaseColorFactor(glm::vec4 base_color_factor) { base_color_factor_ = base_color_factor; }
  glm::vec4 GetBaseColorFactor() const { return base_color_factor_; }

  void SetBaseColorTexture(const std::shared_ptr<Texture>& texture) { base_color_tex_ = texture; }
  bool HasBaseColorTexture() const { return base_color_tex_ != nullptr; }
  void SetBaseColorUv(int base_color_uv) { base_color_uv_ = base_color_uv; }
  int GetBaseColorUv() const { return base_color_uv_; }

  void SetMetallicFactor(float metallic_factor) { metallic_factor_ = metallic_factor; }
  float GetMetallicFactor() const { return metallic_factor_; }

  void SetRoughnessFactor(float roughness_factor) { roughness_factor_ = roughness_factor; }
  float GetRoughnessFactor() const { return roughness_factor_; }

  void SetEmissiveColorFactor(glm::vec3 emissive_color_factor) { emissive_color_factor_ = emissive_color_factor; }
  glm::vec3 GetEmissiveColorFactor() const { return emissive_color_factor_; }

  void SetEmissiveColorTexture(const std::shared_ptr<Texture>& texture) { emissive_color_tex_ = texture; }
  bool HasEmissiveColorTexture() const { return emissive_color_tex_ != nullptr; }
  void SetEmissiveColorUv(int emissive_color_uv) { emissive_color_uv_ = emissive_color_uv; }
  int GetEmissiveColorUv() const { return emissive_color_uv_; }

 protected:
  std::string name_;
  std::shared_ptr<Program> prog_;
  std::unordered_map<std::string, std::pair<Program::Variable, Value>> uniforms_;

  glm::vec4 base_color_factor_;
  std::shared_ptr<Texture> base_color_tex_;
  int base_color_uv_;

  float metallic_factor_;
  float roughness_factor_;

  glm::vec3 emissive_color_factor_;
  std::shared_ptr<Texture> emissive_color_tex_;
  int emissive_color_uv_;
};

}  // namespace hyper
