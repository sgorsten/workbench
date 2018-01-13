#include "geometry.h"
#include "test.h"

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
