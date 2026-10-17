#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform texture2D hdr_image;
layout(set = 0, binding = 1) uniform sampler image_sampler;
layout(set = 0, binding = 2) uniform texture2D depth_image;
layout(set = 0, binding = 3) uniform sampler depth_sampler;
layout(set = 0, binding = 4) uniform texture2D ssao_image;
layout(set = 0, binding = 5) uniform sampler ssao_sampler;
layout(push_constant) uniform Settings {
    float exposure;
    int ssao_quality;
    float ssao_radius;
    float ssao_strength;
    mat4 inverse_view_projection;
    vec4 camera_focal;
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
    float center_depth = depth_at(coordinate);
    if (center_depth >= 0.99999) return 1.0;
    vec3 center = world_at(coordinate, center_depth);
    vec2 pixel = 1.0 / vec2(textureSize(sampler2D(depth_image, depth_sampler), 0));
    float sum = 0.0;
    float weight_sum = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            vec2 sample_uv = coordinate + vec2(x, y) * pixel;
            float sample_depth = depth_at(sample_uv);
            if (sample_depth >= 0.99999) continue;
            float separation = length(world_at(sample_uv, sample_depth) - center);
            float weight = exp(-0.5 * float(x * x + y * y)) *
                           exp(-separation * 16.0 / settings.ssao_radius);
            sum += texture(sampler2D(ssao_image, ssao_sampler), sample_uv).r * weight;
            weight_sum += weight;
        }
    }
    return weight_sum > 0.0 ? sum / weight_sum : 1.0;
}

void main() {
    vec3 linear_color = max(texture(sampler2D(hdr_image, image_sampler), uv).rgb, vec3(0.0)) *
                        ambient_occlusion(uv);
    vec3 x = linear_color * settings.exposure;
    vec3 mapped = clamp((x * (2.51 * x + 0.03)) /
                        (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
    vec3 srgb = mix(mapped * 12.92,
                    1.055 * pow(mapped, vec3(1.0 / 2.4)) - 0.055,
                    greaterThan(mapped, vec3(0.0031308)));
    out_color = vec4(srgb, 1.0);
}
