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

UberMaterial::UberMaterial(const std::string& name, std::shared_ptr<Program>& prog)
    : name_(name),
      prog_(prog),
      base_color_factor_(1.0f, 1.0f, 1.0f, 1.0f),
      base_color_tex_(),
      base_color_uv_(0),
      metallic_factor_(1.0f),
      roughness_factor_(1.0f),
      emissive_color_factor_(0.0f, 0.0f, 0.0f),
      emissive_color_uv_() {
}

UberMaterial::~UberMaterial() {
}

void UberMaterial::Bind() const {
  prog_->Bind();

  // TODO(AJT): cache the uniform locs..
  prog_->SetUniform("base_color_factor", base_color_factor_);

  if (base_color_tex_) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_color_tex_->texture);
    prog_->SetUniform("base_color_tex", 0);
    prog_->SetUniform("base_color_uv", base_color_uv_);
  }

  prog_->SetUniform("metallic_factor", metallic_factor_);
  prog_->SetUniform("roughness_factor", roughness_factor_);

  prog_->SetUniform("emissive_color_factor", emissive_color_factor_);

  if (emissive_color_tex_) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, emissive_color_tex_->texture);
    prog_->SetUniform("emissive_color_tex", 1);
    prog_->SetUniform("emissive_color_uv", emissive_color_uv_);
  }
}

}  // namespace hyper
