/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured mesh
//

/*%%HEADER%%*/
/*%%TEXTUREINFO%%*/

uniform sampler2D colorTex;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

#ifdef HAS_TEXTURE
in vec2 frag_uv;
#endif
in vec3 frag_normal;

out vec4 out_color;

void main()
{
    vec3 n = normalize(frag_normal);
    float ndotl = max(dot(n, lightDir), 0.0);
    vec3 diffuse = ndotl * lightColor;

#ifdef HAS_TEXTURE
    vec4 texColor = texture(colorTex, frag_uv);
    vec3 finalColor = (ambientColor + diffuse) * texColor.rgb;

    // premultiplied alpha blending
    out_color.rgb = texColor.a * finalColor;
    out_color.a = texColor.a;
#else
    vec3 finalColor = ambientColor + diffuse;
    out_color.rgb = finalColor;
    out_color.a = 1.0;
#endif
}
