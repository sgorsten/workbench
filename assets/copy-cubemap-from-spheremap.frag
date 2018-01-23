#version 450
layout(set=0,binding=0) uniform sampler2D u_texture;            
layout(set=0,binding=1) uniform PerOp { mat4 u_transform; };
layout(location=0) in vec3 direction;
layout(location=0) out vec4 f_color;
vec2 compute_spherical_texcoords(vec3 direction) { return vec2(atan(direction.x, direction.z)*0.1591549, asin(direction.y)*0.3183099)+0.5; }
void main() 
{ 
	vec3 dir = normalize((u_transform * vec4(direction,0)).xyz);
	f_color = texture(u_texture, compute_spherical_texcoords(dir));
}