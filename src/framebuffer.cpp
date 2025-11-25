/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/framebuffer.h"

#include "src/glincludes.h"
#include "src/texture.h"

namespace hyper {

FrameBuffer::FrameBuffer() {
  glGenFramebuffers(1, &fbo);
}

FrameBuffer::~FrameBuffer() {
  glDeleteFramebuffers(1, &fbo);
  fbo = 0;
}

void FrameBuffer::Bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void FrameBuffer::AttachColor(std::shared_ptr<Texture> color_tex) {
  Bind();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_tex->texture, 0);
  color_attachment_ = color_tex;
}

void FrameBuffer::AttachDepth(std::shared_ptr<Texture> depth_tex) {
  Bind();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depth_tex->texture, 0);
  depth_attachment_ = depth_tex;
}

void FrameBuffer::AttachStencil(std::shared_ptr<Texture> stencil_tex) {
  Bind();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                         stencil_tex->texture, 0);
  stencil_attachment_ = stencil_tex;
}

bool FrameBuffer::IsComplete() const {
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

}  // namespace hyper
