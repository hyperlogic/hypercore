/*
    Copyright (c) 2026 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// uber shader
//

/*%%HEADER%%*/
/*%%MATERIALINFO%%*/

uniform mat4 model_mat;
uniform mat4 view_mat;
uniform mat4 proj_mat;
uniform mat3 normal_model_mat;

#ifdef HAS_BONES
#define MAX_BONES 200
#define MAX_BONE_INFLUENCES 4  // Typically 4 bones per vertex
uniform mat4 boneMats[MAX_BONES];
in vec4 boneWeights;
in vec4 boneIndices;
#endif

in vec4 position;

#ifdef HAS_TEXTURE
in vec2 uv;
out vec2 frag_uv;
#endif

in vec3 normal;

out vec3 frag_position;  // world space
out vec3 frag_normal;  // world space

void main(void) {
#ifdef HAS_BONES
  vec4 skinnedPosition = vec4(0.0);
	vec3 skinnedNormal = vec3(0.0);
	for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
	{
		int boneIndex = int(boneIndices[i]);
		mat4 boneMat = boneMats[boneIndex];
		skinnedPosition += boneWeights[i] * (boneMat * position);
		skinnedNormal += boneWeights[i] * (mat3(boneMat) * normal);
	}
  gl_Position = proj_mat * view_mat * model_mat * skinnedPosition;
	frag_position = (model_mat * skinnedPosition).xyz;
  frag_normal = normalize(normal_model_mat * skinnedNormal);
#else
  gl_Position = proj_mat * view_mat * model_mat * position;
	frag_position = (model_mat * position).xyz;
  frag_normal = normalize(normal_model_mat * normal);
#endif

#ifdef HAS_TEXTURE
  frag_uv = uv;
#endif
}
