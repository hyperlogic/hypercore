/*
  Copyright (c) 2024 Anthony J. Thibault
  This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/util.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

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

bool LoadFile(const std::string& filename, std::string& data) {
  std::ifstream ifs(FindFile(filename), std::ifstream::in);
  if (ifs.good()) {
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    data = std::move(content);
    return true;
  } else {
    return false;
  }
}

bool LoadBinaryFile(const std::string& filename, std::vector<uint8_t>& data) {
  std::ifstream ifs(FindFile(filename), std::ios::in | std::ios::binary);
  if (ifs.good()) {
    data.assign(std::istreambuf_iterator<char>(ifs),
                std::istreambuf_iterator<char>());
    return true;
  } else {
    return false;
  }
}

bool SaveFile(const std::string& filename, const std::string& data) {
  std::ofstream ofs(FindFile(filename), std::ofstream::out);
  if (ofs.good()) {
    ofs << data;
    return true;
  } else {
    return false;
  }
}

// returns the number of bytes to advance
// fills cp_out with the code point at p.
int NextCodePointUTF8(const char *str, uint32_t *code_point_out) {
  const uint8_t* p = (const uint8_t*)str;
  if ((*p & 0x80) == 0) {
    *code_point_out = *p;
    return 1;
  } else if ((*p & 0xe0) == 0xc0) {  // 110xxxxx 10xxxxxx
    *code_point_out = ((*p & ~0xe0) << 6) | (*(p+1) & ~0xc0);
    return 2;
  } else if ((*p & 0xf0) == 0xe0) {  // 1110xxxx 10xxxxxx 10xxxxxx
    *code_point_out = ((*p & ~0xf0) << 12) | ((*(p+1) & ~0xc0) << 6) |
                      (*(p+2) & ~0xc0);
    return 3;
  } else if ((*p & 0xf8) == 0xf0) {  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    *code_point_out = ((*p & ~0xf8) << 18) | ((*(p+1) & ~0xc0) << 12) |
                      ((*(p+1) & ~0xc0) << 6) | (*(p+2) & ~0xc0);
    return 4;
  } else {
    // p is not at a valid starting point. p is not utf8 encoded or is at a
    // bad offset.
    assert(0);
    *code_point_out = 0;
    return 1;
  }
}

glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& if_zero) {
  float len = glm::length(v);
  if (len > 0.0f) {
    return glm::normalize(v);
  } else {
    return if_zero;
  }
}

glm::quat SafeMix(const glm::quat& a, const glm::quat& b, float alpha) {
  // adjust signs if necessary
  glm::quat b_temp = b;
  float dot = glm::dot(a, b_temp);
  if (dot < 0.0f) {
    b_temp = -b_temp;
  }
  return glm::normalize(glm::lerp(a, b_temp, alpha));
}

glm::mat3 MakeMat3(const glm::quat& rotation) {
  glm::vec3 x_axis = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 y_axis = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 z_axis = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
  return glm::mat3(x_axis, y_axis, z_axis);
}

glm::mat3 MakeMat3(const glm::vec3& scale, const glm::quat& rotation) {
  glm::vec3 x_axis = rotation * glm::vec3(scale.x, 0.0f, 0.0f);
  glm::vec3 y_axis = rotation * glm::vec3(0.0f, scale.y, 0.0f);
  glm::vec3 z_axis = rotation * glm::vec3(0.0f, 0.0f, scale.z);
  return glm::mat3(x_axis, y_axis, z_axis);
}

glm::mat4 MakeMat3(float scale, const glm::quat& rotation) {
  return MakeMat3(glm::vec3(scale), rotation);
}

glm::mat4 MakeMat4(const glm::vec3& scale, const glm::quat& rotation,
                   const glm::vec3& translation) {
  glm::vec3 x_axis = rotation * glm::vec3(scale.x, 0.0f, 0.0f);
  glm::vec3 y_axis = rotation * glm::vec3(0.0f, scale.y, 0.0f);
  glm::vec3 z_axis = rotation * glm::vec3(0.0f, 0.0f, scale.z);
  return glm::mat4(glm::vec4(x_axis, 0.0f), glm::vec4(y_axis, 0.0f),
                   glm::vec4(z_axis, 0.0f), glm::vec4(translation, 1.0f));
}

glm::mat4 MakeMat4(float scale, const glm::quat& rotation,
                   const glm::vec3& translation) {
  return MakeMat4(glm::vec3(scale), rotation, translation);
}

glm::mat4 MakeMat4(const glm::quat& rotation, const glm::vec3& translation) {
  return MakeMat4(glm::vec3(1.0f), rotation, translation);
}

glm::mat4 MakeMat4(const glm::quat& rotation) {
  return MakeMat4(glm::vec3(1.0f), rotation, glm::vec3(0.0f));
}

glm::mat4 MakeMat4(const glm::mat3& m3, const glm::vec3& translation) {
  return glm::mat4(glm::vec4(m3[0], 0.0f), glm::vec4(m3[1], 0.0f),
                   glm::vec4(m3[2], 0.0f), glm::vec4(translation, 1.0f));
}

glm::mat4 MakeMat4(const glm::mat3& m3) {
  return MakeMat4(m3, glm::vec3(0.0f));
}

void Decompose(const glm::mat4& matrix, glm::vec3* scale_out,
               glm::quat* rotation_out, glm::vec3* translation_out) {
  glm::mat3 m(matrix);
  float det = glm::determinant(m);
  if (det < 0.0f) {
    // left handed matrix, flip sign to compensate.
    *scale_out = glm::vec3(-glm::length(m[0]), glm::length(m[1]),
                           glm::length(m[2]));
  } else {
    *scale_out = glm::vec3(glm::length(m[0]), glm::length(m[1]),
                           glm::length(m[2]));
  }

  // quat_cast doesn't work so well with scaled matrices, so cancel it out.
  glm::mat4 tmp = glm::scale(matrix, 1.0f / *scale_out);
  *rotation_out = glm::normalize(glm::quat_cast(tmp));

  *translation_out = glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
}

void Decompose(const glm::mat3& matrix, glm::vec3* scale_out,
               glm::quat* rotation_out) {
  float det = glm::determinant(matrix);
  if (det < 0.0f) {
    // left handed matrix, flip sign to compensate.
    *scale_out = glm::vec3(-glm::length(matrix[0]), glm::length(matrix[1]),
                           glm::length(matrix[2]));
  } else {
    *scale_out = glm::vec3(glm::length(matrix[0]), glm::length(matrix[1]),
                           glm::length(matrix[2]));
  }

  // quat_cast doesn't work so well with scaled matrices, so cancel it out.
  glm::mat3 tmp;
  tmp[0] = matrix[0] * 1.0f / scale_out->x;
  tmp[1] = matrix[1] * 1.0f / scale_out->y;
  tmp[2] = matrix[2] * 1.0f / scale_out->z;
  *rotation_out = glm::normalize(glm::quat_cast(tmp));
}

void DecomposeSwingTwist(const glm::quat& rotation,
                         const glm::vec3& twist_axis, glm::quat* swing_out,
                         glm::quat* twist_out) {
  glm::vec3 d = glm::normalize(twist_axis);

  // the twist part has an axis (imaginary component) that is parallel to
  // twist_axis argument
  glm::vec3 axis_of_rotation(rotation.x, rotation.y, rotation.z);
  glm::vec3 twist_imaginary_part = glm::dot(d, axis_of_rotation) * d;

  // and a real component that is relatively proportional to rotation's real
  // component
  *twist_out = glm::normalize(glm::quat(rotation.w, twist_imaginary_part.x,
                                        twist_imaginary_part.y,
                                        twist_imaginary_part.z));

  // once twist is known we can solve for swing:
  // rotation = swing * twist  -->  swing = rotation * invTwist
  *swing_out = rotation * glm::inverse(*twist_out);
}

glm::vec3 XformPoint(const glm::mat4& m, const glm::vec3& p) {
  glm::vec4 temp(p, 1.0f);
  glm::vec4 result = m * temp;
  return glm::vec3(result.x / result.w, result.y / result.w,
                   result.z / result.w);
}

glm::vec3 XformVec(const glm::mat4& m, const glm::vec3& v) {
  return glm::mat3(m) * v;
}

glm::vec3 XformVecSlow(const glm::mat4& m, const glm::vec3& v) {
  return glm::mat3(m) * v;
}

glm::vec3 RandomColor() {
  return glm::vec3(glm::linearRand(0.0f, 1.0f), glm::linearRand(0.0f, 1.0f),
                   glm::linearRand(0.0f, 1.0f));
}

void PrintMat(const glm::mat4& m4, const std::string& name) {
  Log::D("%s =\n", name.c_str());
  Log::D("    | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][0], m4[1][0],
         m4[2][0], m4[3][0]);
  Log::D("    | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][1], m4[1][1],
         m4[2][1], m4[3][1]);
  Log::D("    | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][2], m4[1][2],
         m4[2][2], m4[3][2]);
  Log::D("    | %10.5f, %10.5f, %10.5f, %10.5f |\n", m4[0][3], m4[1][3],
         m4[2][3], m4[3][3]);
}

void PrintMat(const glm::mat3& m3, const std::string& name) {
  Log::D("%s =\n", name.c_str());
  Log::D("    | %10.5f, %10.5f, %10.5f |\n", m3[0][0], m3[1][0], m3[2][0]);
  Log::D("    | %10.5f, %10.5f, %10.5f |\n", m3[0][1], m3[1][1], m3[2][1]);
  Log::D("    | %10.5f, %10.5f, %10.5f |\n", m3[0][2], m3[1][2], m3[2][2]);
}

void PrintMat(const glm::mat2& m2, const std::string& name) {
  Log::D("%s =\n", name.c_str());
  Log::D("    | %10.5f, %10.5f |\n", m2[0][0], m2[1][0]);
  Log::D("    | %10.5f, %10.5f |\n", m2[0][1], m2[1][1]);
}

void PrintVec(const glm::vec4& v4, const std::string& name) {
  Log::D("%s = ( %.5f, %.5f, %.5f, %.5f )\n", name.c_str(), v4.x, v4.y, v4.z,
         v4.w);
}

void PrintVec(const glm::vec3& v3, const std::string& name) {
  Log::D("%s = ( %.5f, %.5f, %.5f )\n", name.c_str(), v3.x, v3.y, v3.z);
}

void PrintVec(const glm::vec2& v2, const std::string& name) {
  Log::D("%s = ( %.5f, %.5f )\n", name.c_str(), v2.x, v2.y);
}

void PrintQuat(const glm::quat& q, const std::string& name) {
  Log::D("%s = ( %.5f, ( %.5f, %.5f, %.5f ) )\n", name.c_str(), q.x, q.y, q.z,
         q.w);
}

bool FuzzyEquals(float lhs, float rhs, float epsilon) {
  return std::abs(lhs - rhs) < epsilon;
}

bool FuzzyEquals(const glm::vec2& lhs, const glm::vec2& rhs, float epsilon) {
  return FuzzyEquals(lhs.x, rhs.x, epsilon) &&
      FuzzyEquals(lhs.y, rhs.y, epsilon);
}

bool FuzzyEquals(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon) {
  return FuzzyEquals(lhs.x, rhs.x, epsilon) &&
      FuzzyEquals(lhs.y, rhs.y, epsilon) &&
      FuzzyEquals(lhs.z, rhs.z, epsilon);
}

bool FuzzyEquals(const glm::vec4& lhs, const glm::vec4& rhs, float epsilon) {
  return FuzzyEquals(lhs.x, rhs.x, epsilon) &&
      FuzzyEquals(lhs.y, rhs.y, epsilon) &&
      FuzzyEquals(lhs.z, rhs.z, epsilon) &&
      FuzzyEquals(lhs.w, rhs.w, epsilon);
}

bool FuzzyEquals(const glm::quat& lhs, const glm::quat& rhs, float epsilon) {
  // Check if quaternions are close
  // considering both q and -q represent the same rotation
  float dot = glm::dot(lhs, rhs);
  if (dot < 0.0f) {
    // If dot product is negative, compare with negated quaternion
    return FuzzyEquals(lhs.x, -rhs.x, epsilon) &&
        FuzzyEquals(lhs.y, -rhs.y, epsilon) &&
        FuzzyEquals(lhs.z, -rhs.z, epsilon) &&
        FuzzyEquals(lhs.w, -rhs.w, epsilon);
  } else {
    return FuzzyEquals(lhs.x, rhs.x, epsilon) &&
        FuzzyEquals(lhs.y, rhs.y, epsilon) &&
        FuzzyEquals(lhs.z, rhs.z, epsilon) &&
        FuzzyEquals(lhs.w, rhs.w, epsilon);
  }
}

static std::vector<std::string> search_paths = [] {
  std::vector<std::string> paths;
#ifdef SHIPPING
  paths.push_back("");
#else
#if defined(__linux__) || defined(__APPLE__)
  // enables us to run from the build dir
  paths.push_back("../");
  paths.push_back("../hypercore/");
#else
  // enables us to run from the build/Debug dir
  paths.push_back("../../");
  paths.push_back("../../hypercore/");
#endif
#endif
  return paths;
}();

void AppendSearchPath(const std::string& path) {
  search_paths.push_back(path);
}

std::string FindFile(const std::string& filename) {
  // If filename is absolute or starts with ./ or ../, use it directly
  if (!filename.empty() && (filename[0] == '/' || filename[0] == '\\' ||
      (filename.size() >= 2 && filename[0] == '.' &&
       (filename[1] == '/' || filename[1] == '\\')) ||
      (filename.size() >= 3 && filename[0] == '.' &&
       filename[1] == '.' && (filename[2] == '/' || filename[2] == '\\')))) {
    return filename;
  }

  // Search through all paths
  for (const auto& search_path : search_paths) {
    std::string full_path = search_path + filename;
    std::ifstream test(full_path);
    if (test.good()) {
      return full_path;
    }
  }

  // If not found, return filename as-is (will fail when opened)
  return filename;
}

bool PointInsideAABB(const glm::vec3& point, const glm::vec3& aabb_min,
                     const glm::vec3& aabb_max) {
  return (point.x >= aabb_min.x && point.x <= aabb_max.x) &&
         (point.y >= aabb_min.y && point.y <= aabb_max.y) &&
         (point.z >= aabb_min.z && point.z <= aabb_max.z);
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

glm::vec4 LinearToSRGB(const glm::vec4& linear_color) {
  glm::vec4 srgb_color;

  for (int i = 0; i < 3; ++i) {
    srgb_color[i] = LinearToSRGB(linear_color[i]);
  }
  srgb_color.a = linear_color.a;
  return srgb_color;
}

glm::vec4 SRGBToLinear(const glm::vec4& srgb_color) {
  glm::vec4 linear_color;
  for (int i = 0; i < 3; ++i) {  // Convert RGB, leave A unchanged
    linear_color[i] = SRGBToLinear(srgb_color[i]);
  }
  linear_color.a = srgb_color.a;  // Copy alpha channel directly
  return linear_color;
}

glm::mat4 MakeRotateAboutPointMat(const glm::vec3& pos, const glm::quat& rot) {
  glm::mat4 pos_mat = MakeMat4(glm::quat(), pos);
  glm::mat4 inv_pos_mat = MakeMat4(glm::quat(), -pos);
  glm::mat4 rot_mat = MakeMat4(rot);
  return pos_mat * rot_mat * inv_pos_mat;
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

void StrCpy_s(char* dest, size_t destsz, const char* src) {
#ifdef WIN32
  strcpy_s(dest, destsz, src);
#else
  snprintf(dest, destsz, "%s", src);
#endif
}

glm::vec2 ToPlane(const glm::vec3& vec3) {
  return glm::vec2(vec3.x, vec3.z);
}

glm::vec3 FromPlane(const glm::vec2& vec2) {
  return glm::vec3(vec2.x, 0.0f, vec2.y);
}

glm::quat RotationBetweenVectors(glm::vec3 from, glm::vec3 to) {
  from = glm::normalize(from);
  to = glm::normalize(to);

  float cosTheta = glm::dot(from, to);

  // Vectors point in opposite directions — pick an arbitrary perpendicular axis
  if (cosTheta < -1.0f + 1e-6f) {
    glm::vec3 axis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), from);
    if (glm::dot(axis, axis) < 1e-6f)
      axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), from);
    return glm::angleAxis(glm::radians(180.0f), glm::normalize(axis));
  }

  glm::vec3 axis = glm::cross(from, to);

  // Half-angle trick: avoids computing acos
  float s = sqrtf((1.0f + cosTheta) * 2.0f);
  return glm::quat(s * 0.5f, axis.x / s, axis.y / s, axis.z / s);
}

int RaySphereIntersect(glm::vec3 ray_point, glm::vec3 ray_dir,
                       glm::vec3 sphere_center, float sphere_radius,
                       float* result_1, float* result_2) {
  glm::vec3 l = ray_point - sphere_center;
  float a = glm::dot(ray_dir, ray_dir);
  float b = 2.0f * glm::dot(ray_dir, l);
  float c = glm::dot(l, l) - sphere_radius * sphere_radius;
  float disc = glm::sqrt((b * b) - (4.0f * a * c));
  if (disc > 0.0f) {
    *result_1 = (-b * disc) / (2.0f * a);
    *result_2 = (b * disc) / (2.0f * a);
    return 2;
  } else if (disc == 0.0f) {
    *result_1 = (b * disc) / (2.0f * a);
    return 1;
  } else {
    return 0;
  }
}

void ComputePickRay(glm::ivec2 screen_pos, const glm::mat4& camera_mat,
                    const glm::mat4& proj_mat, const glm::vec4& viewport,
                    const glm::vec2& near_far, glm::vec3* ray_point,
                    glm::vec3* ray_dir) {
  // Convert screen position to normalized device coordinates.
  float ndc_x = (static_cast<float>(screen_pos.x) - viewport.x) /
                viewport.z * 2.0f - 1.0f;
  float ndc_y = (static_cast<float>(screen_pos.y) - viewport.y) /
                viewport.w * 2.0f - 1.0f;

  // Unproject from NDC to view space using the inverse projection matrix.
  glm::mat4 inv_proj = glm::inverse(proj_mat);

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
  glm::vec3 near_world = glm::vec3(camera_mat * near_view);
  glm::vec3 far_world = glm::vec3(camera_mat * far_view);

  *ray_point = near_world;
  *ray_dir = glm::normalize(far_world - near_world);
}

}  // namespace hyper
