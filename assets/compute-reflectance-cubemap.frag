#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "preamble.glsl"
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
