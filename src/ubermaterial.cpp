/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/ubermaterial.h"

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/glincludes.h"
#include "src/texture.h"

namespace hyper {

UberMaterial::UberMaterial(const std::string& name, std::shared_ptr<Program>& prog) {
  name_ = name;
  prog_ = prog;
}

UberMaterial::~UberMaterial() {
}

void UberMaterial::AddUniform(const Program::Variable& var, const Value& val) {
  uniforms_.push_back(std::pair(var, val));
}

void UberMaterial::AddTexture(const std::shared_ptr<Texture>& texture) {
  textures_.push_back(texture);
}

void UberMaterial::Bind() const {
  prog_->Bind();
  for (auto& pair : uniforms_) {
    auto& var = pair.first;
    auto& val = pair.second;
    switch (pair.first.type) {
      case GL_FLOAT:
        prog_->SetUniformRaw(var.loc, val.f32[0]);
        break;
      case GL_FLOAT_VEC2:
        prog_->SetUniformRaw(var.loc, reinterpret_cast<const glm::vec2&>(val.f32));
        break;
      case GL_FLOAT_VEC3:
        prog_->SetUniformRaw(var.loc, reinterpret_cast<const glm::vec3&>(val.f32));
        break;
      case GL_FLOAT_VEC4:
        prog_->SetUniformRaw(var.loc, reinterpret_cast<const glm::vec4&>(val.f32));
        break;
      case GL_INT:
        prog_->SetUniformRaw(var.loc, val.i32[0]);
        break;
      case GL_UNSIGNED_INT:
        prog_->SetUniformRaw(var.loc, val.u32[0]);
        break;
      default:
        // just support basic types for now.
        Log::W("Illegal type %d for material %s!\n", name_.c_str());
        break;
    }
  }

  // AJT(TODO) support more then one texture with differnt semantics
  if (textures_.size() > 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]->texture);
    prog_->SetUniform("colorTex", 0);
  }
}

}  // namespace hyper
