#version 330 core

in vec3 vNormal;
in vec2 vUV;

uniform vec3 uColor;
uniform vec3 uLightDirection;
uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(-uLightDirection)), 0.0);
    float lighting = 0.22 + diffuse * 0.78;
    vec4 albedo = uUseTexture ? texture(uTexture, vUV) : vec4(1.0);
    FragColor = vec4(uColor * albedo.rgb * lighting, albedo.a);
}
