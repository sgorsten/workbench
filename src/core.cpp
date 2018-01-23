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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
void debug_break()
{
    if(IsDebuggerPresent()) DebugBreak();
}
#endif