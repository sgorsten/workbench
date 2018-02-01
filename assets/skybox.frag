#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "standard/pbr.glsl"
layout(set=PER_MATERIAL,binding=0) uniform samplerCube u_texture;
layout(location=0) in vec3 direction;
layout(location=0) out vec4 f_color;
void main()
{
	f_color = texture(u_texture, normalize(direction));
}