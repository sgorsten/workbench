#version 450
layout(set=1, binding=0) uniform sampler2D u_texture;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 color;
layout(location=0) out vec4 f_color;
void main() 
{
    f_color = color * texture(u_texture, texcoord);
}
