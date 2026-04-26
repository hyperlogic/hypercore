/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/program.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "src/glincludes.h"

#include "src/log.h"
#include "src/render.h"
#include "src/util.h"

#ifndef NDEBUG
#define WARNINGS_AS_ERRORS
#endif

namespace hyper {

static std::string ExpandMacros(
    std::vector<std::pair<std::string, std::string>> macros,
    const std::string& source) {
  std::string result = source;

  for (const auto& macro : macros) {
    std::string::size_type pos = 0;
    while ((pos = result.find(macro.first, pos)) != std::string::npos) {
      result.replace(pos, macro.first.length(), macro.second);
      // Move past the last replaced position
      pos += macro.second.length();
    }
  }

  return result;
}

static void DumpShaderSource(const std::string& source) {
  std::stringstream ss(source);
  std::string line;
  int i = 1;
  while (std::getline(ss, line)) {
    Log::D("%04d: %s\n", i, line.c_str());
    i++;
  }
  Log::D("\n");
}

static bool CompileShader(GLenum type, const std::string& source,
                          GLint* shader_out, const std::string& debug_name) {
  GLint shader = glCreateShader(type);
  int size = static_cast<int>(source.size());
  const GLchar* source_ptr = source.c_str();
  glShaderSource(shader, 1, (const GLchar**)&source_ptr, &size);
  glCompileShader(shader);

  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

  if (!compiled) {
    Log::E("shader compilation error for \"%s\"!\n", debug_name.c_str());
  }

  GLint buffer_len = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &buffer_len);
  if (buffer_len > 1) {
    if (compiled) {
      Log::E("shader compilation warning for \"%s\"!\n", debug_name.c_str());
    }

    GLsizei len = 0;
    std::unique_ptr<char> buffer(new char[buffer_len]);
    glGetShaderInfoLog(shader, buffer_len, &len, buffer.get());
    Log::E("%s\n", buffer.get());
    DumpShaderSource(source);
  }

#ifdef WARNINGS_AS_ERRORS
  if (!compiled || buffer_len > 1)
#else
  if (!compiled)
#endif
  {
    return false;
  }

  *shader_out = shader;
  return true;
}

Program::Program() : program_(0), vert_shader_(0), geom_shader_(0),
                     frag_shader_(0), compute_shader_(0) {
#ifdef __ANDROID__
  AddMacro("HEADER", "#version 320 es\nprecision highp float;");
#elif __APPLE__
  AddMacro("HEADER", "#version 410");
#else
  AddMacro("HEADER", "#version 460");
#endif
}

Program::~Program() {
  Delete();
}

void Program::AddMacro(const std::string& key, const std::string& value) {
  // In order to keep the glsl code compiling if the macro is not applied.
  // the key is enclosed in a c-style comment and double %.
  std::string token = "/*%%" + key + "%%*/";
  macros_.push_back(std::pair(token, value));
}

bool Program::LoadVertFrag(const std::string& vert_filename,
                           const std::string& frag_filename) {
  return LoadVertGeomFrag(vert_filename, std::string(), frag_filename);
}

