#version 450
layout(location=0) in vec2 v_position;
layout(location=1) in vec3 v_direction;
layout(location=0) out vec3 direction;
void main()
{
	direction = v_direction;
	gl_Position = vec4(v_position, 0, 1);
}
