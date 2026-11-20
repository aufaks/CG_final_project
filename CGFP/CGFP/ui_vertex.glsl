#version 330 core

layout (location = 0) in vec2 aPos;

uniform mat4 uProjection;
uniform vec4 uRect;
uniform vec4 uUVRect;

out vec2 vUV;

void main()
{
    vec2 pos = uRect.xy + aPos * uRect.zw;
    vUV = uUVRect.xy + vec2(aPos.x, 1.0 - aPos.y) * uUVRect.zw;
    gl_Position = uProjection * vec4(pos, 0.0, 1.0);
}
