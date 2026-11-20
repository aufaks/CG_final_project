#version 330 core

in vec2 vUV;

uniform vec4 uColor;
uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 FragColor;

void main()
{
    vec4 tex = uUseTexture ? texture(uTexture, vUV) : vec4(1.0);
    FragColor = uColor * tex;
    if (FragColor.a < 0.01) discard;
}