bool Program::LoadVertGeomFrag(const std::string& vert_filename,
                               const std::string& geom_filename,
                               const std::string& frag_filename) {
  // Delete old shader/program
  Delete();

  const bool use_geom_shader = !geom_filename.empty();

  if (use_geom_shader) {
    debug_name_ = vert_filename + " + " + geom_filename + " + " + frag_filename;
  } else {
    debug_name_ = vert_filename + " + " + frag_filename;
  }

  std::string vert_source, geom_source, frag_source;
  if (!LoadFile(vert_filename, vert_source)) {
    Log::E("Failed to load vertex shader %s\n", vert_filename.c_str());
    return false;
  }
  vert_source = ExpandMacros(macros_, vert_source);

  if (use_geom_shader) {
    if (!LoadFile(geom_filename, geom_source)) {
      Log::E("Failed to load geometry shader %s\n", geom_filename.c_str());
      return false;
    }
    geom_source = ExpandMacros(macros_, geom_source);
  }

  if (!LoadFile(frag_filename, frag_source)) {
    Log::E("Failed to load fragment shader \"%s\"\n", frag_filename.c_str());
    return false;
  }
  frag_source = ExpandMacros(macros_, frag_source);

  if (!CompileShader(GL_VERTEX_SHADER, vert_source, &vert_shader_,
                     vert_filename)) {
    Log::E("Failed to compile vertex shader \"%s\"\n", vert_filename.c_str());
    return false;
  }

#ifdef __GL_H__
  if (use_geom_shader) {
    geom_source = ExpandMacros(macros_, geom_source);
    if (!CompileShader(GL_GEOMETRY_SHADER, geom_source,
                       &geom_shader_, geom_filename)) {
      Log::E("Failed to compile geometry shader \"%s\"\n",
             geom_filename.c_str());
      return false;
    }
  }
#endif

  if (!CompileShader(GL_FRAGMENT_SHADER, frag_source, &frag_shader_,
                     frag_filename)) {
    Log::E("Failed to compile fragment shader \"%s\"\n",
           frag_filename.c_str());
    return false;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vert_shader_);
  glAttachShader(program_, frag_shader_);
  if (use_geom_shader) {
    glAttachShader(program_, geom_shader_);
  }
  glLinkProgram(program_);

  if (!CheckLinkStatus()) {
    Log::E("Failed to link program \"%s\"\n", debug_name_.c_str());

    // dump shader source for reference
    Log::D("\n");
    Log::D("%s =\n", vert_filename.c_str());
    DumpShaderSource(vert_source);
    if (use_geom_shader) {
      Log::D("%s =\n", geom_filename.c_str());
      DumpShaderSource(geom_source);
    }
    Log::D("%s =\n", frag_filename.c_str());
    DumpShaderSource(frag_source);

    return false;
  }

  const int kMaxNameSize = 1028;
  static char name[kMaxNameSize];

  GLint num_attribs;
  glGetProgramiv(program_, GL_ACTIVE_ATTRIBUTES, &num_attribs);
  for (int i = 0; i < num_attribs; ++i) {
    Variable v;
    GLsizei str_len;
    glGetActiveAttrib(program_, i, kMaxNameSize, &str_len, &v.size, &v.type,
                      name);
    v.loc = glGetAttribLocation(program_, name);
    attribs_[name] = v;
  }

  GLint num_uniforms;
  glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  for (int i = 0; i < num_uniforms; ++i) {
    Variable v;
    GLsizei str_len;
    glGetActiveUniform(program_, i, kMaxNameSize, &str_len, &v.size, &v.type,
                       name);
    int loc = glGetUniformLocation(program_, name);
    v.loc = loc;
    uniforms_[name] = v;

    // Log::E("uniform[\"%s\"] loc = %d\n", name, loc);
  }

  return true;
}

bool Program::LoadCompute(const std::string& compute_filename) {
  // Delete old shader/program
  Delete();

  debug_name_ = compute_filename;

  GL_ERROR_CHECK("Program::LoadCompute begin");

#ifdef __GL_H__
  std::string compute_source;
  if (!LoadFile(compute_filename, compute_source)) {
    Log::E("Failed to load compute shader \"%s\"\n", compute_filename.c_str());
    return false;
  }

  GL_ERROR_CHECK("Program::LoadCompute LoadFile");

  compute_source = ExpandMacros(macros_, compute_source);
  if (!CompileShader(GL_COMPUTE_SHADER, compute_source, &compute_shader_,
                     compute_filename)) {
    Log::E("Failed to compile compute shader \"%s\"\n",
           compute_filename.c_str());
    return false;
  }

  GL_ERROR_CHECK("Program::LoadCompute CompileShader");

  program_ = glCreateProgram();
  glAttachShader(program_, compute_shader_);
  glLinkProgram(program_);

  GL_ERROR_CHECK("Program::LoadCompute Attach and Link");

  if (!CheckLinkStatus()) {
    Log::E("Failed to link program \"%s\"\n", debug_name_.c_str());

    // dump shader source for reference
    Log::D("\n");
    Log::D("%s =\n", compute_filename.c_str());
    DumpShaderSource(compute_source);

    return false;
  }

  const int kMaxNameSize = 1028;
  static char name[kMaxNameSize];

  GLint num_uniforms;
  glGetProgramiv(program_, GL_ACTIVE_UNIFORMS, &num_uniforms);
  for (int i = 0; i < num_uniforms; ++i) {
    Variable v;
    GLsizei str_len;
    glGetActiveUniform(program_, i, kMaxNameSize, &str_len, &v.size, &v.type,
                       name);
    int loc = glGetUniformLocation(program_, name);
    v.loc = loc;
    uniforms_[name] = v;
  }

  GL_ERROR_CHECK("Program::LoadCompute get uniforms");

  // AJT(TODO): build reflection info on shader storage blocks

  return true;
#else
  return false;
#endif
}

