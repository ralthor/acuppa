#version 450
layout(location = 0) in vec3 world_position;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec3 vertex_color;
layout(location = 3) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform texture2D base_texture;
layout(set = 0, binding = 1) uniform texture2D metallic_roughness_texture;
layout(set = 0, binding = 2) uniform texture2D normal_texture;
layout(set = 0, binding = 3) uniform texture2D occlusion_texture;
layout(set = 0, binding = 4) uniform texture2D emissive_texture;
layout(set = 0, binding = 5) uniform sampler material_sampler;
layout(std140, set = 0, binding = 6) uniform Frame {
    vec4 camera;
    vec4 light_position_type[8];
    vec4 light_color_intensity[8];
    ivec4 light_count;
} frame;
layout(std140, set = 0, binding = 7) uniform Material {
    vec4 base_color;
    vec4 factors; // metallic, roughness, normal scale, occlusion strength
    vec4 emissive_alpha;
    ivec4 maps0; // base color, metallic-roughness, normal, occlusion
    ivec4 maps1; // emissive, alpha mask
} material;

const float pi = 3.14159265359;

float distribution_ggx(vec3 normal, vec3 half_vector, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, half_vector), 0.0);
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(pi * denom * denom, 0.0001);
}

float geometry_schlick(float ndotv, float roughness) {
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.0001);
}

vec3 fresnel_schlick(float cosine, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

void main() {
    vec4 base = material.base_color;
    if (material.maps0.x != 0)
        base *= texture(sampler2D(base_texture, material_sampler), uv);
    base.rgb *= vertex_color;
    if (material.maps1.y != 0 && base.a < material.emissive_alpha.w) discard;
    vec3 albedo = base.rgb;
    float metallic = material.factors.x;
    float roughness = material.factors.y;
    if (material.maps0.y != 0) {
        vec4 mr = texture(sampler2D(metallic_roughness_texture, material_sampler), uv);
        metallic *= mr.b;
        roughness *= mr.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    vec3 normal = normalize(world_normal);
    if (material.maps0.z != 0) {
        vec3 sampled = texture(sampler2D(normal_texture, material_sampler), uv).xyz * 2.0 - 1.0;
        sampled.xy *= material.factors.z;
        vec3 dp1 = dFdx(world_position), dp2 = dFdy(world_position);
        vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
        vec3 tangent = normalize(dp1 * duv2.y - dp2 * duv1.y);
        vec3 bitangent = normalize(-dp1 * duv2.x + dp2 * duv1.x);
        normal = normalize(mat3(tangent, bitangent, normal) * sampled);
    }
    vec3 view = normalize(frame.camera.xyz - world_position);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 lighting = vec3(0.0);
    for (int i = 0; i < frame.light_count.x; ++i) {
        bool directional = frame.light_position_type[i].w < 0.5;
        vec3 delta = directional ? -frame.light_position_type[i].xyz
                                 : frame.light_position_type[i].xyz - world_position;
        float distance2 = max(dot(delta, delta), 0.01);
        vec3 light = normalize(delta);
        vec3 halfway = normalize(view + light);
        float attenuation = directional ? 1.0 : 1.0 / distance2;
        vec3 radiance = frame.light_color_intensity[i].rgb *
                        frame.light_color_intensity[i].a * attenuation;
        float ndotv = max(dot(normal, view), 0.0);
        float ndotl = max(dot(normal, light), 0.0);
        if (ndotl <= 0.0 || ndotv <= 0.0) continue;
        float distribution = distribution_ggx(normal, halfway, roughness);
        float geometry = geometry_schlick(ndotv, roughness) *
                         geometry_schlick(ndotl, roughness);
        vec3 fresnel = fresnel_schlick(max(dot(halfway, view), 0.0), f0);
        vec3 specular = distribution * geometry * fresnel /
                        max(4.0 * ndotv * ndotl, 0.0001);
        vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / pi;
        lighting += (diffuse + specular) * radiance * ndotl;
    }
    float occlusion = 1.0;
    if (material.maps0.w != 0)
        occlusion = mix(1.0, texture(sampler2D(occlusion_texture, material_sampler), uv).r,
                        material.factors.w);
    vec3 emissive = material.emissive_alpha.rgb;
    if (material.maps1.x != 0)
        emissive *= texture(sampler2D(emissive_texture, material_sampler), uv).rgb;
    vec3 ambient = 0.03 * albedo * (1.0 - metallic) * occlusion;
    out_color = vec4(lighting + ambient + emissive, 1.0);
}
