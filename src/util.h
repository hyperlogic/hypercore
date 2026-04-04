/*
  Copyright (c) 2024 Anthony J. Thibault
  This software is licensed under the MIT License. See LICENSE for more details.
*/

// helper functions

#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#ifndef NDEBUG
#define GL_ERROR_CHECK(x) GLErrorCheck(x)
void GLErrorCheck(const char* message);
#else
#define GL_ERROR_CHECK(x)
#endif

#define DISABLE_COPY_AND_MOVE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName(ClassName&&) = delete; \
    ClassName& operator=(ClassName&&) = delete

namespace hyper {

// returns true on success, false on failure
bool LoadFile(const std::string& filename, std::string& result);
bool LoadBinaryFile(const std::string& filename, std::vector<uint8_t>& data);
bool SaveFile(const std::string& filename, const std::string& data);

// Iterate over codepoints in a utf-8 encoded string
int NextCodePointUTF8(const char *str, uint32_t *code_point_out);

glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& if_zero);
glm::quat SafeMix(const glm::quat& a, const glm::quat& b, float alpha);

glm::mat3 MakeMat3(const glm::quat& rotation);
glm::mat3 MakeMat3(const glm::vec3& scale, const glm::quat& rotation);
glm::mat4 MakeMat3(float scale, const glm::quat& rotation);

glm::mat4 MakeMat4(const glm::vec3& scale, const glm::quat& rotation,
                   const glm::vec3& translation);
glm::mat4 MakeMat4(float scale, const glm::quat& rotation,
                   const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::quat& rotation, const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::quat& rotation);
glm::mat4 MakeMat4(const glm::mat3& m3, const glm::vec3& translation);
glm::mat4 MakeMat4(const glm::mat3& m3);

void Decompose(const glm::mat4& matrix, glm::vec3* scale_out,
               glm::quat* rotation_out, glm::vec3* translation_out);
void Decompose(const glm::mat3& matrix, glm::vec3* scale_out,
               glm::quat* rotation_out);
void DecomposeSwingTwist(const glm::quat& rotation, const glm::vec3& twist_axis,
                         glm::quat* swing_out, glm::quat* twist_out);

glm::vec3 XformPoint(const glm::mat4& m, const glm::vec3& p);
glm::vec3 XformVec(const glm::mat4& m, const glm::vec3& v);

glm::vec3 RandomColor();

void PrintMat(const glm::mat4& m4, const std::string& name);
void PrintMat(const glm::mat3& m3, const std::string& name);
void PrintMat(const glm::mat2& m2, const std::string& name);
void PrintVec(const glm::vec4& v4, const std::string& name);
void PrintVec(const glm::vec3& v3, const std::string& name);
void PrintVec(const glm::vec2& v2, const std::string& name);
void PrintQuat(const glm::quat& q, const std::string& name);

bool FuzzyEquals(float lhs, float rhs, float epsilon = 0.001f);
bool FuzzyEquals(const glm::vec2& lhs, const glm::vec2& rhs,
                 float epsilon = 0.001f);
bool FuzzyEquals(const glm::vec3& lhs, const glm::vec3& rhs,
                 float epsilon = 0.001f);
bool FuzzyEquals(const glm::vec4& lhs, const glm::vec4& rhs,
                 float epsilon = 0.001f);
bool FuzzyEquals(const glm::quat& lhs, const glm::quat& rhs,
                 float epsilon = 0.001f);

void AppendSearchPath(const std::string& path);
std::string FindFile(const std::string& filename);

bool PointInsideAABB(const glm::vec3& point, const glm::vec3& aabb_min,
                     const glm::vec3& aabb_max);
float LinearToSRGB(float linear);
float SRGBToLinear(float srgb);
glm::vec4 LinearToSRGB(const glm::vec4& linear_color);
glm::vec4 SRGBToLinear(const glm::vec4& srgb_color);

glm::mat4 MakeRotateAboutPointMat(const glm::vec3& pos, const glm::quat& rot);

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

void StrCpy_s(char* dest, size_t destsz, const char* src);

glm::vec2 ToPlane(const glm::vec3& vec3);
glm::vec3 FromPlane(const glm::vec2& vec2);
glm::quat RotationBetweenVectors(glm::vec3 from, glm::vec3 to);

}  // namespace hyper
