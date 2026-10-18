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
    vec4 environment_params; // x = intensity
} frame;
layout(std140, set = 0, binding = 7) uniform Material {
    vec4 base_color;
    vec4 factors; // metallic, roughness, normal scale, occlusion strength
    vec4 emissive_alpha;
    ivec4 maps0; // base color, metallic-roughness, normal, occlusion
    ivec4 maps1; // emissive, alpha mask
} material;
layout(set = 1, binding = 0) uniform texture2D environment_texture;
layout(set = 1, binding = 1) uniform sampler environment_sampler;
layout(set = 1, binding = 2) uniform texture2D environment_diffuse_texture;
layout(set = 2, binding = 0) uniform texture2D shadow_texture;
layout(set = 2, binding = 1) uniform sampler shadow_sampler;
layout(std140, set = 2, binding = 2) uniform Shadows {
    mat4 transforms[48];
    vec4 settings[8]; // x = enabled
} shadows;

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

vec2 environment_uv(vec3 direction) {
    return vec2(atan(direction.z, direction.x) / (2.0 * pi) + 0.5,
                acos(clamp(direction.y, -1.0, 1.0)) / pi);
}

vec3 image_based_light(vec3 normal, vec3 view, vec3 albedo,
                       float metallic, float roughness, vec3 f0) {
    float ndotv = max(dot(normal, view), 0.0);
    vec3 fresnel = fresnel_schlick(ndotv, f0);
    vec3 diffuse = texture(sampler2D(environment_diffuse_texture, environment_sampler),
                           environment_uv(normal)).rgb * albedo * (1.0 - metallic) *
                   (vec3(1.0) - fresnel);
    vec3 reflected = reflect(-view, normal);
    vec3 radiance = textureLod(sampler2D(environment_texture, environment_sampler),
                               environment_uv(reflected), roughness * 6.0).rgb;
    // Analytic split-sum BRDF fit for GGX, evaluated per pixel without a LUT.
    vec4 coefficients = roughness * vec4(-1.0, -0.0275, -0.572, 0.022) +
                        vec4(1.0, 0.0425, 1.04, -0.04);
    float factor = min(coefficients.x * coefficients.x, exp2(-9.28 * ndotv)) *
                   coefficients.x + coefficients.y;
    vec2 brdf = vec2(-1.04, 1.04) * factor + coefficients.zw;
    return diffuse + radiance * (f0 * brdf.x + brdf.y);
}

float shadow_visibility(int light_index, vec3 position, vec3 normal, vec3 light_direction) {
    if (shadows.settings[light_index].x < 0.5) return 1.0;
    int face = 0;
    if (frame.light_position_type[light_index].w > 0.5) {
        vec3 delta = position - frame.light_position_type[light_index].xyz;
        vec3 axis = abs(delta);
        if (axis.x >= axis.y && axis.x >= axis.z) face = delta.x > 0.0 ? 0 : 1;
        else if (axis.y >= axis.z) face = delta.y > 0.0 ? 2 : 3;
        else face = delta.z > 0.0 ? 4 : 5;
    }
    vec4 clip = shadows.transforms[light_index * 6 + face] * vec4(position, 1.0);
    vec3 projected = clip.xyz / clip.w;
    if (abs(projected.x) > 1.0 || abs(projected.y) > 1.0 ||
        projected.z < 0.0 || projected.z > 1.0) return 1.0;
    vec2 local = vec2(projected.x * 0.5 + 0.5, 0.5 - projected.y * 0.5);
    float bias = max(0.004, 0.006 * (1.0 - max(dot(normal, light_direction), 0.0)));
    float visible = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 tap = clamp(local + vec2(x, y) / 512.0,
                             vec2(0.5 / 512.0), vec2(1.0 - 0.5 / 512.0));
            vec2 atlas_uv = (vec2(float(face), float(light_index)) + tap) / vec2(6.0, 8.0);
            float depth = texture(sampler2D(shadow_texture, shadow_sampler), atlas_uv).r;
            visible += projected.z - bias <= depth ? 1.0 : 0.0;
        }
    }
    return visible / 9.0;
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
        lighting += (diffuse + specular) * radiance * ndotl *
                    shadow_visibility(i, world_position, normal, light);
    }
    float occlusion = 1.0;
    if (material.maps0.w != 0)
        occlusion = mix(1.0, texture(sampler2D(occlusion_texture, material_sampler), uv).r,
                        material.factors.w);
    vec3 emissive = material.emissive_alpha.rgb;
    if (material.maps1.x != 0)
        emissive *= texture(sampler2D(emissive_texture, material_sampler), uv).rgb;
    vec3 ambient = frame.environment_params.x > 0.0
        ? image_based_light(normal, view, albedo, metallic, roughness, f0) *
          frame.environment_params.x * occlusion
        : 0.03 * albedo * (1.0 - metallic) * occlusion;
    out_color = vec4(lighting + ambient + emissive, 1.0);
}
