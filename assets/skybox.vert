#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "standard/pbr.glsl"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_color;
layout(location=2) in vec3 v_normal;
layout(location=3) in vec2 v_texcoord;
layout(location=0) out vec3 direction;
void main()
{
	direction = v_position;
	gl_Position = u_skybox_view_proj_matrix * vec4(v_position,1);
}
