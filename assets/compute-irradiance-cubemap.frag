#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "preamble.glsl"
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
