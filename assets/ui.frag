#version 450
layout(set=1, binding=1) uniform sampler2D u_texture;
layout(location=0) in vec2 texcoords;
layout(location=0) out vec4 f_color;
void main() 
{
    f_color = texture(u_texture, texcoords);
}
