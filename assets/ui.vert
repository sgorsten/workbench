#version 450
layout(set=1, binding=0) uniform PerOp { mat4 u_transform; };
layout(location=0) in vec2 v_position;
layout(location=1) in vec2 v_texcoord;
layout(location=0) out vec2 texcoord;
void main()
{
    texcoord = v_texcoord;
    gl_Position = u_transform * vec4(v_position, 0, 1);
}
