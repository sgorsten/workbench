#version 450
layout(set=2, binding=1) uniform sampler2D u_texture;
layout(location=0) in vec2 texcoord;
layout(location=1) in vec4 color;
layout(location=0) out vec4 f_color;
void main() 
{
    f_color = vec4(color.rgb, color.a*texture(u_texture, texcoord).r);
}
