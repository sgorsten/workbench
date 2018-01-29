#pragma once
#include "core.h"

// A value type representing a rectangular region of 2D space, which is considered to contain all points x,y such that x0 <= x < x1 and y0 <= y < y1
template<class T> struct rect 
{ 
    using vec2 = linalg::vec<T,2>;

    T x0, y0, x1, y1; 

    constexpr rect() : x0{}, y0{}, x1{}, y1{} {}
    constexpr rect(T x0, T y0, T x1, T y1) : x0{x0}, y0{y0}, x1{x1}, y1{y1} {}
    constexpr rect(vec2 corner00, vec2 corner11) : rect(corner00.x, corner00.y, corner11.x, corner11.y) {}

    // Observers
    constexpr bool empty() const noexcept { return x0 >= x1 || y0 >= y1; } // empty() is true if there exist no points which would satisfy contains(...)
    constexpr T width() const noexcept { return x1 - x0; }
    constexpr T height() const noexcept { return y1 - y0; }
    constexpr vec2 dims() const noexcept { return {width(), height()}; }
    constexpr vec2 corner00() const noexcept { return {x0, y0}; }
    constexpr vec2 corner10() const noexcept { return {x1, y0}; }
    constexpr vec2 corner01() const noexcept { return {x0, y1}; }
    constexpr vec2 corner11() const noexcept { return {x1, y1}; }
    constexpr auto aspect_ratio() const noexcept { return static_cast<std::common_type_t<T,float>>(width())/height(); }
    constexpr bool contains(vec2 coord) const noexcept { return x0 <= coord.x && y0 <= coord.y && coord.x < x1 && coord.y < y1; }

    // Transformations
    constexpr rect adjusted(T dx0, T dy0, T dx1, T dy1) const noexcept { return {x0+dx0, y0+dy0, x1+dx1, y1+dy1}; }
    constexpr rect intersected_with(rect r) const noexcept { return {std::max(x0, r.x0), std::max(y0, r.y0), std::min(x1, r.x1), std::min(y1, r.y1)}; }
    constexpr rect mirrored_x() const noexcept { return {x1, y0, x0, y1}; }
    constexpr rect mirrored_y() const noexcept { return {x0, y1, x1, y0}; }
    constexpr rect rotated_180() const noexcept { return {x1, y1, x0, y0}; }
    constexpr rect shrink(T amount) const noexcept { return adjusted(amount, amount, -amount, -amount); }

    // Mutators
    constexpr rect take_x0(T x) { rect r {x0, y0, x0+x, y1}; x0 = r.x1; return r; }
    constexpr rect take_x1(T x) { rect r {x1-x, y0, x1, y1}; x1 = r.x0; return r; }
    constexpr rect take_y0(T y) { rect r {x0, y0, x1, y0+y}; y0 = r.y1; return r; }
    constexpr rect take_y1(T y) { rect r {x0, y1-y, x1, y1}; y1 = r.y0; return r; }  
};

// Non-owning rectangular view over an array, with elements neither assumed to be contiguous nor row-major
template<class T> class grid_view
{
    const T * view_data; int2 view_dims, view_stride;
public:
    constexpr grid_view() noexcept : view_data{nullptr} {}
    constexpr grid_view(const T * data, int2 dims) noexcept : grid_view(data, dims, {1,dims.x}) {}
    constexpr grid_view(const T * data, int2 dims, int2 stride) noexcept : view_data{data}, view_dims{dims}, view_stride{stride} {}

    // Observers
    constexpr bool empty() const noexcept { return view_dims.x == 0 || view_dims.y == 0; }
    constexpr int width() const noexcept { return view_dims.x; }
    constexpr int height() const noexcept { return view_dims.y; }
    constexpr int2 dims() const noexcept { return view_dims; }
    constexpr int2 stride() const noexcept { return view_stride; }
    constexpr const T * data() const noexcept { return view_data; }
    constexpr const T & operator [] (int2 pos) const noexcept { return view_data[dot(pos,view_stride)]; }

    // Transformations
    constexpr grid_view mirrored_x() const noexcept { return {view_data + view_stride.x*(view_dims.x-1), view_dims, view_stride*int2(-1,1)}; }
    constexpr grid_view mirrored_y() const noexcept { return {view_data + view_stride.y*(view_dims.y-1), view_dims, view_stride*int2(1,-1)}; }
    constexpr grid_view transposed() const noexcept { return {view_data, {view_dims.y, view_dims.x}, {view_stride.y, view_stride.x}}; }
    constexpr grid_view subrect(const rect<int> & r) const noexcept { return {&(*this)[r.corner00()], r.dims(), view_stride}; }
};

// Value type modelling a dynamically sized rectangular array, with elements contiguously laid out in row-major order
template<class T> class grid
{
    std::unique_ptr<T[]> grid_data; int2 grid_dims;
public:
    grid() = default;
    explicit grid(int2 dims) : grid_data{new T[product(dims)]}, grid_dims{dims} {}
    grid(int2 dims, const T & value) : grid_data{new T[product(dims)]}, grid_dims{dims} { std::fill_n(grid_data.get(), product(dims), value); }
    grid(grid && r) noexcept : grid_data{move(r.grid_data)}, grid_dims{r.grid_dims} { r.clear(); }
    grid(const grid & r) : grid_data{new T[product(r.dims)]}, grid_dims{r.dims} { std::copy_n(r.grid_data.get(), product(dims), grid_data.get()); }

    // Observers
    bool empty() const noexcept { return grid_dims.x == 0 || grid_dims.y == 0; }
    int width() const noexcept { return grid_dims.x; }
    int height() const noexcept { return grid_dims.y; }
    int2 dims() const noexcept { return grid_dims; }
    int2 stride() const noexcept { return {1,grid_dims.x}; }
    const T * data() const noexcept { return grid_data.get(); }
    grid_view<T> view() const noexcept { return {data(), dims(), stride()}; }
    const T & operator [] (int2 pos) const noexcept { return grid_data[dot(pos,stride())]; }
    operator grid_view<T> () const noexcept { return view(); }

    // Transformations
    grid_view<T> mirrored_x() const noexcept { return view().mirrored_x(); }
    grid_view<T> mirrored_y() const noexcept { return view().mirrored_y(); }
    grid_view<T> transposed() const noexcept { return view().transposed(); }
    grid_view<T> subrect(const rect<int> & r) const noexcept { return view().subrect(r); }

    // Mutators
    void fill(const rect<int> & rect, const T & value) { for(int y=rect.y0; y<rect.y1; ++y) for(int x=rect.x0; x<rect.x1; ++x) (*this)[{x,y}] = value; }
    void blit(int2 pos, const grid_view<T> & view) { for(int2 p; p.y<view.height(); ++p.y) for(p.x=0; p.x<view.width(); ++p.x) (*this)[pos+p] = view[p]; }
    void clear() { grid_data.reset(); grid_dims={0,0}; }
    void resize(int2 dims) { grid g {dims}; g.blit({0,0}, view()); swap(g); }
    void resize(int2 dims, const T & value) { grid g {dims, value}; g.blit({0,0}, view()); swap(g); }
    void swap(grid & r) noexcept { std::swap(grid_data, r.grid_data); std::swap(grid_dims, r.grid_dims); }
    T * data() noexcept { return grid_data.get(); }
    T & operator [] (int2 pos) noexcept { return grid_data[dot(pos,stride())]; }
    grid & operator = (grid && r) noexcept { swap(r); return *this; }
    grid & operator = (const grid & r) { return *this = grid(r); } // TODO: Reuse memory if possible? Should we have an integral capacity(), reserve(), shrink_to_fit()?
};
