/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// uber shader
//

/*%%HEADER%%*/
/*%%MATERIALINFO%%*/

const float M_PI = 3.141592653589793;

#ifdef HAS_BASE_TEXTURE
uniform sampler2D base_color_tex;
uniform int base_color_uv_index;
#ifdef HAS_BASE_TEXTURE_UV_TRANSFORM
uniform vec2 base_color_uv_offset;
uniform vec2 base_color_uv_scale;
uniform float base_color_uv_rotation;
#endif
#endif

#ifdef HAS_EMISSIVE_TEXTURE
uniform sampler2D emissive_color_tex;
uniform int emissive_color_uv_index;
#ifdef HAS_EMISSIVE_TEXTURE_UV_TRANSFORM
uniform vec2 emissive_color_uv_offset;
uniform vec2 emissive_color_uv_scale;
uniform float emissive_color_uv_rotation;
#endif
#endif

uniform vec3 camera_pos;

uniform vec3 light_direct_dir;
uniform vec3 light_direct_color;
uniform vec3 light_ambient_color;

uniform vec4 base_color_factor;
uniform float metallic_factor;
uniform float roughness_factor;
uniform vec3 emissive_color_factor;

#ifdef HAS_UV0
in vec2 frag_uv0;
#endif

#ifdef HAS_UV1
in vec2 frag_uv1;
#endif

in vec3 frag_position;
in vec3 frag_normal;

out vec4 out_color;

#if defined(HAS_UV0) && defined(HAS_UV1)
vec2 get_uv(int idx) {
  if (idx == 0) {
    return frag_uv0;
  } else {
    return frag_uv1;
  }
}
#elif defined(HAS_UV0)
vec2 get_uv(int idx) {
  if (idx == 0) {
    return frag_uv0;
  } else {
    return vec2(0.0, 0.0);
  }
}
#elif defined(HAS_UV1)
vec2 get_uv(int idx) {
  if (idx == 1) {
    return frag_uv1;
  } else {
    return vec2(0.0, 0.0);
  }
}
#endif

vec2 uv_transform(vec2 uv, vec2 offset, vec2 scale, float rotation) {
    float c = cos(rotation);
    float s = sin(rotation);
    return mat2(c, s, -s, c) * (uv * scale) + offset;
}

float ClampedDot(vec3 x, vec3 y) {
  return clamp(dot(x, y), 0.0, 1.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;
  float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
  float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
  float GGX = GGXV + GGXL;
  if (GGX > 0.0) {
    return 0.5 / GGX;
  }
  return 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness) {
  float alphaRoughnessSq = alphaRoughness * alphaRoughness;
  float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
  return alphaRoughnessSq / (M_PI * f * f);
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH) {
  return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_specularGGX(float alphaRoughness, float NdotL, float NdotV, float NdotH) {
  float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
  float D = D_GGX(NdotH, alphaRoughness);
  return vec3(Vis * D);
}

//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 diffuseColor) {
  // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
  return (diffuseColor / M_PI);
}

void main() {
  vec3 v = normalize(camera_pos - frag_position);
  vec3 n = normalize(frag_normal);
  vec3 l = light_direct_dir;
  vec3 h = normalize(l + v);

  float ndotv = ClampedDot(n, v);
  float ndotl = ClampedDot(n, l);
  float ndoth = ClampedDot(n, h);
  float vdoth = ClampedDot(v, h);

  float lightIntensity = 4.0;

#ifdef HAS_BASE_TEXTURE
#ifdef HAS_BASE_TEXTURE_UV_TRANSFORM
  vec2 base_color_uv = uv_transform(get_uv(base_color_uv_index),
                                    base_color_uv_offset,
                                    base_color_uv_scale,
                                    base_color_uv_rotation);
  vec4 base_color = texture(base_color_tex, base_color_uv) * base_color_factor;
#else
  vec4 base_color = texture(base_color_tex, get_uv(base_color_uv_index)) * base_color_factor;
#endif
#else
  vec4 base_color = base_color_factor;
#endif

  // PBR metallic-roughness (glTF 2.0 spec)
  vec3 c_diff = base_color.rgb * (1.0 - 0.04) * (1.0 - metallic_factor);
  vec3 f0 = mix(vec3(0.04), base_color.rgb, metallic_factor);
  vec3 f90 = vec3(1.0);
  float alpha_roughness = roughness_factor * roughness_factor;

  vec3 F_specular = F_Schlick(f0, f90, abs(vdoth));
  vec3 F_diffuse = F_Schlick(f0, f90, ndotv);
  vec3 f_diffuse = (1.0 - F_diffuse) * BRDF_lambertian(c_diff);
  vec3 f_specular = F_specular * BRDF_specularGGX(alpha_roughness, ndotl, ndotv, ndoth);

  vec3 l_color = (f_diffuse + f_specular) * lightIntensity * light_direct_color * ndotl;

#ifdef HAS_EMISSIVE_TEXTURE
#ifdef HAS_EMISSIVE_TEXTURE_UV_TRANSFORM
  vec2 emissive_color_uv = uv_transform(get_uv(emissive_color_uv_index),
                                        emissive_color_uv_offset,
                                        emissive_color_uv_scale,
                                        emissive_color_uv_rotation);
  vec3 emissive_color = texture(emissive_color_tex, emissive_color_uv).rgb * emissive_color_factor;
#else
  vec3 emissive_color = texture(emissive_color_tex, get_uv(emissive_color_uv_index)).rgb * emissive_color_factor;
#endif
#else
  vec3 emissive_color = emissive_color_factor;
#endif

  //vec3 ambient = light_ambient_color * (c_diff + f0);
  vec3 ambient = light_ambient_color * ((1.0 - F_diffuse) * c_diff + F_diffuse);
  vec3 final_color = ambient + l_color + emissive_color;

  // premultiplied alpha blending
  out_color.rgb = base_color.a * final_color;
  out_color.a = base_color.a;
}
