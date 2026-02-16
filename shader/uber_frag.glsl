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

uniform vec3 lightDir;
uniform vec3 lightColor;

uniform vec3 color_ambient;
uniform vec3 color_diffuse;
uniform float opacity;

#ifdef HAS_TEXTURE
in vec2 frag_uv;
#endif

in vec3 frag_normal;

out vec4 out_color;

void main() {
  vec3 n = normalize(frag_normal);
  float ndotl = max(dot(n, lightDir), 0.0);
  vec3 diffuse = ndotl * lightColor * color_diffuse;

#ifdef HAS_TEXTURE
  vec4 diffuse_tex_color = texture(colorTex, frag_uv);
  vec3 final_color = (color_ambient + diffuse) * diffuse_tex_color.rgb;
  // premultiplied alpha blending
  float alpha = diffuse_tex_color.a * opacity;
  out_color.rgb = alpha * final_color;
  out_color.a = alpha;
#else
  vec3 final_color = color_ambient + diffuse;
  // premultiplied alpha blending
  float alpha = opacity;
  out_color.rgb = alpha * final_color;
  out_color.a = alpha;
#endif
}
