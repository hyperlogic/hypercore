/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

#include "src/program.h"
#include "src/util.h"

namespace hyper {

class Material {
 public:
  union Value {
    float f32[4];
    int32_t i32[4];
    uint32_t u32[4];
  };
  explicit Material(std::shared_ptr<Program>& prog);
  ~Material();

  DISABLE_COPY_AND_MOVE(Material);

  void AddUniform(const Program::Variable& var, const Value& val);
  void Bind() const;
 protected:
  std::shared_ptr<Program> prog_;
  std::vector<std::pair<Program::Variable, Value>> uniforms_;
};

}  // namespace hyper
