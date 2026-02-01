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

class Image {
 public:
  Image();
  bool LoadBytes(const uint8_t* bytes, size_t num_bytes);
  bool Load(const std::string& filename_in);
  void MultiplyAlpha();
  void ConvertToRGBA();

  std::string filename;
  uint32_t width;
  uint32_t height;
  PixelFormat pixel_format;
  bool is_srgb;
  std::vector<uint8_t> data;
};

}  // namespace hyper
