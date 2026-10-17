#version 450
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;
layout(push_constant) uniform Transform { mat4 mvp; mat4 model; } transform;
layout(location = 0) out vec2 uv;
void main() {
    uv = in_uv;
    gl_Position = transform.mvp * vec4(in_position, 1.0);
}
