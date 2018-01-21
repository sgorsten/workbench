#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "preamble.glsl"
layout(location=0) in vec2 texcoords;
layout(location=0) out vec4 f_color;

const int sample_count = 1024;
vec2 integrate_brdf(float n_dot_v, float alpha)
{
    // Without loss of generality, evaluate the case where the normal is aligned with the z-axis and the viewing direction is in the xz-plane
    const vec3 N = vec3(0,0,1);
    const vec3 V = vec3(sqrt(1 - n_dot_v*n_dot_v), 0, n_dot_v);

    vec2 result = vec2(0,0);    
    for(int i=0; i<sample_count; ++i)
    {
        // For the desired roughness, sample possible half-angle vectors, and compute the lighting vector from them
        const vec3 H = importance_sample_ggx(alpha, i, sample_count);
        const vec3 L = normalize(2 * dot(V, H) * H - V);
        if(dot(N, L) <= 0) continue;

        // Integrate results
        const float Fc = pow(1 - dotp(V,H), 5);
        const float G = geometry_smith(N, V, L, alpha*alpha/2);
        const float G_Vis = (G * dotp(V,H)) / (dotp(N,H) * n_dot_v);
        result.x += (1 - Fc) * G_Vis;
        result.y += Fc * G_Vis;
    }
    return result/sample_count;
}
void main() 
{
    f_color = vec4(integrate_brdf(texcoords.x, roughness_to_alpha(texcoords.y)), 0, 1);
}
