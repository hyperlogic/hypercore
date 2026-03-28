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
#include "src/ubershader.h"
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
  UberMaterial(const std::string& name, std::shared_ptr<Program>& prog, UberShaderVariantKey key);
  ~UberMaterial();

  DISABLE_COPY_AND_MOVE(UberMaterial);

  static std::shared_ptr<UberMaterial> Make(UberShaderCache& shader_cache, glm::vec3 base_color,
                                            glm::vec3 emissive_color, float roughness, float metallic);

  void Bind() const;
  const std::shared_ptr<Program>& prog() const { return prog_; }
  const std::string& name() const { return name_; }
  UberShaderVariantKey key() const { return key_; }

  void SetBaseColorFactor(glm::vec4 base_color_factor) { base_color_factor_ = base_color_factor; }
  glm::vec4 GetBaseColorFactor() const { return base_color_factor_; }

  void SetBaseColorTexture(const std::shared_ptr<Texture>& texture) { base_color_tex_ = texture; }
  bool HasBaseColorTexture() const { return base_color_tex_ != nullptr; }
  void SetBaseColorUvIndex(int base_color_uv_index) { base_color_uv_index_ = base_color_uv_index; }
  int GetBaseColorUvIndex() const { return base_color_uv_index_; }

  void SetBaseColorUvOffset(glm::vec2 uv_offset) { base_color_uv_offset_ = uv_offset; }
  glm::vec2 GetBaseColorUvOffset() const { return base_color_uv_offset_; }
  void SetBaseColorUvScale(glm::vec2 uv_scale) { base_color_uv_scale_ = uv_scale; }
  glm::vec2 GetBaseColorUvScale() const { return base_color_uv_scale_; }
  void SetBaseColorUvRotation(float rotation) { base_color_uv_rotation_ = rotation; }
  float GetBaseColorUvRotation() const { return base_color_uv_rotation_; }

  void SetMetallicFactor(float metallic_factor) { metallic_factor_ = metallic_factor; }
  float GetMetallicFactor() const { return metallic_factor_; }

  void SetRoughnessFactor(float roughness_factor) { roughness_factor_ = roughness_factor; }
  float GetRoughnessFactor() const { return roughness_factor_; }

  void SetMetallicRoughnessTexture(const std::shared_ptr<Texture>& texture) { metallic_roughness_tex_ = texture; }
  bool HasMetallicRoughnessTexture() const { return metallic_roughness_tex_ != nullptr; }
  void SetMetallicRoughnessUvIndex(int metallic_roughness_uv_index) {
    metallic_roughness_uv_index_ = metallic_roughness_uv_index;
  }
  int GetMetallicRoughnessUvIndex() const { return metallic_roughness_uv_index_; }

  void SetMetallicRoughnessUvOffset(glm::vec2 uv_offset) { metallic_roughness_uv_offset_ = uv_offset; }
  glm::vec2 GetMetallicRoughnessUvOffset() const { return metallic_roughness_uv_offset_; }
  void SetMetallicRoughnessUvScale(glm::vec2 uv_scale) { metallic_roughness_uv_scale_ = uv_scale; }
  glm::vec2 GetMetallicRoughnessUvScale() const { return metallic_roughness_uv_scale_; }
  void SetMetallicRoughnessUvRotation(float uv_rotation) { metallic_roughness_uv_rotation_ = uv_rotation; }
  float GetMetallicRoughnessUvRotation() const { return metallic_roughness_uv_rotation_; }

  void SetEmissiveColorFactor(glm::vec3 emissive_color_factor) { emissive_color_factor_ = emissive_color_factor; }
  glm::vec3 GetEmissiveColorFactor() const { return emissive_color_factor_; }

  void SetEmissiveColorTexture(const std::shared_ptr<Texture>& texture) { emissive_color_tex_ = texture; }
  bool HasEmissiveColorTexture() const { return emissive_color_tex_ != nullptr; }
  void SetEmissiveColorUvIndex(int emissive_color_uv_index) { emissive_color_uv_index_ = emissive_color_uv_index; }
  int GetEmissiveColorUvIndex() const { return emissive_color_uv_index_; }

  void SetEmissiveColorUvOffset(glm::vec2 uv_offset) { emissive_color_uv_offset_ = uv_offset; }
  glm::vec2 GetEmissiveColorUvOffset() const { return emissive_color_uv_offset_; }
  void SetEmissiveColorUvScale(glm::vec2 uv_scale) { emissive_color_uv_scale_ = uv_scale; }
  glm::vec2 GetEmissiveColorUvScale() const { return emissive_color_uv_scale_; }
  void SetEmissiveColorUvRotation(float uv_rotation) { emissive_color_uv_rotation_ = uv_rotation; }
  float GetEmissiveColorUvRotation() const { return emissive_color_uv_rotation_; }

  void SetSpecularColorFactor(glm::vec3 specular_color_factor) { specular_color_factor_ = specular_color_factor; }
  glm::vec3 GetSpecularColorFactor() const { return specular_color_factor_; }
  void SetSpecularExponent(float specular_exponent) { specular_exponent_ = specular_exponent; }
  float GetSpecularExponent() const { return specular_exponent_; }

 protected:
  std::string name_;
  UberShaderVariantKey key_;
  std::shared_ptr<Program> prog_;

  glm::vec4 base_color_factor_;
  std::shared_ptr<Texture> base_color_tex_;
  int base_color_uv_index_;
  glm::vec2 base_color_uv_offset_;
  glm::vec2 base_color_uv_scale_;
  float base_color_uv_rotation_;

  float metallic_factor_;
  float roughness_factor_;

  glm::vec3 metallic_roughness_factor_;
  std::shared_ptr<Texture> metallic_roughness_tex_;
  int metallic_roughness_uv_index_;
  glm::vec2 metallic_roughness_uv_offset_;
  glm::vec2 metallic_roughness_uv_scale_;
  float metallic_roughness_uv_rotation_;

  glm::vec3 emissive_color_factor_;
  std::shared_ptr<Texture> emissive_color_tex_;
  int emissive_color_uv_index_;
  glm::vec2 emissive_color_uv_offset_;
  glm::vec2 emissive_color_uv_scale_;
  float emissive_color_uv_rotation_;

  glm::vec3 specular_color_factor_;
  float specular_exponent_;
};

}  // namespace hyper
