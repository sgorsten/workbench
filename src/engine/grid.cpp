#include "grid.h"

DOCTEST_TEST_CASE("Test view transformations")
{
    // Form a rectangular view over an array
    const int elements[] {1,2,3,4,5, 6,7,8,9,10, 11,12,13,14,15, 16,17,18,19,20};
    const auto view = grid_view<int>(elements, {5,4});
    DOCTEST_REQUIRE(!view.empty());
    DOCTEST_REQUIRE(view.width() == 5);
    DOCTEST_REQUIRE(view.height() == 4);
    DOCTEST_REQUIRE(view.dims() == int2{5,4});
    DOCTEST_REQUIRE(view.stride() == int2{1,5});
    DOCTEST_REQUIRE(view.data() == elements);

    // Take a subrect of the view
    auto sub = view.subrect({1,1,4,3});
    DOCTEST_REQUIRE(!sub.empty());
    DOCTEST_REQUIRE(sub.dims() == int2{3,2});
    DOCTEST_REQUIRE(sub.stride() == int2{1,5});
    DOCTEST_REQUIRE(sub[{0,0}] == 7);
    DOCTEST_REQUIRE(sub[{1,0}] == 8);
    DOCTEST_REQUIRE(sub[{2,0}] == 9);
    DOCTEST_REQUIRE(sub[{0,1}] == 12);
    DOCTEST_REQUIRE(sub[{1,1}] == 13);
    DOCTEST_REQUIRE(sub[{2,1}] == 14);

    DOCTEST_SUBCASE("mirrored_x() reverses the order of elements in each row")
    {
        const auto mx = sub.mirrored_x();
        DOCTEST_CHECK(!mx.empty());
        DOCTEST_CHECK(mx.dims() == int2{3,2});
        DOCTEST_CHECK(mx.stride() == int2{-1,5});
        DOCTEST_CHECK(mx[{0,0}] == 9);
        DOCTEST_CHECK(mx[{1,0}] == 8);
        DOCTEST_CHECK(mx[{2,0}] == 7);
        DOCTEST_CHECK(mx[{0,1}] == 14);
        DOCTEST_CHECK(mx[{1,1}] == 13);
        DOCTEST_CHECK(mx[{2,1}] == 12);
    }

    DOCTEST_SUBCASE("mirrored_y() reverses the order of elements in each column")
    {
        const auto my = sub.mirrored_y();
        DOCTEST_REQUIRE(!my.empty());
        DOCTEST_REQUIRE(my.dims() == int2{3,2});
        DOCTEST_REQUIRE(my.stride() == int2{1,-5});
        DOCTEST_REQUIRE(my[{0,0}] == 12);
        DOCTEST_REQUIRE(my[{1,0}] == 13);
        DOCTEST_REQUIRE(my[{2,0}] == 14);
        DOCTEST_REQUIRE(my[{0,1}] == 7);
        DOCTEST_REQUIRE(my[{1,1}] == 8);
        DOCTEST_REQUIRE(my[{2,1}] == 9);
    }

    DOCTEST_SUBCASE("transposed() exchanges rows and columns")
    {
        const auto t = sub.transposed();
        DOCTEST_REQUIRE(!t.empty());
        DOCTEST_REQUIRE(t.dims() == int2{2,3});
        DOCTEST_REQUIRE(t.stride() == int2{5,1});
        DOCTEST_REQUIRE(t[{0,0}] == 7);
        DOCTEST_REQUIRE(t[{1,0}] == 12);
        DOCTEST_REQUIRE(t[{0,1}] == 8);
        DOCTEST_REQUIRE(t[{1,1}] == 13);
        DOCTEST_REQUIRE(t[{0,2}] == 9);   
        DOCTEST_REQUIRE(t[{1,2}] == 14);
    }
}
