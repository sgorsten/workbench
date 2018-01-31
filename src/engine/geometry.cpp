#include "geometry.h"

static constexpr coord_axis all_axes[] {coord_axis::forward, coord_axis::back, coord_axis::left, coord_axis::right, coord_axis::up, coord_axis::down};

DOCTEST_TEST_CASE("dot product of coord_axis and itself is one")
{
    for(auto a : all_axes) DOCTEST_CHECK(dot(a, a) == 1);
}

DOCTEST_TEST_CASE("dot product of coord_axis is symmetric")
{
    for(auto a : all_axes) for(auto b : all_axes) DOCTEST_CHECK(dot(a,b) == dot(b,a));
}

DOCTEST_TEST_CASE("dot product of coord_axis and its opposite is negative one")
{
    DOCTEST_CHECK(dot(coord_axis::forward, coord_axis::back) == -1);
    DOCTEST_CHECK(dot(coord_axis::left, coord_axis::right) == -1);
    DOCTEST_CHECK(dot(coord_axis::up, coord_axis::down) == -1);
}

DOCTEST_TEST_CASE("dot products of orthogonal coord_axis values are zero")
{
    int pos_count=0, neg_count=0, zero_count=0;
    for(auto a : all_axes) for(auto b : all_axes)
    {
        if(dot(a,b) > 0) ++pos_count;
        if(dot(a,b) < 0) ++neg_count;
        if(dot(a,b) == 0) ++zero_count;
    }
    DOCTEST_CHECK(pos_count == 6);
    DOCTEST_CHECK(neg_count == 6);
    DOCTEST_CHECK(zero_count == 24);
}

DOCTEST_TEST_CASE("check handedness of all coordinate systems")
{
    int ortho=0, left=0, right=0;
    for(auto x : all_axes) for(auto y : all_axes) for(auto z : all_axes)
    {
        const coord_system coords {x, y, z};
        if(coords.is_orthogonal()) ++ortho;
        if(coords.is_left_handed()) ++left;
        if(coords.is_right_handed()) ++right;
    }
    DOCTEST_CHECK(ortho == 48);
    DOCTEST_CHECK(left == 24);
    DOCTEST_CHECK(right == 24);
}

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
