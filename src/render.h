/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>

#include "src/glincludes.h"

#ifndef NDEBUG
#define GL_ERROR_CHECK(x) GLErrorCheck(x)
void GLErrorCheck(const char* message);
#else
#define GL_ERROR_CHECK(x)
#endif

namespace hyper {

struct RenderParams {
  glm::mat4 camera_mat;
  glm::mat4 proj_mat;
  glm::vec4 viewport;
  glm::vec2 near_far;
};

struct EnvIrradianceSH {
  EnvIrradianceSH();
  // "Peter-Pike Sloan" packing — 4 vec4s per channel, 12 vec4s total = 48 floats, "Stupid SH Tricks"
  glm::vec4 r_sh0;  // sh coeff for red channel (up to third-order)
  glm::vec4 r_sh1;
  glm::vec4 r_sh2;
  glm::vec4 r_sh3;
  glm::vec4 g_sh0;  // sh coeff for green channel
  glm::vec4 g_sh1;
  glm::vec4 g_sh2;
  glm::vec4 g_sh3;
  glm::vec4 b_sh0;  // sh coeff for blue channel
  glm::vec4 b_sh1;
  glm::vec4 b_sh2;
  glm::vec4 b_sh3;
};

struct LightingParams {
  LightingParams();
  glm::vec3 direct_dir;
  glm::vec3 direct_color;
  glm::vec3 ambient_color;
  EnvIrradianceSH env_irr_sh;
};

float LinearToSRGB(float linear);
float SRGBToLinear(float srgb);
glm::vec3 LinearToSRGB(const glm::vec3& linear_color);
glm::vec4 LinearToSRGB(const glm::vec4& linear_color);
glm::vec3 SRGBToLinear(const glm::vec3& srgb_color);
glm::vec4 SRGBToLinear(const glm::vec4& srgb_color);

enum GraphicsAPI {
  GRAPHICS_VULKAN,
  GRAPHICS_OPENGL,
  GRAPHICS_OPENGL_ES,
  GRAPHICS_D3D
};
void CreateProjection(float* m, GraphicsAPI graphics_api,
                      const float tan_angle_left,
                      const float tan_angle_right, const float tan_angle_up,
                      float const tan_angle_down, const float near_z,
                      const float far_z);

// Create a pick ray from screen pos and RenderParams
void ComputePickRay(glm::ivec2 screen_pos, const RenderParams& render_params,
                    glm::vec3* ray_point, glm::vec3* ray_dir);


// just the zero'th order dc
glm::vec3 ComputeColorFromSH(float dc_0, float dc_1, float dc_2);



} // namespace hyper
