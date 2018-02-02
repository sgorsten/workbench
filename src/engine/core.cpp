#include "core.h"
#include <iostream>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

void fail_fast()
{
    debug_break();
    std::cerr << "fail_fast() called." << std::endl;
    std::exit(EXIT_FAILURE);
}

DOCTEST_TEST_CASE("comparisons")
{
    DOCTEST_CHECK( equivalent<int8_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int8_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int8_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int8_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int16_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int16_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int16_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int16_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int32_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int32_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int32_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int32_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int64_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int64_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int64_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<int64_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, int8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, int16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, int32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, int64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, uint8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, uint16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, uint32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint8_t, uint64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, uint8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, uint16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, uint32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint16_t, uint64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, uint8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, uint16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, uint32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint32_t, uint64_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, uint8_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, uint16_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, uint32_t>(10, 10) );
    DOCTEST_CHECK( equivalent<uint64_t, uint64_t>(10, 10) );

    DOCTEST_CHECK( equivalent<int8_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int8_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int8_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int8_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int16_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int16_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int16_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int16_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int32_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int32_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int32_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int32_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int64_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int64_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int64_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( equivalent<int64_t, int64_t>(-10, -10) );

    DOCTEST_CHECK( !equivalent<uint8_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint8_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint8_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint8_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint16_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint16_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint16_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint16_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint32_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint32_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint32_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint32_t, int64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint64_t, int8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint64_t, int16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint64_t, int32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<uint64_t, int64_t>(-10, -10) );

    DOCTEST_CHECK( !equivalent<int8_t, uint8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int8_t, uint16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int8_t, uint32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int8_t, uint64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int16_t, uint8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int16_t, uint16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int16_t, uint32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int16_t, uint64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int32_t, uint8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int32_t, uint16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int32_t, uint32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int32_t, uint64_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int64_t, uint8_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int64_t, uint16_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int64_t, uint32_t>(-10, -10) );
    DOCTEST_CHECK( !equivalent<int64_t, uint64_t>(-10, -10) );

    DOCTEST_CHECK( equivalent<int, float>(10, 10.0f) );
    DOCTEST_CHECK( !equivalent<int, float>(10, 10.001f) );

    DOCTEST_CHECK( equivalent<int, float>(16777216, 16777216.0f) );
    DOCTEST_CHECK( !equivalent<int, float>(16777217, 16777217.0f) );
}

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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
void debug_break()
{
    if(IsDebuggerPresent()) DebugBreak();
}
#endif