#version 450
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 color;
layout(push_constant) uniform Transform { mat4 mvp; } transform;

void main() {
    gl_Position = transform.mvp * vec4(in_position, 1.0);
    color = in_color;
}
