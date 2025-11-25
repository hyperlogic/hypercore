/*
  Copyright (c) 2024 Anthony J. Thibault
  This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/vertexbuffer.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "src/glincludes.h"
#include "src/log.h"
#include "src/util.h"

namespace hyper {

#if defined(__GL_H__) && !defined(__APPLE__)
#define glBufferStorageShim glBufferStorage
#else
static void glBufferStorageShim(GLenum target, GLsizeiptr size,
                                const void* data, GLbitfield flags) {
  GLenum usage = 0;
  if (flags & GL_DYNAMIC_STORAGE_BIT) {
    if (flags & GL_MAP_READ_BIT) {
      usage = GL_DYNAMIC_READ;
    } else {
      usage = GL_DYNAMIC_DRAW;
    }
  } else {
    if (flags & GL_MAP_READ_BIT) {
      usage = GL_STATIC_READ;
    } else {
      usage = GL_STATIC_DRAW;
    }
  }
  glBufferData(target, size, data, usage);
}
#endif

BufferObject::BufferObject(int target_in, void* data, size_t size,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, size, data, flags);
  Unbind();
  element_size_ = 0;
  num_elements_ = 0;
}

BufferObject::BufferObject(int target_in, const std::vector<float>& data,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, sizeof(float) * data.size(),
                      reinterpret_cast<const void*>(data.data()), flags);
  Unbind();
  element_size_ = 1;
  num_elements_ = static_cast<int>(data.size());
}

BufferObject::BufferObject(int target_in, const std::vector<glm::vec2>& data,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, sizeof(glm::vec2) * data.size(),
                      reinterpret_cast<const void*>(data.data()), flags);
  Unbind();
  element_size_ = 2;
  num_elements_ = static_cast<int>(data.size());
}

BufferObject::BufferObject(int target_in, const std::vector<glm::vec3>& data,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, sizeof(glm::vec3) * data.size(),
                      reinterpret_cast<const void*>(data.data()), flags);
  Unbind();
  element_size_ = 3;
  num_elements_ = static_cast<int>(data.size());
}

BufferObject::BufferObject(int target_in, const std::vector<glm::vec4>& data,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, sizeof(glm::vec4) * data.size(),
                      reinterpret_cast<const void*>(data.data()), flags);
  Unbind();
  element_size_ = 4;
  num_elements_ = static_cast<int>(data.size());
}

BufferObject::BufferObject(int target_in, const std::vector<uint32_t>& data,
                           unsigned int flags) {
  target_ = target_in;
  glGenBuffers(1, &obj_);
  Bind();
  glBufferStorageShim(target_, sizeof(uint32_t) * data.size(),
                      reinterpret_cast<const void*>(data.data()), flags);
  Unbind();
  element_size_ = 1;
  num_elements_ = static_cast<int>(data.size());
}

BufferObject::~BufferObject() {
  glDeleteBuffers(1, &obj_);
}

void BufferObject::Bind() const {
  glBindBuffer(target_, obj_);
}

void BufferObject::Unbind() const {
  glBindBuffer(target_, 0);
}

void BufferObject::Update(const std::vector<float>& data) {
  Bind();
  glBufferSubData(target_, 0, sizeof(float) * data.size(),
                  reinterpret_cast<const void*>(data.data()));
  Unbind();
}

void BufferObject::Update(const std::vector<glm::vec2>& data) {
  Bind();
  glBufferSubData(target_, 0, sizeof(glm::vec2) * data.size(),
                  reinterpret_cast<const void*>(data.data()));
  Unbind();
}

void BufferObject::Update(const std::vector<glm::vec3>& data) {
  Bind();
  glBufferSubData(target_, 0, sizeof(glm::vec3) * data.size(),
                  reinterpret_cast<const void*>(data.data()));
  Unbind();
}

void BufferObject::Update(const std::vector<glm::vec4>& data) {
  Bind();
  glBufferSubData(target_, 0, sizeof(glm::vec4) * data.size(),
                  reinterpret_cast<const void*>(data.data()));
  Unbind();
}

void BufferObject::Update(const std::vector<uint32_t>& data) {
  Bind();
  glBufferSubData(target_, 0, sizeof(uint32_t) * data.size(),
                  reinterpret_cast<const void*>(data.data()));
  Unbind();
}

void BufferObject::Read(std::vector<uint32_t>& data) {
  Bind();
  size_t buffer_size = sizeof(uint32_t) * data.size();
  assert(buffer_size == (element_size_ * sizeof(uint32_t) * num_elements_));
  // void* raw_buffer = glMapBuffer(target_, GL_READ_ONLY);
  void* raw_buffer = glMapBufferRange(target_, 0, buffer_size, GL_MAP_READ_BIT);
  if (raw_buffer) {
    memcpy(reinterpret_cast<void*>(data.data()), raw_buffer, buffer_size);
  }
  glUnmapBuffer(target_);
  Unbind();
}

VertexArrayObject::VertexArrayObject() {
  glGenVertexArrays(1, &obj_);
}

VertexArrayObject::~VertexArrayObject() {
  glDeleteVertexArrays(1, &obj_);
}

void VertexArrayObject::Bind() const {
  glBindVertexArray(obj_);
}

void VertexArrayObject::Unbind() const {
  glBindVertexArray(0);
}

void VertexArrayObject::SetAttribBuffer(
    int loc, std::shared_ptr<BufferObject> attrib_buffer) {
  assert(attrib_buffer->target_ == GL_ARRAY_BUFFER);

  Bind();
  attrib_buffer->Bind();
  glVertexAttribPointer(loc, attrib_buffer->element_size_, GL_FLOAT, GL_FALSE,
                        0, nullptr);
  glEnableVertexAttribArray(loc);
  attrib_buffer->Unbind();
  attrib_buffer_vec_.push_back(attrib_buffer);
  Unbind();
}

void VertexArrayObject::SetElementBuffer(
    std::shared_ptr<BufferObject> element_buffer_in) {
  assert(element_buffer_in->target_ == GL_ELEMENT_ARRAY_BUFFER);
  element_buffer_ = element_buffer_in;

  Bind();
  element_buffer_in->Bind();
  Unbind();
}

void VertexArrayObject::DrawElements(int mode) const {
  Bind();
  glDrawElements(static_cast<GLenum>(mode), element_buffer_->num_elements_,
                 GL_UNSIGNED_INT, nullptr);
  Unbind();
}

}  // namespace hyper
