/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "src/program.h"
#include "src/util.h"

namespace hyper {

class Texture;

class Material {
 public:
  union Value {
    float f32[4];
    int32_t i32[4];
    uint32_t u32[4];
  };
  Material(const std::string& name, std::shared_ptr<Program>& prog);
  ~Material();

  DISABLE_COPY_AND_MOVE(Material);

  void AddUniform(const Program::Variable& var, const Value& val);
  void AddTexture(const std::shared_ptr<Texture>& texture);
  void Bind() const;
  const std::shared_ptr<Program>& prog() const { return prog_; }
  const std::string& name() const { return name_; }
  bool HasTextures() const { return textures_.size() > 0; }
 protected:
  std::string name_;
  std::shared_ptr<Program> prog_;
  std::vector<std::pair<Program::Variable, Value>> uniforms_;
  std::vector<std::shared_ptr<Texture>> textures_;
};

}  // namespace hyper
