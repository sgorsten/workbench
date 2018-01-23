#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec4 f_color;
void main() 
{
	f_color = vec4(color,1);
}
