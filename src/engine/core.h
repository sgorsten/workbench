#pragma once
#include <vector>
#include <variant>
#include <optional>
#include <string_view>
#include <sstream>
#include <functional>

#include "linalg.h"
using namespace linalg::aliases;

// Globally include doctest when compiling in debug mode
#ifdef NDEBUG
#define DOCTEST_CONFIG_DISABLE
#endif
#define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
#include <doctest.h>

[[noreturn]] void fail_fast(); // Call debug_break() and then exit
void debug_break(); // If a debugger is attached, break, otherwise do nothing

// Non-owning view over a contiguous range of values of type T, a generalization of std::string_view without find functions or comparison operators
template<class T> class array_view
{
    const T * view_data;
    size_t view_size;
public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using const_iterator = const T *;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    constexpr array_view() noexcept : view_data{nullptr}, view_size{0} {}
    constexpr array_view(const array_view & view) noexcept = default;
    constexpr array_view(const T * s, size_type count) noexcept : view_data{s}, view_size{count} {}
    constexpr array_view(const std::vector<T> & vec) noexcept : array_view{vec.data(), vec.size()} {}
    constexpr array_view(std::initializer_list<T> ilist) noexcept : array_view{ilist.begin(), ilist.size()} {}   
    template<size_type N> constexpr array_view(const T (& array)[N]) : array_view{array, N} {}

    constexpr array_view & operator = (const array_view & view) noexcept = default;

    constexpr const_iterator begin() const noexcept { return view_data; }
    constexpr const_iterator cbegin() const noexcept { return view_data; }
    constexpr const_iterator end() const noexcept { return view_data + view_size; }
    constexpr const_iterator cend() const noexcept { return view_data + view_size; }    
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

    constexpr const_reference operator[] (size_type pos) const noexcept { return view_data[pos]; }
    constexpr const_reference at(size_type pos) const { return pos < size() ? view_data[pos] : throw std::out_of_range(); }
    constexpr const_reference front() const noexcept { return view_data[0]; }
    constexpr const_reference back() const noexcept { return view_data[view_size-1]; }
    constexpr const_pointer data() const noexcept { return view_data; }

    constexpr size_type size() const noexcept { return view_size; }
    constexpr size_type max_size() const noexcept { return static_cast<size_type>(std::numeric_limits<difference_type>::max()) };
    constexpr bool empty() const noexcept { return view_size == 0; }

    constexpr void remove_prefix(size_type n) noexcept { view_data += n; view_size -= n; }
    constexpr void remove_suffix(size_type n) noexcept { view_size -= n; }
    constexpr void swap(array_view & view) noexcept { const array_view temp{view}; view=*this; *this=temp; }
    constexpr array_view substr(size_type pos=0, size_type count=npos) const { return pos <= view_size ? {view_data+pos, std::min(count, view_size-pos)} : throw std::out_of_range(); }

    static constexpr size_type npos = size_type(-1);
};

// Abstract over determining the number of elements in a collection
template<class T, size_t N> constexpr size_t countof(const T (&)[N]) { return N; }
template<class T, size_t N> constexpr size_t countof(const std::array<T,N> &) { return N; }
template<class T> constexpr size_t countof(const std::vector<T> & vec) { return vec.size(); }
template<class T> constexpr size_t countof(std::initializer_list<T> ilist) { return ilist.size(); }
template<class T> constexpr size_t countof(array_view<T> view) { return view.size(); }

// equivalent(a, b) returns true if a and b represent the same number, independent of underlying type
template<class A, class B> constexpr std::enable_if_t<std::is_arithmetic_v<A> && std::is_arithmetic_v<B>, bool> equivalent(A a, B b)
{
    if constexpr(std::is_floating_point_v<A> != std::is_floating_point_v<B>) return a == static_cast<A>(b) && static_cast<B>(a) == b;
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

// Helper to produce overload sets of lambdas
template<class... F> struct overload_set;
template<class Last> struct overload_set<Last> : Last
{
    overload_set(Last last) : Last(last) {}
    using Last::operator();
};
template<class First, class... Rest> struct overload_set<First,Rest...> : First, overload_set<Rest...>
{
    overload_set(First first, Rest... rest) : First(first), overload_set<Rest...>(rest...) {}
    using First::operator();
    using overload_set<Rest...>::operator();
};
template<class... F> overload_set<F...> overload(F... f) { return {f...}; }

// A non-owning view of a callable object of a specific signature.
// Requires no dynamic memory allocation, and imposes only one additional indirection per call.
template<class S> class function_view;
template<class R, class... A> class function_view<R(A...)>
{
    const void * user;                  // Type-erased pointer to the original callable object
    R (* func)(const void *, A...);     // Pointer to function which dispatches call to original object
public:
    template<class F> function_view(const F & f) : user(&f) { func = [](const void * u, A... a) -> R { return (*reinterpret_cast<const F *>(u))(std::forward<A>(a)...); }; }
    R operator()(A... args) const { return func(user, std::forward<A>(args)...); }
};

// Determine the offset of a non virtually inherited member variable of a class type
template<class C, class T> intptr_t member_offset(T C::*member_pointer) { C object; return reinterpret_cast<char *>(&(object.*member_pointer)) - reinterpret_cast<char *>(&object); }

// Round an integral value up to the next whole multiple of the alignment parameter
template<class T> T round_up(T value, T alignment) { return (value+alignment-1)/alignment*alignment; }

// Helper for forming strings via an ostringstream
template<class... T> std::string to_string(T && ... args)
{
    std::ostringstream ss;
    const int z[] {(ss << std::forward<T>(args), 0)...};
    return ss.str();
}
