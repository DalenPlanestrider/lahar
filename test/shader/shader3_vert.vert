// shader.vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

// Shared with fragment
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

// Vertex only
layout(set = 0, binding = 1) uniform ModelUBO {
    mat4 model;
} model;

void main() {
    gl_Position = camera.proj * camera.view * model.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}