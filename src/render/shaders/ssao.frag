#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out float out_occlusion;

layout(set = 0, binding = 0) uniform texture2D depth_image;
layout(set = 0, binding = 1) uniform sampler depth_sampler;
layout(push_constant) uniform Settings {
    float exposure;
    int ssao_quality;
    float ssao_radius;
    float ssao_strength;
    mat4 inverse_view_projection;
    vec4 camera_focal; // xyz = eye, w = focal length in pixels
} settings;

float depth_at(vec2 coordinate) {
    return texture(sampler2D(depth_image, depth_sampler), coordinate).r;
}

vec3 world_at(vec2 coordinate, float depth) {
    vec4 clip = vec4(coordinate.x * 2.0 - 1.0, 1.0 - coordinate.y * 2.0, depth, 1.0);
    vec4 world = settings.inverse_view_projection * clip;
    return world.xyz / world.w;
}

float ambient_occlusion(vec2 coordinate) {
    if (settings.ssao_quality == 0 || settings.ssao_strength == 0.0) return 1.0;
    float depth = depth_at(coordinate);
    if (depth >= 0.99999) return 1.0;
    vec2 pixel = 1.0 / vec2(textureSize(sampler2D(depth_image, depth_sampler), 0));
    vec3 position = world_at(coordinate, depth);
    // One-pixel depth gradients are noisy on receding flat faces at half resolution.
    // Start with an edge-aware four-pixel normal, then use a wider span only
    // when those farther samples still lie on the same plane.
    vec2 step_uv = 4.0 * pixel;
    vec3 left = world_at(coordinate - vec2(step_uv.x, 0.0),
                         depth_at(coordinate - vec2(step_uv.x, 0.0)));
    vec3 right = world_at(coordinate + vec2(step_uv.x, 0.0),
                          depth_at(coordinate + vec2(step_uv.x, 0.0)));
    vec3 up = world_at(coordinate - vec2(0.0, step_uv.y),
                       depth_at(coordinate - vec2(0.0, step_uv.y)));
    vec3 down = world_at(coordinate + vec2(0.0, step_uv.y),
                         depth_at(coordinate + vec2(0.0, step_uv.y)));
    vec3 dx = length(right - position) < length(left - position)
        ? right - position : position - left;
    vec3 dy = length(down - position) < length(up - position)
        ? down - position : position - up;
    vec3 cross_product = cross(dx, dy);
    if (dot(cross_product, cross_product) < 1e-10) return 1.0;
    vec3 normal = normalize(cross_product);
    vec2 wide_step = 8.0 * pixel;
    vec3 wide_left = world_at(coordinate - vec2(wide_step.x, 0.0),
                              depth_at(coordinate - vec2(wide_step.x, 0.0)));
    vec3 wide_right = world_at(coordinate + vec2(wide_step.x, 0.0),
                               depth_at(coordinate + vec2(wide_step.x, 0.0)));
    vec3 wide_up = world_at(coordinate - vec2(0.0, wide_step.y),
                            depth_at(coordinate - vec2(0.0, wide_step.y)));
    vec3 wide_down = world_at(coordinate + vec2(0.0, wide_step.y),
                              depth_at(coordinate + vec2(0.0, wide_step.y)));
    float plane_error = max(max(abs(dot(normal, wide_left - position)),
                                abs(dot(normal, wide_right - position))),
                            max(abs(dot(normal, wide_up - position)),
                                abs(dot(normal, wide_down - position))));
    if (plane_error < min(0.1, settings.ssao_radius * 0.25)) {
        vec3 wide_dx = length(wide_right - position) < length(wide_left - position)
            ? wide_right - position : position - wide_left;
        vec3 wide_dy = length(wide_down - position) < length(wide_up - position)
            ? wide_down - position : position - wide_up;
        vec3 wide_cross = cross(wide_dx, wide_dy);
        if (dot(wide_cross, wide_cross) > 1e-10) normal = normalize(wide_cross);
    }
    if (dot(normal, settings.camera_focal.xyz - position) < 0.0) normal = -normal;

    int count = settings.ssao_quality == 1 ? 12 :
                (settings.ssao_quality == 2 ? 24 : 48);
    float radius_pixels = clamp(settings.ssao_radius * settings.camera_focal.w /
                                max(length(settings.camera_focal.xyz - position), 0.1),
                                1.0, 80.0);
    float occlusion = 0.0;
    for (int i = 0; i < count; ++i) {
        float angle = 2.39996323 * float(i);
        float distance_pixels = sqrt((float(i) + 0.5) / float(count)) * radius_pixels;
        vec2 sample_uv = coordinate + vec2(cos(angle), sin(angle)) * distance_pixels * pixel;
        if (any(lessThan(sample_uv, vec2(0.0))) ||
            any(greaterThan(sample_uv, vec2(1.0)))) continue;
        float sample_depth = depth_at(sample_uv);
        if (sample_depth >= 0.99999) continue;
        vec3 delta = world_at(sample_uv, sample_depth) - position;
        float distance_world = length(delta);
        if (distance_world < 0.02 || distance_world >= settings.ssao_radius) continue;
        float facing = max(dot(normal, delta) / distance_world - 0.1, 0.0);
        float falloff = 1.0 - distance_world / settings.ssao_radius;
        occlusion += facing * falloff * falloff;
    }
    return clamp(1.0 - 45.0 * settings.ssao_strength * occlusion / float(count), 0.55, 1.0);
}

void main() {
    out_occlusion = ambient_occlusion(uv);
}
