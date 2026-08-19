/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/render.h"

#include "src/glincludes.h"
#include "src/log.h"

#ifndef NDEBUG
// If there is a glError this outputs it along with a message to stderr.
// otherwise there is no output.
void GLErrorCheck(const char* message) {
  GLenum val = glGetError();
  switch (val) {
    case GL_INVALID_ENUM:
      hyper::Log::D("GL_INVALID_ENUM : %s\n", message);
      break;
    case GL_INVALID_VALUE:
      hyper::Log::D("GL_INVALID_VALUE : %s\n", message);
      break;
    case GL_INVALID_OPERATION:
      hyper::Log::D("GL_INVALID_OPERATION : %s\n", message);
      break;
#ifndef GL_ES_VERSION_2_0
    case GL_STACK_OVERFLOW:
      hyper::Log::D("GL_STACK_OVERFLOW : %s\n", message);
      break;
    case GL_STACK_UNDERFLOW:
      hyper::Log::D("GL_STACK_UNDERFLOW : %s\n", message);
      break;
#endif
    case GL_OUT_OF_MEMORY:
      hyper::Log::D("GL_OUT_OF_MEMORY : %s\n", message);
      break;
    case GL_NO_ERROR:
      break;
  }
}
#endif

namespace hyper {

static const glm::vec3 kLightDir(0.2f, 1.0f, 0.2f);
static const float kLightIntensity = 1.0f;
static const glm::vec3 kLightColor(1.0f, 1.0f, 1.0f);
static const glm::vec3 kAmbientColor(0.2f, 0.2f, 0.2f);
static const float kEnvIntensity = 0.5f;

// default to a bright outdoor scene
EnvIrradianceSH::EnvIrradianceSH()
    : r_sh0(kEnvIntensity * glm::vec4(0.000247f,  0.067317f,  0.049472f,  0.044667f)),
      r_sh1(kEnvIntensity * glm::vec4(0.067317f, -0.000247f,  0.038672f, -0.016214f)),
      r_sh2(kEnvIntensity * glm::vec4(0.049472f,  0.038672f,  0.041687f, -0.063235f)),
      r_sh3(kEnvIntensity * glm::vec4(0.044667f, -0.016214f, -0.063235f,  0.732629f)),
      g_sh0(kEnvIntensity * glm::vec4(0.005058f,  0.063251f,  0.042333f,  0.042035f)),
      g_sh1(kEnvIntensity * glm::vec4(0.063251f, -0.005058f,  0.032467f,  0.063713f)),
      g_sh2(kEnvIntensity * glm::vec4(0.042333f,  0.032467f,  0.003732f, -0.054176f)),
      g_sh3(kEnvIntensity * glm::vec4(0.042035f,  0.063713f, -0.054176f,  0.800368f)),
      b_sh0(kEnvIntensity * glm::vec4(0.003552f,  0.046977f,  0.030923f,  0.025932f)),
      b_sh1(kEnvIntensity * glm::vec4(0.046977f, -0.003552f,  0.024697f,  0.184137f)),
      b_sh2(kEnvIntensity * glm::vec4(0.030923f,  0.024697f, -0.052322f, -0.042096f)),
      b_sh3(kEnvIntensity * glm::vec4(0.025932f,  0.184137f, -0.042096f,  0.955088f)) {
}

LightingParams::LightingParams()
    : direct_dir(glm::normalize(kLightDir)),
      direct_color(kLightIntensity * kLightColor),
      ambient_color(kAmbientColor),
      env_irr_sh() {
}

float LinearToSRGB(float linear) {
  if (linear <= 0.0031308f) {
    return 12.92f * linear;
  } else {
    return 1.055f * glm::pow(linear, 1.0f / 2.4f) - 0.055f;
  }
}

float SRGBToLinear(float srgb) {
  if (srgb <= 0.04045f) {
    return srgb / 12.92f;
  } else {
    return glm::pow((srgb + 0.055f) / 1.055f, 2.4f);
  }
}

glm::vec3 LinearToSRGB(const glm::vec3& linear_color) {
  glm::vec3 srgb_color;
  for (int i = 0; i < 3; ++i) {
    srgb_color[i] = LinearToSRGB(linear_color[i]);
  }
  return srgb_color;
}

glm::vec4 LinearToSRGB(const glm::vec4& linear_color) {
  glm::vec4 srgb_color;

  for (int i = 0; i < 3; ++i) {
    srgb_color[i] = LinearToSRGB(linear_color[i]);
  }
  srgb_color.a = linear_color.a;
  return srgb_color;
}

glm::vec3 SRGBToLinear(const glm::vec3& srgb_color) {
  glm::vec3 linear_color;
  for (int i = 0; i < 3; ++i) {  // Convert RGB, leave A unchanged
    linear_color[i] = SRGBToLinear(srgb_color[i]);
  }
  return linear_color;
}

glm::vec4 SRGBToLinear(const glm::vec4& srgb_color) {
  glm::vec4 linear_color;
  for (int i = 0; i < 3; ++i) {  // Convert RGB, leave A unchanged
    linear_color[i] = SRGBToLinear(srgb_color[i]);
  }
  linear_color.a = srgb_color.a;  // Copy alpha channel directly
  return linear_color;
}

// Creates a projection matrix based on the specified dimensions.
// The projection matrix transforms -Z=forward, +Y=up, +X=right to the
// appropriate clip space for the graphics API.
// The far plane is placed at infinity if farZ <= nearZ.
// An infinite projection matrix is preferred for rasterization because, except
// for things *right* up against the near plane, it always provides better
// precision:
//              "Tightening the Precision of Perspective Rendering"
//              Paul Upchurch, Mathieu Desbrun
//              Journal of Graphics Tools, Volume 16, Issue 1, 2012
void CreateProjection(float* m, GraphicsAPI graphics_api,
                      const float tan_angle_left,
                      const float tan_angle_right, const float tan_angle_up,
                      float const tan_angle_down, const float near_z,
                      const float far_z) {
  const float tan_angle_width = tan_angle_right - tan_angle_left;

  // Set to tanAngleDown - tanAngleUp for a clip space with positive Y down
  // (Vulkan).
  // Set to tanAngleUp - tanAngleDown for a clip space with positive Y up
  // (OpenGL / D3D / Metal).
  const float tan_angle_height =
      graphics_api == GRAPHICS_VULKAN ? (tan_angle_down - tan_angle_up)
                                      : (tan_angle_up - tan_angle_down);

  // Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
  // Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
  const float offset_z = (graphics_api == GRAPHICS_OPENGL ||
                          graphics_api == GRAPHICS_OPENGL_ES)
                             ? near_z
                             : 0;

  if (far_z <= near_z) {
    // place the far plane at infinity
    m[0] = 2 / tan_angle_width;
    m[4] = 0;
    m[8] = (tan_angle_right + tan_angle_left) / tan_angle_width;
    m[12] = 0;

    m[1] = 0;
    m[5] = 2 / tan_angle_height;
    m[9] = (tan_angle_up + tan_angle_down) / tan_angle_height;
    m[13] = 0;

    m[2] = 0;
    m[6] = 0;
    m[10] = -1;
    m[14] = -(near_z + offset_z);

    m[3] = 0;
    m[7] = 0;
    m[11] = -1;
    m[15] = 0;
  } else {
    // normal projection
    m[0] = 2 / tan_angle_width;
    m[4] = 0;
    m[8] = (tan_angle_right + tan_angle_left) / tan_angle_width;
    m[12] = 0;

    m[1] = 0;
    m[5] = 2 / tan_angle_height;
    m[9] = (tan_angle_up + tan_angle_down) / tan_angle_height;
    m[13] = 0;

    m[2] = 0;
    m[6] = 0;
    m[10] = -(far_z + offset_z) / (far_z - near_z);
    m[14] = -(far_z * (near_z + offset_z)) / (far_z - near_z);

    m[3] = 0;
    m[7] = 0;
    m[11] = -1;
    m[15] = 0;
  }
}

void ComputePickRay(glm::ivec2 screen_pos, const RenderParams& render_params,
                    glm::vec3* ray_point, glm::vec3* ray_dir) {
  // Convert screen position to normalized device coordinates.
  float ndc_x = (static_cast<float>(screen_pos.x) - render_params.viewport.x) /
                render_params.viewport.z * 2.0f - 1.0f;
  float ndc_y = (static_cast<float>(screen_pos.y) - render_params.viewport.y) /
                render_params.viewport.w * 2.0f - 1.0f;

  // Unproject from NDC to view space using the inverse projection matrix.
  glm::mat4 inv_proj = glm::inverse(render_params.proj_mat);

  // Near and far points in clip space (OpenGL convention: z in [-1, 1]).
  glm::vec4 near_clip(ndc_x, ndc_y, -1.0f, 1.0f);
  glm::vec4 far_clip(ndc_x, ndc_y, 1.0f, 1.0f);

  // Transform to view space with perspective divide.
  glm::vec4 near_view = inv_proj * near_clip;
  near_view /= near_view.w;
  glm::vec4 far_view = inv_proj * far_clip;
  far_view /= far_view.w;

  // Transform from view space to world space using the camera matrix
  // (inverse view matrix).
  glm::vec3 near_world = glm::vec3(render_params.camera_mat * near_view);
  glm::vec3 far_world = glm::vec3(render_params.camera_mat * far_view);

  *ray_point = near_world;
  *ray_dir = glm::normalize(far_world - near_world);
}

glm::vec3 ComputeColorFromSH(float dc_0, float dc_1, float dc_2) {
  // zeroth order
  // (/ 1.0 (* 2.0 (sqrt pi)))
  constexpr float SH_C0 = 0.28209479177387814f;
  return glm::vec3(0.5f + SH_C0 * dc_0, 0.5f + SH_C0 * dc_1, 0.5f * SH_C0 * dc_2);
}

}  // namespace hyper
