/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/texture.h"

#include "src/glincludes.h"

#include "src/image.h"

namespace hyper {

static GLenum filter_type_to_gl[] = {
  GL_NEAREST,
  GL_LINEAR,
  GL_NEAREST_MIPMAP_NEAREST,
  GL_LINEAR_MIPMAP_NEAREST,
  GL_NEAREST_MIPMAP_LINEAR,
  GL_LINEAR_MIPMAP_LINEAR
};

static GLenum wrap_type_to_gl[] = {
  GL_REPEAT,
  GL_MIRRORED_REPEAT,
  GL_CLAMP_TO_EDGE,
#ifdef __GL_H__
  GL_MIRROR_CLAMP_TO_EDGE,
#else
  GL_CLAMP_TO_EDGE,
#endif
};

static GLenum pixel_format_to_gl[] = {
  GL_LUMINANCE,
  GL_LUMINANCE_ALPHA,
  GL_RGB,
  GL_RGBA
};

Texture::Texture(const Image& image, const Params& params) {
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  filter_type_to_gl[static_cast<int>(params.min_filter)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  filter_type_to_gl[static_cast<int>(params.mag_filter)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  wrap_type_to_gl[static_cast<int>(params.s_wrap)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  wrap_type_to_gl[static_cast<int>(params.t_wrap)]);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLenum pf = pixel_format_to_gl[static_cast<int>(image.pixel_format)];

  int internal_format = pf;

  if (image.is_srgb && pf == GL_RGB) {
    internal_format = GL_SRGB8;
  } else if (image.is_srgb && pf == GL_RGBA) {
    internal_format = GL_SRGB8_ALPHA8;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, image.width, image.height,
               0, pf, GL_UNSIGNED_BYTE, &image.data[0]);

  if (static_cast<int>(params.min_filter) >=
      static_cast<int>(FilterType::NearestMipmapNearest)) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }

  if (image.pixel_format == PixelFormat::RA ||
      image.pixel_format == PixelFormat::RGBA) {
    has_alpha_channel = true;
  }
}

Texture::Texture(uint32_t width, uint32_t height, uint32_t internal_format,
                 uint32_t format, uint32_t type, const Params& params) {
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  filter_type_to_gl[static_cast<int>(params.min_filter)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  filter_type_to_gl[static_cast<int>(params.mag_filter)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  wrap_type_to_gl[static_cast<int>(params.s_wrap)]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  wrap_type_to_gl[static_cast<int>(params.t_wrap)]);

  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height,
               0, format, type, nullptr);
}

Texture::~Texture() {
  glDeleteTextures(1, &texture);
}

void Texture::Bind(int unit) const {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, texture);
}

}  // namespace hyper
