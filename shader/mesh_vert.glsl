/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured mesh
//

/*%%HEADER%%*/
/*%%TEXTUREINFO%%*/

uniform mat4 modelViewProjMat;
uniform mat3 normalModelMat;

in vec3 position;
#ifdef HAS_TEXTURE
in vec2 uv;
#endif
in vec3 normal;

#ifdef HAS_TEXTURE
out vec2 frag_uv;
#endif

out vec3 frag_normal;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
#ifdef HAS_TEXTURE
    frag_uv = uv;
#endif
    frag_normal = normalize(normalModelMat * normal);
}
