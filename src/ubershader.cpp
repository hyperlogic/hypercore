/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/ubershader.h"

#include <memory>
#include <string>

#include "src/program.h"

namespace hyper {

UberShaderCache::UberShaderCache() {
}
UberShaderCache::~UberShaderCache() {
}

std::shared_ptr<Program> UberShaderCache::GetOrCreate(const UberShaderVariantKey key) {
  auto iter = program_vec_.find(key);
  if (iter == program_vec_.end()) {
    // create program
    auto prog = std::make_shared<Program>();
    std::string mat_info;
    if (key & UberShaderVariantFlags::HAS_BONES) {
      mat_info += "#define HAS_BONES\n";
    }
    if (key & UberShaderVariantFlags::HAS_BASE_TEXTURE) {
      mat_info += "#define HAS_BASE_TEXTURE\n";
    }
    if (key & UberShaderVariantFlags::HAS_BASE_TEXTURE_UV_TRANSFORM) {
      mat_info += "#define HAS_BASE_TEXTURE_UV_TRANSFORM\n";
    }
    if (key & UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE) {
      mat_info += "#define HAS_EMISSIVE_TEXTURE\n";
    }
    if (key & UberShaderVariantFlags::HAS_EMISSIVE_TEXTURE_UV_TRANSFORM) {
      mat_info += "#define HAS_EMISSIVE_TEXTURE_UV_TRANSFORM\n";
    }
    if (key & UberShaderVariantFlags::HAS_UV0) {
      mat_info += "#define HAS_UV0\n";
    }
    if (key & UberShaderVariantFlags::HAS_UV1) {
      mat_info += "#define HAS_UV1\n";
    }
    if (key & UberShaderVariantFlags::HAS_VERTEX_COLORS) {
      mat_info += "#define HAS_VERTEX_COLORS\n";
    }
    prog->AddMacro("MATERIALINFO", mat_info);
    if (!prog->LoadVertFrag("shader/uber_vert.glsl", "shader/uber_frag.glsl")) {
      Log::E("Error loading uber shader! key = 0x%x\n", key);
      return nullptr;
    }
    return prog;
  } else {
    return iter->second;
  }
}

}  // namespace hyper
