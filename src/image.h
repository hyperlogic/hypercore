/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace hyper {

enum class PixelFormat {
  R = 0,  // intensity
  RA,     // intensity alpha
  RGB,
  RGBA
};

struct Image {
  Image();
  bool Load(const std::string& filename);
  void MultiplyAlpha();
  void ConvertToRGBA();

  uint32_t width;
  uint32_t height;
  PixelFormat pixel_format;
  bool is_srgb;
  std::vector<uint8_t> data;
};

}  // namespace hyper
