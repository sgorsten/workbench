#include "pbr.h"
#include <string>

const std::string preamble = R"(#version 450
const float pi = 3.14159265359, tau = 6.28318530718;
float dotp(vec3 a, vec3 b) { return max(dot(a,b),0); }
float pow2(float x) { return x*x; }
float length2(vec3 v) { return dot(v,v); }

// Our physically based lighting equations use the following common terminology
// N - normal vector, unit vector perpendicular to the surface
// V - view vector, unit vector pointing from the surface towards the viewer
// L - light vector, unit vector pointing from the surface towards the light source
// H - half-angle vector, unit vector halfway between V and L
// R - reflection vector, V mirrored about N
// F0 - base reflectance of the surface
// alpha - common measure of surface roughness
float roughness_to_alpha(float roughness) { return roughness*roughness; }
float trowbridge_reitz_ggx(vec3 N, vec3 H, float alpha) { return alpha*alpha / (pi * pow2(dotp(N,H)*dotp(N,H)*(alpha*alpha-1) + 1)); }
float geometry_schlick_ggx(vec3 N, vec3 V, float k) { return dotp(N,V) / (dotp(N,V)*(1-k) + k); }
float geometry_smith(vec3 N, vec3 V, vec3 L, float k) { return geometry_schlick_ggx(N, L, k) * geometry_schlick_ggx(N, V, k); }
vec3 fresnel_schlick(vec3 V, vec3 H, vec3 F0) { return F0 + (1-F0) * pow(1-dotp(V,H), 5); }
vec3 cook_torrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 albedo, vec3 F0, float alpha, float metalness)
{
    const float D       = trowbridge_reitz_ggx(N, H, alpha);
    const float G       = geometry_smith(N, V, L, (alpha+1)*(alpha+1)/8);
    const vec3 F        = fresnel_schlick(V, H, F0);
    const vec3 diffuse  = (1-F) * (1-metalness) * albedo/pi;
    const vec3 specular = (D * G * F) / (4 * dotp(N,V) * dotp(N,L) + 0.001);  
    return (diffuse + specular) * dotp(N,L);
}

vec3 spherical(float phi, float cos_theta, float sin_theta) { return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta); }
vec3 spherical(float phi, float theta) { return spherical(phi, cos(theta), sin(theta)); }
)";

