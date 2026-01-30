/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/image.h"

#define STB_IMAGE_IMPLEMENTATION  // Include this only once in your project
#include <stb_image.h>

#include <string>
#include <vector>

#include "src/log.h"
#include "src/util.h"

namespace hyper {

Image::Image() : filename("?"), width(0), height(0),
                 pixel_format(PixelFormat::R), is_srgb(false) {
}

bool Image::LoadBytes(const uint8_t* bytes, size_t num_bytes) {
  // Load the image using stb_image
  int w, h, channels;
  unsigned char* image_data = stbi_load_from_memory(
      bytes, static_cast<int>(num_bytes), &w, &h, &channels, 0);
  if (!image_data) {
    Log::E("Failed to load texture \"%s\": %s\n", filename.c_str(),
           stbi_failure_reason());
    return false;
  }

  // Determine pixel format based on channels
  int pixel_size;
  switch (channels) {
    case 1:
      pixel_format = PixelFormat::R;
      pixel_size = 1;
      break;
    case 2:
      pixel_format = PixelFormat::RA;
      pixel_size = 2;
      break;
    case 3:
      pixel_format = PixelFormat::RGB;
      pixel_size = 3;
      break;
    case 4:
      pixel_format = PixelFormat::RGBA;
      pixel_size = 4;
      break;
    default:
      Log::E("Failed to load texture \"%s\": unsupported channel count %d\n",
             filename.c_str(), channels);
      stbi_image_free(image_data);
      return false;
  }

  width = w;
  height = h;
  data.resize(width * height * pixel_size);

  // Copy image data, flipping vertically
  // (stb_image loads top-to-bottom, OpenGL expects bottom-to-top)
  for (uint32_t i = 0; i < height; ++i) {
    memcpy(&data[0] + i * width * pixel_size,
           image_data + (height - 1 - i) * width * pixel_size,
           width * pixel_size);
  }

  // Free stb_image data
  stbi_image_free(image_data);

  // pre-multiply alpha
  MultiplyAlpha();

  // AJT(TODO) should probably check if the sRGB color space chunk is present
  is_srgb = true;

  return true;
}

bool Image::Load(const std::string& filename_in) {
  filename = FindFile(filename_in);
  std::vector<uint8_t> data_vec;
  if (!LoadBinaryFile(filename, data_vec)) {
    Log::E("Failed to load texture \"%s\": LoadBinaryFile failed\n",
           filename.c_str());
  }
  return LoadBytes(data_vec.data(), data_vec.size());
}

void Image::MultiplyAlpha() {
  if (pixel_format == PixelFormat::R || pixel_format == PixelFormat::RGB) {
    return;
  } else if (pixel_format == PixelFormat::RA) {
    size_t pixel_size = 2;
    for (size_t i = 0; i < width * height; i++) {
      float red = static_cast<float>(data[i * pixel_size]) / 255.0f;
      float alpha = static_cast<float>(data[i * pixel_size + 1]) / 255.0f;
      data[i * pixel_size] = static_cast<uint8_t>((red * alpha) * 255.0f);
    }
  } else if (pixel_format == PixelFormat::RGBA) {
    size_t pixel_size = 4;
    for (size_t i = 0; i < width * height; i++) {
      float red = static_cast<float>(data[i * pixel_size]) / 255.0f;
      float green = static_cast<float>(data[i * pixel_size + 1]) / 255.0f;
      float blue = static_cast<float>(data[i * pixel_size + 2]) / 255.0f;
      float alpha = static_cast<float>(data[i * pixel_size + 3]) / 255.0f;
      data[i * pixel_size] = static_cast<uint8_t>((red * alpha) * 255.0f);
      data[i * pixel_size + 1] = static_cast<uint8_t>((green * alpha) * 255.0f);
      data[i * pixel_size + 2] = static_cast<uint8_t>((blue * alpha) * 255.0f);
    }
  }
}

void Image::ConvertToRGBA() {
  if (pixel_format == PixelFormat::RGBA) {
    return;
  }
  uint32_t old_pixel_size = static_cast<uint32_t>(pixel_format) + 1;
  uint32_t new_pixel_size = 4;
  std::vector<uint8_t> new_data(width * height * new_pixel_size, 0);

  if (pixel_format == PixelFormat::R) {
    for (uint32_t y = 0; y < width; y++) {
      for (uint32_t x = 0; x < height; x++) {
        uint32_t i = (y * width + x) * old_pixel_size;
        uint8_t r = data[i + 0];

        uint32_t j = (y * width + x) * new_pixel_size;
        new_data[j + 0] = r;
        new_data[j + 1] = r;
        new_data[j + 2] = r;
        new_data[j + 3] = 255;
      }
    }
    data = new_data;
    pixel_format = PixelFormat::RGBA;
  } else if (pixel_format == PixelFormat::RA) {
    for (uint32_t y = 0; y < width; y++) {
      for (uint32_t x = 0; x < height; x++) {
        uint32_t i = (y * width + x) * old_pixel_size;
        uint8_t a = data[i + 0];
        uint8_t r = data[i + 1];

        uint32_t j = (y * width + x) * new_pixel_size;
        new_data[j + 0] = r;  // r
        new_data[j + 1] = r;  // g
        new_data[j + 2] = r;  // b
        new_data[j + 3] = a;  // a
      }
    }
    data = new_data;
    pixel_format = PixelFormat::RGBA;
  } else if (pixel_format == PixelFormat::RGB) {
    for (uint32_t y = 0; y < width; y++) {
      for (uint32_t x = 0; x < height; x++) {
        uint32_t i = (y * width + x) * old_pixel_size;
        uint8_t r = data[i + 0];
        uint8_t g = data[i + 1];
        uint8_t b = data[i + 2];

        uint32_t j = (y * width + x) * new_pixel_size;
        new_data[j + 0] = r;
        new_data[j + 1] = g;
        new_data[j + 2] = b;
        new_data[j + 3] = 255;
      }
    }
    data = new_data;
    pixel_format = PixelFormat::RGBA;
  }
}

}  // namespace hyper
