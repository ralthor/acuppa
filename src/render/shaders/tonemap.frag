#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform texture2D hdr_image;
layout(set = 0, binding = 1) uniform sampler image_sampler;
layout(push_constant) uniform Settings { float exposure; } settings;

void main() {
    vec3 linear_color = max(texture(sampler2D(hdr_image, image_sampler), uv).rgb, vec3(0.0));
    vec3 x = linear_color * settings.exposure;
    vec3 mapped = clamp((x * (2.51 * x + 0.03)) /
                        (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
    vec3 srgb = mix(mapped * 12.92,
                    1.055 * pow(mapped, vec3(1.0 / 2.4)) - 0.055,
                    greaterThan(mapped, vec3(0.0031308)));
    out_color = vec4(srgb, 1.0);
}