standard_shaders standard_shaders::compile(shader_compiler & compiler)
{
    const std::string importance_sample_ggx = R"(  
        vec3 importance_sample_ggx(float alpha, uint i, uint n)
        {
            // Phi is distributed uniformly over the integration range
            const float phi = i*tau/n;

            // Theta is importance-sampled using the Van Der Corpus sequence
            i = (i << 16u) | (i >> 16u);
            i = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
            i = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
            i = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
            i = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);
            float radical_inverse = i * 2.3283064365386963e-10; // Divide by 0x100000000
            float cos_theta = sqrt((1 - radical_inverse) / ((alpha*alpha-1)*radical_inverse + 1));
            return spherical(phi, cos_theta, sqrt(1 - cos_theta*cos_theta));
        }
    )";

    standard_shaders standard;
    standard.render_image_vertex_shader = compiler.compile(shader_stage::vertex, R"(#version 450
        layout(location=0) in vec2 v_position;
        layout(location=1) in vec2 v_texcoord;
        layout(location=0) out vec2 texcoord;
        void main()
        {
            texcoord = v_texcoord;
            gl_Position = vec4(v_position, 0, 1);
        }
    )");
    standard.compute_brdf_integral_image_fragment_shader = compiler.compile(shader_stage::fragment, preamble + importance_sample_ggx + R"(
        layout(location=0) in vec2 texcoords;
        layout(location=0) out vec4 f_color;

        const int sample_count = 1024;
        vec2 integrate_brdf(float n_dot_v, float alpha)
        {
            // Without loss of generality, evaluate the case where the normal is aligned with the z-axis and the viewing direction is in the xz-plane
            const vec3 N = vec3(0,0,1);
            const vec3 V = vec3(sqrt(1 - n_dot_v*n_dot_v), 0, n_dot_v);

            vec2 result = vec2(0,0);    
            for(int i=0; i<sample_count; ++i)
            {
                // For the desired roughness, sample possible half-angle vectors, and compute the lighting vector from them
                const vec3 H = importance_sample_ggx(alpha, i, sample_count);
                const vec3 L = normalize(2 * dot(V, H) * H - V);
                if(dot(N, L) <= 0) continue;

                // Integrate results
                const float Fc = pow(1 - dotp(V,H), 5);
                const float G = geometry_smith(N, V, L, alpha*alpha/2);
                const float G_Vis = (G * dotp(V,H)) / (dotp(N,H) * n_dot_v);
                result.x += (1 - Fc) * G_Vis;
                result.y += Fc * G_Vis;
            }
            return result/sample_count;
        }
        void main() 
        {
            f_color = vec4(integrate_brdf(texcoords.x, roughness_to_alpha(texcoords.y)), 0, 1);
        }
    )");

    standard.render_cubemap_vertex_shader = compiler.compile(shader_stage::vertex, R"(#version 450
        layout(location=0) in vec2 v_position;
        layout(location=1) in vec3 v_direction;
        layout(location=0) out vec3 direction;
        void main()
        {
            direction = v_direction;
            gl_Position = vec4(v_position, 0, 1);
        }
    )");
    standard.copy_cubemap_from_spheremap_fragment_shader = compiler.compile(shader_stage::fragment, R"(#version 450
        layout(set=0,binding=0) uniform PerOp { mat4 u_transform; } per_op;
        layout(set=0,binding=1) uniform sampler2D u_texture;            
        layout(location=0) in vec3 direction;
        layout(location=0) out vec4 f_color;
        vec2 compute_spherical_texcoords(vec3 direction) { return vec2(atan(direction.x, direction.z)*0.1591549, asin(direction.y)*0.3183099)+0.5; }
        void main() 
        { 
            vec3 dir = normalize((per_op.u_transform * vec4(direction,0)).xyz);
            f_color = texture(u_texture, compute_spherical_texcoords(dir));
        }
    )");
    standard.compute_irradiance_cubemap_fragment_shader = compiler.compile(shader_stage::fragment, preamble + R"(
        layout(set=0,binding=1) uniform samplerCube u_texture;    
        layout(location=0) in vec3 direction;
        layout(location=0) out vec4 f_color;
        void main()
        {
            const vec3 basis_z = normalize(direction);    
            const vec3 basis_x = normalize(cross(dFdx(basis_z), basis_z));
            const vec3 basis_y = normalize(cross(basis_z, basis_x));

            vec3 irradiance = vec3(0,0,0);
            float num_samples = 0; 
            for(float phi=0; phi<tau; phi+=0.01)
            {
                for(float theta=0; theta<tau/4; theta+=0.01)
                {
                    // Sample irradiance from the source texture, and weight by the sampling area
                    vec3 samp = spherical(phi, theta);
                    vec3 L = samp.x*basis_x + samp.y*basis_y + samp.z*basis_z; // basis * samp
                    irradiance += texture(u_texture, L).rgb * cos(theta) * sin(theta);
                    ++num_samples;
                }
            }
            f_color = vec4(irradiance*(pi/num_samples), 1);
        }
    )");
    standard.compute_reflectance_cubemap_fragment_shader = compiler.compile(shader_stage::fragment, preamble + importance_sample_ggx + R"(
        layout(set=0,binding=0) uniform PerOp { float u_roughness; } per_op;
        layout(set=0,binding=1) uniform samplerCube u_texture;
        layout(location=0) in vec3 direction;
        layout(location=0) out vec4 f_color;

        const int sample_count = 1;
        void main()
        {
            // As we are evaluating base reflectance, both the normal and view vectors are equal to our sampling direction
            const vec3 N = normalize(direction), V = N;
            const vec3 basis_z = normalize(direction);    
            const vec3 basis_x = normalize(cross(dFdx(basis_z), basis_z));
            const vec3 basis_y = normalize(cross(basis_z, basis_x));
            const float alpha = roughness_to_alpha(per_op.u_roughness);

            // Precompute the average solid angle of a cube map texel
            const int cube_width = textureSize(u_texture, 0).x;
            const float texel_solid_angle = pi*4 / (6*cube_width*cube_width);

            vec3 sum_color = vec3(0,0,0);
            float sum_weight = 0;     
            for(int i=0; i<sample_count; ++i)
            {
                // For the desired roughness, sample possible half-angle vectors, and compute the lighting vector from them
                const vec3 samp = importance_sample_ggx(alpha, i, sample_count);
                const vec3 H = samp.x*basis_x + samp.y*basis_y + samp.z*basis_z; // basis * samp
                const vec3 L = normalize(2*dot(V,H)*H - V);
                if(dot(N, L) <= 0) continue;

                // Compute the mip-level at which to sample
                const float D = trowbridge_reitz_ggx(N, H, alpha);
                const float pdf = D*dotp(N,H) / (4*dotp(V,H)) + 0.0001; 
                const float sample_solid_angle = 1 / (sample_count * pdf + 0.0001);
                const float mip_level = alpha > 0 ? log2(sample_solid_angle / texel_solid_angle)/2 : 0;

                // Sample the environment map according to the lighting direction, and weight the resulting contribution by N dot L
                sum_color += textureLod(u_texture, L, mip_level).rgb * dot(N, L);
                sum_weight += dot(N, L);
            }

            f_color = vec4(sum_color/sum_weight, 1);
        }
    )");
    return standard;
}

