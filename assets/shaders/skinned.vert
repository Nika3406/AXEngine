#version 330 core
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3  aPosition;
layout(location = 1) in vec3  aNormal;
layout(location = 2) in vec2  aUV;
layout(location = 3) in vec4  aTangent;
layout(location = 4) in uvec4 aJoints;
layout(location = 5) in vec4  aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

uniform int uSkinned;

layout(std140, binding = 0) uniform JointBlock {
    mat4 uJointMatrices[256];
};

out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vUV;
out vec4 vLightSpacePos;

void main() {
    vec4 pos = vec4(aPosition, 1.0);
    vec4 normal = vec4(aNormal, 0.0);

    if (uSkinned != 0) {
        mat4 skin =
            aWeights.x * uJointMatrices[aJoints.x] +
            aWeights.y * uJointMatrices[aJoints.y] +
            aWeights.z * uJointMatrices[aJoints.z] +
            aWeights.w * uJointMatrices[aJoints.w];

        pos = skin * pos;
        normal = skin * normal;
    }

    vec4 worldPos = uModel * pos;

    vWorldPos = worldPos.xyz;
    vWorldNormal = normalize(mat3(transpose(inverse(uModel))) * normal.xyz);
    vUV = aUV;

    // Used by skinned.frag to compare this fragment against the sun's depth map.
    vLightSpacePos = uLightSpaceMatrix * worldPos;

    gl_Position = uProjection * uView * worldPos;
}