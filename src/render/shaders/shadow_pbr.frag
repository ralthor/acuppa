#version 450
layout(location = 0) in vec2 uv;
layout(set = 0, binding = 0) uniform texture2D base_texture;
layout(set = 0, binding = 5) uniform sampler material_sampler;
layout(std140, set = 0, binding = 7) uniform Material {
    vec4 base_color;
    vec4 factors;
    vec4 emissive_alpha;
    ivec4 maps0;
    ivec4 maps1;
} material;
void main() {
    if (material.maps1.y != 0) {
        float alpha = material.base_color.a;
        if (material.maps0.x != 0)
            alpha *= texture(sampler2D(base_texture, material_sampler), uv).a;
        if (alpha < material.emissive_alpha.w) discard;
    }
}