standard_device_objects::standard_device_objects(std::shared_ptr<rhi::device> dev, const standard_shaders & standard) : dev{dev}
{
    image_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge});
    spheremap_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::repeat, rhi::address_mode::clamp_to_edge});
    cubemap_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, rhi::filter::linear, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge});

    const float y = dev->get_info().inverted_framebuffers ? -1 : 1;
    const render_image_vertex image_vertices[] {{{-1,-y},{0,0}}, {{+1,-y},{1,0}}, {{+1,+y},{1,1}}, {{-1,-y},{0,0}}, {{+1,+y},{1,1}}, {{-1,+y},{0,1}}};
    const render_cubemap_vertex cubemap_vertices[]
    {
        {{-1,-y},{+1,+1,+1}}, {{+1,-y},{+1,+1,-1}}, {{+1,+y},{+1,-1,-1}}, {{-1,-y},{+1,+1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,+y},{+1,-1,+1}}, // standard positive x face
        {{-1,-y},{-1,+1,-1}}, {{+1,-y},{-1,+1,+1}}, {{+1,+y},{-1,-1,+1}}, {{-1,-y},{-1,+1,-1}}, {{+1,+y},{-1,-1,+1}}, {{-1,+y},{-1,-1,-1}}, // standard negative x face
        {{-1,-y},{-1,+1,-1}}, {{+1,-y},{+1,+1,-1}}, {{+1,+y},{+1,+1,+1}}, {{-1,-y},{-1,+1,-1}}, {{+1,+y},{+1,+1,+1}}, {{-1,+y},{-1,+1,+1}}, // standard positive y face
        {{-1,-y},{-1,-1,+1}}, {{+1,-y},{+1,-1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,-y},{-1,-1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,+y},{-1,-1,-1}}, // standard negative y face
        {{-1,-y},{-1,+1,+1}}, {{+1,-y},{+1,+1,+1}}, {{+1,+y},{+1,-1,+1}}, {{-1,-y},{-1,+1,+1}}, {{+1,+y},{+1,-1,+1}}, {{-1,+y},{-1,-1,+1}}, // standard positive z face
        {{-1,-y},{+1,+1,-1}}, {{+1,-y},{-1,+1,-1}}, {{+1,+y},{-1,-1,-1}}, {{-1,-y},{+1,+1,-1}}, {{+1,+y},{-1,-1,-1}}, {{-1,+y},{+1,-1,-1}}, // standard negative z face
    };
    render_image_vertex_buffer = dev->create_buffer({sizeof(image_vertices), rhi::buffer_usage::vertex, false}, image_vertices);
    render_cubemap_vertex_buffer = dev->create_buffer({sizeof(cubemap_vertices), rhi::buffer_usage::vertex, false}, cubemap_vertices);

    render_image_vertex_shader = dev->create_shader(standard.render_image_vertex_shader);
    compute_brdf_integral_image_fragment_shader = dev->create_shader(standard.compute_brdf_integral_image_fragment_shader);

    render_cubemap_vertex_shader = dev->create_shader(standard.render_cubemap_vertex_shader);
    copy_cubemap_from_spheremap_fragment_shader = dev->create_shader(standard.copy_cubemap_from_spheremap_fragment_shader);
    compute_irradiance_cubemap_fragment_shader = dev->create_shader(standard.compute_irradiance_cubemap_fragment_shader);
    compute_reflectance_cubemap_fragment_shader = dev->create_shader(standard.compute_reflectance_cubemap_fragment_shader);

    op_set_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    op_pipeline_layout = dev->create_pipeline_layout({op_set_layout});
    empty_pipeline_layout = dev->create_pipeline_layout({});

    compute_brdf_integral_image_pipeline = create_image_pipeline(empty_pipeline_layout, compute_brdf_integral_image_fragment_shader);
    copy_cubemap_from_spheremap_pipeline = create_cubemap_pipeline(op_pipeline_layout, copy_cubemap_from_spheremap_fragment_shader);
    compute_irradiance_cubemap_pipeline = create_cubemap_pipeline(op_pipeline_layout, compute_irradiance_cubemap_fragment_shader);
    compute_reflectance_cubemap_pipeline = create_cubemap_pipeline(op_pipeline_layout, compute_reflectance_cubemap_fragment_shader);    
}

