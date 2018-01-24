#version 450
layout(set=0, binding=0) uniform PerOp { mat4 u_transform; };
layout(location=0) in vec2 v_position;
layout(location=1) in vec2 v_texcoord;
layout(location=2) in vec4 v_color;
layout(location=0) out vec2 texcoord;
layout(location=1) out vec4 color;
void main()
{
	gl_Position = u_transform * vec4(v_position, 0, 1);
    texcoord = v_texcoord;
	color = v_color;    
}
