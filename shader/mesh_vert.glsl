/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured mesh
//

/*%%HEADER%%*/

uniform mat4 modelViewProjMat;
uniform mat3 normalModelMat;

in vec3 position;
in vec2 uv;
in vec3 normal;

out vec2 frag_uv;
out vec3 frag_normal;

void main(void)
{
    gl_Position = modelViewProjMat * vec4(position, 1);
    frag_uv = uv;
    frag_normal = normalize(normalModelMat * normal);
}
