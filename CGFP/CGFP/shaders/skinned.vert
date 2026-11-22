#version 330 core
// 뼈대 애니메이션 캐릭터용 GPU 스키닝. 프래그먼트는 object_viewer.frag를 같이 쓴다.

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in uvec4 aJoints;
layout (location = 4) in vec4 aWeights;

const int MAX_BONES = 64; // SkinnedModel::kMaxBones와 같아야 한다
uniform mat4 uBones[MAX_BONES];
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out vec2 vUV;

void main()
{
    mat4 skin = aWeights.x * uBones[aJoints.x] + aWeights.y * uBones[aJoints.y] +
                aWeights.z * uBones[aJoints.z] + aWeights.w * uBones[aJoints.w];
    mat4 model = uModel * skin;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * model * vec4(aPosition, 1.0);
}