void Program::Bind() const {
  glUseProgram(program_);
}

int Program::GetUniformLoc(const std::string& name) const {
  auto iter = uniforms_.find(name);
  if (iter != uniforms_.end()) {
    return iter->second.loc;
  } else {
    assert(false);
    Log::W("Could not find uniform \"%s\" for program \"%s\"\n",
           name.c_str(), debug_name_.c_str());
    return 0;
  }
}

const Program::Variable& Program::GetUniformVar(const std::string& name) const {
  auto iter = uniforms_.find(name);
  if (iter != uniforms_.end()) {
    return iter->second;
  } else {
    Log::W("Could not find uniform \"%s\" for program \"%s\"\n",
           name.c_str(), debug_name_.c_str());
    assert(false);
    static Variable var;
    memset(&var, 0, sizeof(Variable));
    return var;
  }
}


int Program::GetAttribLoc(const std::string& name) const {
  auto iter = attribs_.find(name);
  if (iter != attribs_.end()) {
    return iter->second.loc;
  } else {
    Log::W("Could not find attrib \"%s\" for program \"%s\"\n",
           name.c_str(), debug_name_.c_str());
    assert(false);
    return 0;
  }
}

void Program::SetUniformRaw(int loc, uint32_t value) const {
  glUniform1ui(loc, value);
}

void Program::SetUniformRaw(int loc, int32_t value) const {
  glUniform1i(loc, value);
}

void Program::SetUniformRaw(int loc, float value) const {
  glUniform1f(loc, value);
}

void Program::SetUniformRaw(int loc, const glm::vec2& value) const {
  glUniform2fv(loc, 1, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc, const glm::vec3& value) const {
  glUniform3fv(loc, 1, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc, const glm::vec4& value) const {
  glUniform4fv(loc, 1, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc, const glm::mat2& value) const {
  glUniformMatrix2fv(loc, 1, GL_FALSE, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc, const glm::mat3& value) const {
  glUniformMatrix3fv(loc, 1, GL_FALSE, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc, const glm::mat4& value) const {
  glUniformMatrix4fv(loc, 1, GL_FALSE, reinterpret_cast<const float*>(&value));
}

void Program::SetUniformRaw(int loc,
                            const std::vector<glm::mat4>& value) const {
  glUniformMatrix4fv(loc, static_cast<GLsizei>(value.size()), GL_FALSE,
                     reinterpret_cast<const float*>(value.data()));
}

void Program::SetAttribRaw(int loc, float* values, size_t stride) const {
  glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), values);
  glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec2* values, size_t stride) const {
  glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), values);
  glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec3* values, size_t stride) const {
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), values);
  glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec4* values, size_t stride) const {
  glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                        static_cast<GLsizei>(stride), values);
  glEnableVertexAttribArray(loc);
}



void Program::Delete() {
  debug_name_ = "";

  if (vert_shader_ > 0) {
    glDeleteShader(vert_shader_);
    vert_shader_ = 0;
  }

  if (geom_shader_ > 0) {
    glDeleteShader(geom_shader_);
    geom_shader_ = 0;
  }

  if (frag_shader_ > 0) {
    glDeleteShader(frag_shader_);
    frag_shader_ = 0;
  }

  if (compute_shader_ > 0) {
    glDeleteShader(compute_shader_);
    compute_shader_ = 0;
  }

  if (program_ > 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }

  uniforms_.clear();
  attribs_.clear();
}

bool Program::CheckLinkStatus() {
  GLint linked;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);

  if (!linked) {
    Log::E("Failed to link shaders \"%s\"\n", debug_name_.c_str());
  }

  const GLint kMaxBufferLen = 4096;
  GLsizei buffer_len = 0;
  std::unique_ptr<char[]> buffer(new char[kMaxBufferLen]);
  glGetProgramInfoLog(program_, kMaxBufferLen, &buffer_len, buffer.get());
  if (buffer_len > 0) {
    if (linked) {
      Log::W("Warning during linking shaders \"%s\"\n", debug_name_.c_str());
    }
    Log::W("%s\n", buffer.get());
  }

#ifdef WARNINGS_AS_ERRORS
  if (!linked || buffer_len > 1)
#else
  if (!linked)
#endif
  {
    return false;
  }

  return true;
}

}  // namespace hyper
