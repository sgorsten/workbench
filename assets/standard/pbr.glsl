#include "pbr/preamble.glsl"

// Descriptor set indices
const int PER_SCENE = 0;	// Used for descriptors that are constant within the span of a single frame
const int PER_VIEW = 1;		// Used for descriptors that vary between viewpoints but are constant within a viewpoint
const int PER_MATERIAL = 2;	// Used for textures and material properties which may be shared by several objects
const int PER_OBJECT = 3;	// Used for static or skeletal mesh pose data specific to a single object (optional for skyboxes, particle systems, and instances)

struct point_light { vec3 position, light; };

// Per-scene descriptors
layout(set=PER_SCENE, binding=0, std140) uniform PerScene { point_light u_point_lights[4]; }; 
layout(set=PER_SCENE, binding=1) uniform sampler2D u_brdf_integration_map;
layout(set=PER_SCENE, binding=2) uniform samplerCube u_irradiance_map;
layout(set=PER_SCENE, binding=3) uniform samplerCube u_reflectance_map;

// Per-view descriptors
layout(set=PER_VIEW, binding=0, std140) uniform PerView
{
    mat4 u_view_proj_matrix;
    mat4 u_skybox_view_proj_matrix;
    vec3 u_eye_position;
	vec3 u_right_vector;
	vec3 u_down_vector;
};

// This function computes the full lighting to apply to a single fragment
const float MAX_REFLECTANCE_LOD = 4.0;
vec3 compute_lighting(vec3 position, vec3 normal, vec3 albedo, float roughness, float metalness, float ambient_occlusion)
{
    // Compute common terms of lighting equations
    const vec3 N = normalize(normal);
    const vec3 V = normalize(u_eye_position - position);
    const vec3 R = reflect(-V, N);
    const vec3 F0 = mix(vec3(0.04), albedo, metalness);
    const float alpha = roughness_to_alpha(roughness);

    // Initialize our light accumulator
    vec3 light = vec3(0,0,0);

    // Add contribution from indirect lights
    {
        vec2 brdf = texture(u_brdf_integration_map, vec2(dotp(N,V), roughness)).xy;
        vec3 F    = F0 + max(1-F0-roughness, 0) * pow(1-dotp(N,V), 5);
        vec3 spec = (F * brdf.x + brdf.y) * textureLod(u_reflectance_map, R, roughness * MAX_REFLECTANCE_LOD).rgb;
        vec3 diff = (1-F) * (1-metalness) * albedo * texture(u_irradiance_map, N).rgb;
        light     += (diff + spec) * ambient_occlusion; 
    }

    // Add contributions from direct lights
    for(int i=0; i<4; ++i)
    {
        // Evaluate light vector, half-angle vector, and radiance of light source at the current distance
        const vec3 L = normalize(u_point_lights[i].position - position);
        const vec3 H = normalize(V + L);
        const vec3 radiance = u_point_lights[i].light / length2(u_point_lights[i].position - position);
        light += radiance * cook_torrance(N, V, L, H, albedo, F0, alpha, metalness);
    }
    return light;
}
