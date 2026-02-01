/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/material.h"

#include <glm/glm.hpp>

#include "src/glincludes.h"

namespace hyper {

Material::Material(std::shared_ptr<Program>& prog) {
  prog_ = prog;
}

void Material::AddUniform(const Program::Variable& var, const Value& val) {
  uniforms_.push_back(std::pair(var, val));
}

void Material::Bind() const {
  prog_->Bind();
  for (auto& pair : uniforms_) {
    auto& var = pair.first;
    auto& val = pair.second;
    switch (pair.first.type) {
      case GL_FLOAT:
        prog_->SetUniformRaw(var.loc, val.f32[0]);
        break;
      case GL_FLOAT_VEC2:
        prog_->SetUniformRaw(var.loc,
                             reinterpret_cast<const glm::vec2&>(val.f32));
        break;
      case GL_FLOAT_VEC3:
        prog_->SetUniformRaw(var.loc,
                             reinterpret_cast<const glm::vec3&>(val.f32));
        break;
      case GL_FLOAT_VEC4:
        prog_->SetUniformRaw(var.loc,
                             reinterpret_cast<const glm::vec4&>(val.f32));
        break;
      case GL_INT:
        prog_->SetUniformRaw(var.loc, val.i32[0]);
        break;
      case GL_UNSIGNED_INT:
        prog_->SetUniformRaw(var.loc, val.u32[0]);
        break;
      default:
        // just support basic types for now.
        Log::W("Illegal type %d for material!\n");
        break;
    }
  }
}

}  // namespace hyper