standard_device_objects::~standard_device_objects()
{
    dev->destroy_pipeline(compute_reflectance_cubemap_pipeline);
    dev->destroy_pipeline(compute_irradiance_cubemap_pipeline);
    dev->destroy_pipeline(copy_cubemap_from_spheremap_pipeline);
    dev->destroy_pipeline(compute_brdf_integral_image_pipeline);
    dev->destroy_pipeline_layout(empty_pipeline_layout);
    dev->destroy_pipeline_layout(op_pipeline_layout);
    dev->destroy_descriptor_set_layout(op_set_layout);
    dev->destroy_shader(compute_reflectance_cubemap_fragment_shader);
    dev->destroy_shader(compute_irradiance_cubemap_fragment_shader);
    dev->destroy_shader(copy_cubemap_from_spheremap_fragment_shader);
    dev->destroy_shader(render_cubemap_vertex_shader);
    dev->destroy_shader(compute_brdf_integral_image_fragment_shader);
    dev->destroy_shader(render_image_vertex_shader);
    dev->destroy_sampler(spheremap_sampler);
    dev->destroy_sampler(cubemap_sampler);
    dev->destroy_sampler(image_sampler);
    dev->destroy_buffer(render_cubemap_vertex_buffer);
    dev->destroy_buffer(render_image_vertex_buffer);
}

const std::string pbr_lighting = R"(
// This function computes the full lighting to apply to a single fragment
layout(set=0,binding=0) uniform sampler2D u_brdf_integration_map;
layout(set=0,binding=1) uniform samplerCube u_irradiance_map;
layout(set=0,binding=2) uniform samplerCube u_reflectance_map;
layout(set=0,binding=3) uniform PerView
{
    mat4 u_view_proj_matrix;
    mat4 u_skybox_view_proj_matrix;
    vec3 u_eye_position;
};
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
    const vec3 light_positions[4] = {vec3(-3, -3, 8), vec3(3, -3, 8), vec3(3, 3, 8), vec3(-3, 3, 8)};
    const vec3 light_colors[4] = {vec3(23.47, 21.31, 20.79), vec3(23.47, 21.31, 20.79), vec3(23.47, 21.31, 20.79), vec3(23.47, 21.31, 20.79)};
    for(int i=0; i<4; ++i)
    {
        // Evaluate light vector, half-angle vector, and radiance of light source at the current distance
        const vec3 L = normalize(light_positions[i] - position);
        const vec3 H = normalize(V + L);
        const vec3 radiance = light_colors[i] / length2(light_positions[i] - position);
        light += radiance * cook_torrance(N, V, L, H, albedo, F0, alpha, metalness);
    }
    return light;
}
)";
