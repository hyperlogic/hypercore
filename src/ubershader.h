/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <memory>
#include <unordered_map>

#include "src/util.h"

namespace hyper {

class Program;

using UberShaderVariantKey = uint32_t;

enum class UberShaderVariantFlags : uint32_t {
  HAS_BONES = (1 << 0),
  HAS_BASE_TEXTURE = (1 << 1),
  HAS_BASE_TEXTURE_UV_TRANSFORM = (1 << 2),
  HAS_EMISSIVE_TEXTURE = (1 << 3),
  HAS_EMISSIVE_TEXTURE_UV_TRANSFORM = (1 << 4),
  HAS_UV0 = (1 << 5),
  HAS_UV1 = (1 << 6),
  HAS_VERTEX_COLORS = (1 << 7),
};

inline constexpr UberShaderVariantFlags operator|(UberShaderVariantFlags a, UberShaderVariantFlags b) {
  return static_cast<UberShaderVariantFlags>(
      static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr uint32_t operator|(uint32_t a, UberShaderVariantFlags b) {
  return a | static_cast<uint32_t>(b);
}

inline constexpr uint32_t operator&(uint32_t a, UberShaderVariantFlags b) {
  return a & static_cast<uint32_t>(b);
}

inline constexpr uint32_t& operator|=(uint32_t& a, UberShaderVariantFlags b) {
  return a |= static_cast<uint32_t>(b);
}

class UberShaderCache {
 public:
  UberShaderCache();
  virtual ~UberShaderCache();

  DISABLE_COPY_AND_MOVE(UberShaderCache);

  std::shared_ptr<Program> GetOrCreate(const UberShaderVariantKey);

 protected:
  std::unordered_map<UberShaderVariantKey, std::shared_ptr<Program>> program_vec_;
};

}  // namespace hyper
