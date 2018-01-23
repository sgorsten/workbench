#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "pbr.glsl"
layout(set=PER_OBJECT,binding=0) uniform PerObject 
{ 
	mat4 u_model_matrix;
	vec2 u_material; // roughness, metalness
};
layout(set=PER_OBJECT,binding=1) uniform sampler2D u_albedo_tex;
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 texcoord;
layout(location=0) out vec4 f_color;
void main() 
{ 
	f_color = vec4(compute_lighting(position, normalize(normal), color*texture(u_albedo_tex, texcoord).rgb, u_material.x, u_material.y, 1.0), 1);
}