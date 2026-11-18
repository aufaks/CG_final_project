#version 330 core

in vec3 vNormal;
in vec2 vUV;

uniform vec3 uColor;
uniform vec3 uLightDirection;
uniform bool uReferenceLighting;
uniform float uOpacity;
uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    if (uReferenceLighting && !gl_FrontFacing) normal = -normal;

    float diffuse = max(dot(normal, normalize(-uLightDirection)), 0.0);
    vec3 illumination = vec3(0.22 + diffuse * 0.78);
    if (uReferenceLighting) {
        // A soft blue sky fill and a warm key light approximate the Sketchfab interior preview.
        float skyAmount = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 ambient = mix(vec3(0.25, 0.21, 0.17), vec3(0.38, 0.54, 0.68), skyAmount);
        vec3 keyLight = vec3(0.58, 0.52, 0.42) * diffuse;
        illumination = min(ambient + keyLight, vec3(1.0));
    }

    vec4 albedo = uUseTexture ? texture(uTexture, vUV) : vec4(1.0);
    FragColor = vec4(uColor * albedo.rgb * illumination, albedo.a * uOpacity);
}
