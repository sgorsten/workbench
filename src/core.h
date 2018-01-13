#pragma once
#include <cstdint>
#include <array>
#include <vector>

[[noreturn]] void fail_fast(); // Call debug_break() and then exit
void debug_break(); // If a debugger is attached, break, otherwise do nothing

// equivalent(a, b) returns true if a and b represent the same integer value, independent of underlying type
template<class A, class B> constexpr std::enable_if_t<std::is_integral_v<A> && std::is_integral_v<B>, bool> equivalent(A a, B b)
{
    if constexpr(std::is_signed_v<A> && std::is_unsigned_v<B>) if(a < 0) return false;
    if constexpr(std::is_signed_v<B> && std::is_unsigned_v<A>) if(b < 0) return false;
    return a == b;
}

// exact_cast<T>(x) is like static_cast<T>(x) but fails if the resulting value is not equivalent to x
template<class T, class U> T exact_cast(U value)
{
    T casted_value = static_cast<T>(value);
    if(!equivalent(casted_value, value)) fail_fast();
    return casted_value;
}

// exactly(x) wraps x in a struct that will implicitly cast to other types using exact_cast
template<class U> struct exact_caster
{
    U value;
    operator U () const { return value; }
    template<class T> operator T () const { return exact_cast<T>(value); }
};
template<class U> exact_caster<U> exactly(U value) { return {value}; }

// Abstract over determining the number of elements in a collection
template<class T, size_t N> constexpr size_t countof(const T (&)[N]) { return N; }
template<class T, size_t N> constexpr size_t countof(const std::array<T,N> &) { return N; }
template<class T> constexpr size_t countof(const std::initializer_list<T> & ilist) { return ilist.size(); }
template<class T> constexpr size_t countof(const std::vector<T> & vec) { return vec.size(); }