const float pi = 3.14159265359, tau = 6.28318530718;
float dotp(vec3 a, vec3 b) { return max(dot(a,b),0); }
float pow2(float x) { return x*x; }
float length2(vec3 v) { return dot(v,v); }

// Our physically based lighting equations use the following common terminology
// N - normal vector, unit vector perpendicular to the surface
// V - view vector, unit vector pointing from the surface towards the viewer
// L - light vector, unit vector pointing from the surface towards the light source
// H - half-angle vector, unit vector halfway between V and L
// R - reflection vector, V mirrored about N
// F0 - base reflectance of the surface
// alpha - common measure of surface roughness
float roughness_to_alpha(float roughness) { return roughness*roughness; }
float trowbridge_reitz_ggx(vec3 N, vec3 H, float alpha) { return alpha*alpha / (pi * pow2(dotp(N,H)*dotp(N,H)*(alpha*alpha-1) + 1)); }
float geometry_schlick_ggx(vec3 N, vec3 V, float k) { return dotp(N,V) / (dotp(N,V)*(1-k) + k); }
float geometry_smith(vec3 N, vec3 V, vec3 L, float k) { return geometry_schlick_ggx(N, L, k) * geometry_schlick_ggx(N, V, k); }
vec3 fresnel_schlick(vec3 V, vec3 H, vec3 F0) { return F0 + (1-F0) * pow(1-dotp(V,H), 5); }
vec3 cook_torrance(vec3 N, vec3 V, vec3 L, vec3 H, vec3 albedo, vec3 F0, float alpha, float metalness)
{
    const float D       = trowbridge_reitz_ggx(N, H, alpha);
    const float G       = geometry_smith(N, V, L, (alpha+1)*(alpha+1)/8);
    const vec3 F        = fresnel_schlick(V, H, F0);
    const vec3 diffuse  = (1-F) * (1-metalness) * albedo/pi;
    const vec3 specular = (D * G * F) / (4 * dotp(N,V) * dotp(N,L) + 0.001);  
    return (diffuse + specular) * dotp(N,L);
}

vec3 spherical(float phi, float cos_theta, float sin_theta) { return vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta); }
vec3 spherical(float phi, float theta) { return spherical(phi, cos(theta), sin(theta)); }

vec3 importance_sample_ggx(float alpha, uint i, uint n)
{
    // Phi is distributed uniformly over the integration range
    const float phi = i*tau/n;

    // Theta is importance-sampled using the Van Der Corpus sequence
    i = (i << 16u) | (i >> 16u);
    i = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
    i = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
    i = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
    i = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);
    float radical_inverse = i * 2.3283064365386963e-10; // Divide by 0x100000000
    float cos_theta = sqrt((1 - radical_inverse) / ((alpha*alpha-1)*radical_inverse + 1));
    return spherical(phi, cos_theta, sqrt(1 - cos_theta*cos_theta));
}
