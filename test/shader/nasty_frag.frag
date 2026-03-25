#version 450

layout(location = 0) in vec3  fragWorldPos;
layout(location = 1) in vec3  fragNormal;
layout(location = 2) in vec2  fragUV;
layout(location = 3) in flat uint fragMaterialID;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(fragNormal);
    float ndotl = max(dot(n, normalize(vec3(1.0, 1.0, 0.5))), 0.05);
    outColor = vec4(vec3(ndotl), 1.0);
}
