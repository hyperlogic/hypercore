/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/ubermaterial.h"

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/glincludes.h"
#include "src/program.h"
#include "src/texture.h"


namespace hyper {

UberMaterial::UberMaterial(const std::string& name, std::shared_ptr<Program>& prog, UberShaderVariantKey key)
    : name_(name),
      key_(key),
      prog_(prog),
      base_color_factor_(1.0f, 1.0f, 1.0f, 1.0f),
      base_color_tex_(),
      base_color_uv_index_(0),
      base_color_uv_offset_(0.0f, 0.0f),
      base_color_uv_scale_(0.0f, 0.0f),
      base_color_uv_rotation_(0.0f),
      metallic_factor_(1.0f),
      roughness_factor_(1.0f),
      emissive_color_factor_(0.0f, 0.0f, 0.0f),
      emissive_color_uv_index_(),
      emissive_color_uv_offset_(0.0f, 0.0f),
      emissive_color_uv_scale_(0.0f, 0.0f),
      emissive_color_uv_rotation_(0.0f),
      specular_color_factor_(0.5f, 0.5f, 0.5f),
      specular_exponent_(32.0f) {
}

UberMaterial::~UberMaterial() {
}

std::shared_ptr<UberMaterial> UberMaterial::Make(UberShaderCache& shader_cache, glm::vec3 base_color,
                                                 glm::vec3 emissive_color, float roughness, float metallic) {
  UberShaderVariantKey key = 0;
  auto prog = shader_cache.GetOrCreate(key);
  auto mat = std::make_shared<UberMaterial>("generated", prog, key);
  mat->SetBaseColorFactor(glm::vec4(base_color, 1.0f));
  mat->SetEmissiveColorFactor(emissive_color);
  mat->SetMetallicFactor(metallic);
  mat->SetRoughnessFactor(roughness);
  return mat;
}

void UberMaterial::Bind() const {
  prog_->Bind();

  // TODO(AJT): cache the uniform locs..
  prog_->SetUniform("base_color_factor", base_color_factor_);
  if (key_ & UberShaderVariantFlags::HAS_BASE_TEXTURE) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_color_tex_->texture);
    prog_->SetUniform("base_color_tex", 0);
    prog_->SetUniform("base_color_uv_index", base_color_uv_index_);
    if (key_ & UberShaderVariantFlags::HAS_BASE_TEXTURE_UV_TRANSFORM) {
      prog_->SetUniform("base_color_uv_offset", base_color_uv_offset_);
      prog_->SetUniform("base_color_uv_scale", base_color_uv_scale_);
      prog_->SetUniform("base_color_uv_rotation", base_color_uv_rotation_);
    }
  }

  if (!(key_ & UberShaderVariantFlags::HAS_SPECULAR)) {
    prog_->SetUniform("metallic_factor", metallic_factor_);
    prog_->SetUniform("roughness_factor", roughness_factor_);
  }

  if (key_ & UberShaderVariantFlags::HAS_METALLIC_ROUGHNESS_TEXTURE) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness_tex_->texture);
    prog_->SetUniform("metallic_roughness_tex", 1);
    prog_->SetUniform("metallic_roughness_uv_index", metallic_roughness_uv_index_);
    if (key_ & UberShaderVariantFlags::HAS_METALLIC_ROUGHNESS_TEXTURE_UV_TRANSFORM) {
      prog_->SetUniform("metallic_roughness_uv_offset", metallic_roughness_uv_offset_);
      prog_->SetUniform("metallic_roughness_uv_scale", metallic_roughness_uv_scale_);
      prog_->SetUniform("metallic_roughness_uv_rotation", metallic_roughness_uv_rotation_);
    }
  }

  prog_->SetUniform("emissive_color_factor", emissive_color_factor_);
  if (key_ & UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, emissive_color_tex_->texture);
    prog_->SetUniform("emissive_color_tex", 1);
    prog_->SetUniform("emissive_color_uv_index", emissive_color_uv_index_);
    if (key_ & UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE_UV_TRANSFORM) {
      prog_->SetUniform("emissive_color_uv_offset", emissive_color_uv_offset_);
      prog_->SetUniform("emissive_color_uv_scale", emissive_color_uv_scale_);
      prog_->SetUniform("emissive_color_uv_rotation", emissive_color_uv_rotation_);
    }
  }

  if (key_ & UberShaderVariantFlags::HAS_SPECULAR) {
    prog_->SetUniform("specular_color_factor", specular_color_factor_);
    prog_->SetUniform("specular_exponent", specular_exponent_);
  }
}

}  // namespace hyper
