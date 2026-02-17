/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// uber shader
//

/*%%HEADER%%*/
/*%%MATERIALINFO%%*/

#ifdef HAS_TEXTURE
uniform sampler2D colorTex;
#endif

uniform vec3 light_direct_dir;
uniform vec3 light_direct_color;
uniform vec3 light_ambient_color;

uniform vec3 color_diffuse;
uniform float opacity;

#ifdef HAS_TEXTURE
in vec2 frag_uv;
#endif

in vec3 frag_normal;

out vec4 out_color;

void main() {
  vec3 n = normalize(frag_normal);
  float ndotl = max(dot(n, light_direct_dir), 0.0);
  vec3 diffuse = ndotl * light_direct_color * color_diffuse;

#ifdef HAS_TEXTURE
  vec4 diffuse_tex_color = texture(colorTex, frag_uv);
  vec3 final_color = (light_ambient_color + diffuse) * diffuse_tex_color.rgb;
  // premultiplied alpha blending
  float alpha = diffuse_tex_color.a * opacity;
  out_color.rgb = alpha * final_color;
  out_color.a = alpha;
#else
  vec3 final_color = light_ambient_color + diffuse;
  // premultiplied alpha blending
  float alpha = opacity;
  out_color.rgb = alpha * final_color;
  out_color.a = alpha;
#endif
}
