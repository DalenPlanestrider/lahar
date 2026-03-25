// shader.frag
#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Shared with vertex
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

// Fragment only
layout(set = 0, binding = 2) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}