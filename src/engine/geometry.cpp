#include "geometry.h"

DOCTEST_TEST_CASE("compute_sphere_texcoords(...) semantics")
{
    DOCTEST_CHECK( compute_sphere_texcoords({-1,  0, 0}).x == doctest::Approx(0.25f) );
    DOCTEST_CHECK( compute_sphere_texcoords({ 0,  0, 1}).x == doctest::Approx(0.50f) );
    DOCTEST_CHECK( compute_sphere_texcoords({+1,  0, 0}).x == doctest::Approx(0.75f) );
    DOCTEST_CHECK( compute_sphere_texcoords({ 0, -1, 0}).y == doctest::Approx(0.00f) );
    DOCTEST_CHECK( compute_sphere_texcoords({ 0,  0, 1}).y == doctest::Approx(0.50f) );
    DOCTEST_CHECK( compute_sphere_texcoords({ 0, +1, 0}).y == doctest::Approx(1.00f) );
}

std::optional<float> intersect_ray_plane(const ray & ray, const float4 & plane)
{
    float denom = dot(plane.xyz(), ray.direction);
    if(std::abs(denom) == 0) return std::nullopt;
    return -dot(plane, float4(ray.origin,1)) / denom;
}

std::optional<ray_triangle_hit> intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2)
{
    const float3 e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
    auto a = dot(e1, h);
    if(std::abs(a) == 0) return std::nullopt;

    float f = 1 / a;
    const float3 s = ray.origin - v0;
	const float u = f * dot(s, h);
	if(u < 0 || u > 1) return std::nullopt;

	const float3 q = cross(s, e1);
	const float v = f * dot(ray.direction, q);
	if(v < 0 || u + v > 1) return std::nullopt;

    const float t = f * dot(e2, q);
    if(t < 0) return std::nullopt;

    return ray_triangle_hit{t, {u,v}};
}
