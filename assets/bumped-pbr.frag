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
layout(set=PER_MATERIAL,binding=1) uniform sampler2D u_albedo_tex;
layout(set=PER_MATERIAL,binding=2) uniform sampler2D u_normal_tex;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in vec3 tangent;
layout(location=4) in vec3 bitangent;
layout(location=0) out vec4 f_color;
void main() 
{ 
	vec3 ts_normal = texture(u_normal_tex, texcoord).rgb*2-1;
	vec3 ws_normal = normalize(tangent)*ts_normal.x
	               + normalize(bitangent)*ts_normal.y
				   + normalize(normal)*ts_normal.z;
	f_color = vec4(compute_lighting(position, normalize(ws_normal), u_albedo_tint*texture(u_albedo_tex, texcoord).rgb, u_roughness, u_metalness, 1.0), 1);
}