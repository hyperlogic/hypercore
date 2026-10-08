/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// No lighting at all, solid color
//

/*%%HEADER%%*/
/*%%BONES%%*/

uniform mat4 modelViewProjMat;

#ifdef HAS_BONES
#define MAX_BONE_INFLUENCES 4
uniform samplerBuffer boneMats;
in vec4 boneWeights;
in vec4 boneIndices;

mat4 GetBoneMat(int index) {
  // Each mat4 is stored as 4 consecutive RGBA32F texels.
  int offset = index * 4;
  return mat4(texelFetch(boneMats, offset),
              texelFetch(boneMats, offset + 1),
              texelFetch(boneMats, offset + 2),
              texelFetch(boneMats, offset + 3));
}
#endif

in vec4 position;

void main(void)
{
#ifdef HAS_BONES
  vec4 skinnedPosition = vec4(0.0);
  for (int i = 0; i < MAX_BONE_INFLUENCES; i++) {
    mat4 boneMat = GetBoneMat(int(boneIndices[i]));
    skinnedPosition += boneWeights[i] * (boneMat * position);
  }
  gl_Position = modelViewProjMat * skinnedPosition;
#else
  gl_Position = modelViewProjMat * position;
#endif
}
