#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "standard/pbr.glsl"
layout(set=PER_MATERIAL,binding=0,std140) uniform PerMaterial 
{
	vec3 u_albedo_tint;
	float u_roughness;
	float u_metalness;
};
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 f_color;
void main() 
{ 
	f_color = vec4(compute_lighting(position, normalize(normal), u_albedo_tint, u_roughness, u_metalness, 1.0), 1);
}