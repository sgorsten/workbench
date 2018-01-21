#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "pbr.glsl"
layout(set=1,binding=0) uniform PerObject 
{ 
	mat4 u_model_matrix;
	vec2 u_material; // roughness, metalness
};
layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_color;
layout(location=2) in vec3 v_normal;
layout(location=3) in vec2 v_texcoord;
layout(location=0) out vec3 position;
layout(location=1) out vec3 color;
layout(location=2) out vec3 normal;
layout(location=3) out vec2 texcoord;
void main()
{
	position = (u_model_matrix * vec4(v_position,1)).xyz;
	color = v_color;
	normal = (u_model_matrix * vec4(v_normal,0)).xyz;
	texcoord = v_texcoord;
	gl_Position = u_view_proj_matrix * vec4(position,1);
}