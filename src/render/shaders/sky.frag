#version 450
// Gradient sky drawn into the HDR target before geometry. Matches
// acuppa::sky_radiance in src/core/sky.cpp.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform SkyFrame {
    mat4 inverse_view_projection;
    vec4 eye;
    vec4 zenith;
    vec4 horizon;
    vec4 ground;
    vec4 sun_direction;  // xyz unit direction, w = 1 when a disc is drawn
    vec4 sun_color;
    vec4 sun_shape;      // outer cos, inner cos, halo sharpness, halo strength
} sky;

void main() {
    vec4 clip = vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0);
    vec4 world = sky.inverse_view_projection * clip;
    vec3 direction = normalize(world.xyz / world.w - sky.eye.xyz);
    float height = direction.y;
    float up = pow(max(height, 0.0), 0.5);
    vec3 color = mix(sky.horizon.rgb, sky.zenith.rgb, smoothstep(0.0, 1.0, up));
    color = mix(color, sky.ground.rgb, smoothstep(0.0, 0.12, -height));
    if (sky.sun_direction.w > 0.0) {
        float facing = dot(direction, sky.sun_direction.xyz);
        float disc = smoothstep(sky.sun_shape.x, sky.sun_shape.y, facing);
        float halo = sky.sun_shape.w * exp((facing - 1.0) * sky.sun_shape.z);
        color += sky.sun_color.rgb * (disc + halo);
    }
    out_color = vec4(color, 1.0);
}
