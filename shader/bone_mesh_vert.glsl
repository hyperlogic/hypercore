/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

//
// fullbright textured mesh
//

/*%%HEADER%%*/

#define MAX_BONES 200
#define MAX_BONE_INFLUENCES 4  // Typically 4 bones per vertex

uniform mat4 modelViewProjMat;
uniform mat3 normalModelMat;
uniform mat4 boneMats[MAX_BONES];

in vec3 position;
in vec2 uv;
in vec3 normal;
in vec4 boneWeights;
in vec4 boneIndices;

out vec2 frag_uv;
out vec3 frag_normal;

void main(void)
{
	vec4 skinnedPosition = vec4(0.0);
	vec3 skinnedNormal = vec3(0.0);
	for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
	{
		int boneIndex = int(boneIndices[i]);
		mat4 boneMat = boneMats[boneIndex];
		skinnedPosition += boneWeights[i] * (boneMat * vec4(position, 1.0));
		skinnedNormal += boneWeights[i] * (mat3(boneMat) * normal);
	}

	gl_Position = modelViewProjMat * skinnedPosition;
    frag_uv = uv;
    frag_normal = normalize(normalModelMat * skinnedNormal);
}
