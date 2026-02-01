/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/log.h"
#include "src/util.h"

namespace hyper {

class Program {
 public:
  Program();
  ~Program();

  DISABLE_COPY_AND_MOVE(Program);

  // used to inject #defines or other code into shaders
  // AddMacro("FOO", "BAR");
  // will replace the string /*%%FOO%%*/ in the source shader with BAR
  void AddMacro(const std::string& key, const std::string& value);

  bool LoadVertFrag(const std::string& vert_filename,
                    const std::string& frag_filename);
  bool LoadVertGeomFrag(const std::string& vert_filename,
                        const std::string& geom_filename,
                        const std::string& frag_filename);
  bool LoadCompute(const std::string& compute_filename);
  void Bind() const;

  int GetUniformLoc(const std::string& name) const;
  int GetAttribLoc(const std::string& name) const;

  template <typename T>
  void SetUniform(const std::string& name, T value) const {
    auto iter = uniforms_.find(name);
    if (iter != uniforms_.end()) {
      SetUniformRaw(iter->second.loc, value);
    } else {
      Log::W("Could not find uniform \"%s\" for program \"%s\"\n",
             name.c_str(), debug_name_.c_str());
    }
  }

  void SetUniformRaw(int loc, int32_t value) const;
  void SetUniformRaw(int loc, uint32_t value) const;
  void SetUniformRaw(int loc, float value) const;
  void SetUniformRaw(int loc, const glm::vec2& value) const;
  void SetUniformRaw(int loc, const glm::vec3& value) const;
  void SetUniformRaw(int loc, const glm::vec4& value) const;
  void SetUniformRaw(int loc, const glm::mat2& value) const;
  void SetUniformRaw(int loc, const glm::mat3& value) const;
  void SetUniformRaw(int loc, const glm::mat4& value) const;
  void SetUniformRaw(int loc, const std::vector<glm::mat4>& value) const;

  template <typename T>
  void SetAttrib(const std::string& name, T* values, size_t stride = 0) const {
    auto iter = attribs_.find(name);
    if (iter != attribs_.end()) {
      SetAttribRaw(iter->second.loc, values, stride);
    } else {
      Log::W("Could not find attrib \"%s\" for program \"%s\"\n",
             name.c_str(), debug_name_.c_str());
    }
  }

  void SetAttribRaw(int loc, float* values, size_t stride = 0) const;
  void SetAttribRaw(int loc, glm::vec2* values, size_t stride = 0) const;
  void SetAttribRaw(int loc, glm::vec3* values, size_t stride = 0) const;
  void SetAttribRaw(int loc, glm::vec4* values, size_t stride = 0) const;

 protected:
  void Delete();
  bool CheckLinkStatus();

  int program_;
  int vert_shader_;
  int geom_shader_;
  int frag_shader_;
  int compute_shader_;

  class Variable {
   public:
    int size;
    uint32_t type;
    int loc;
  };

  std::unordered_map<std::string, Variable> uniforms_;
  std::unordered_map<std::string, Variable> attribs_;
  std::vector<std::pair<std::string, std::string>> macros_;
  std::string debug_name_;
};

}  // namespace hyper
