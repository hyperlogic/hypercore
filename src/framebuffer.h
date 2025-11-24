/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <memory>

struct Texture;

struct FrameBuffer {
  FrameBuffer();
  ~FrameBuffer();

  void Bind() const;
  void AttachColor(std::shared_ptr<Texture> color_tex);
  void AttachDepth(std::shared_ptr<Texture> depth_tex);
  void AttachStencil(std::shared_ptr<Texture> stencil_tex);

  bool IsComplete() const;

  std::shared_ptr<Texture> GetColorTexture() const { return color_attachment_; }
  std::shared_ptr<Texture> GetDepthTexture() const { return depth_attachment_; }
  std::shared_ptr<Texture> GetStencilTexture() const {
    return stencil_attachment_;
  }

  uint32_t fbo;
  std::shared_ptr<Texture> color_attachment_;
  std::shared_ptr<Texture> depth_attachment_;
  std::shared_ptr<Texture> stencil_attachment_;
};
