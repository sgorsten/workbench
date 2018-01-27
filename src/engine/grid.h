#pragma once
template<class T> struct rect 
{ 
    using value_type = T;
    using dims_type = linalg::vec<T,2>;
    using floating_point = std::common_type_t<T,float>;    

    T x0, y0, x1, y1; 

    constexpr rect() : x0{}, y0{}, x1{}, y1{} {}
    constexpr rect(T x0, T y0, T x1, T y1) : x0{x0}, y0{y0}, x1{x1}, y1{y1} {}
    constexpr rect(dims_type top_left, dims_type bottom_right) : x0{top_left.x}, y0{top_left.y}, x1{bottom_right.x}, y1{bottom_right.y} {}

    constexpr T width() const noexcept { return x1 - x0; }
    constexpr T height() const noexcept { return y1 - y0; }
    constexpr dims_type dims() const noexcept { return {width(), height()}; }
    constexpr dims_type top_left() const noexcept { return {x0,y0}; }
    constexpr dims_type top_right() const noexcept { return {x1,y0}; }
    constexpr dims_type bottom_left() const noexcept { return {x0,y1}; }
    constexpr dims_type bottom_right() const noexcept { return {x1,y1}; }
    constexpr floating_point aspect_ratio() const noexcept { return static_cast<floating_point>(width())/height(); }
    constexpr bool contains(dims_type pos) const noexcept { return x0 <= pos.x && y0 <= pos.y && pos.x < x1 && pos.y < y1; }
    constexpr rect adjusted(int dx0, int dy0, int dx1, int dy1) const noexcept { return {x0+dx0, y0+dy0, x1+dx1, y1+dy1}; }

    constexpr rect take_x0(int x) { rect r {x0, y0, x0+x, y1}; x0 = r.x1; return r; }
    constexpr rect take_x1(int x) { rect r {x1-x, y0, x1, y1}; x1 = r.x0; return r; }
    constexpr rect take_y0(int y) { rect r {x0, y0, x1, y0+y}; y0 = r.y1; return r; }
    constexpr rect take_y1(int y) { rect r {x0, y1-y, x1, y1}; y1 = r.y0; return r; }    
};

// Non-owning rectangular view over an array, where elements within rows are contiguous
template<class T> class grid_view
{
    const T * view_data;
    int2 view_dims;
    int row_stride;
public:
    using value_type = T;
    using dims_type = int2;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    constexpr grid_view() noexcept : view_data{nullptr}, view_size{0}, row_stride{0} {}
    constexpr grid_view(const grid_view & view) noexcept = default;
    constexpr grid_view(const T * s, dims_type dims) noexcept : view_data{s}, view_dims{dims}, row_stride{dims.x} {}
    constexpr grid_view(const T * s, dims_type dims, int stride) noexcept : view_data{s}, view_dims{dims}, row_stride{stride} {}

    constexpr grid_view & operator = (const grid_view & view) noexcept = default;

    constexpr const_reference at(dims_type pos) const { return rect<int>{0,0,view_dims.x,view_dims.y}.contains(pos) ? operator[](pos) : throw std::out_of_range(); }
	constexpr const_reference operator [] (dims_type pos) const noexcept { return view_data[pos.y*row_stride+pos.x]; }
    constexpr const T * data() const noexcept { return view_data; }

    constexpr bool empty() const noexcept { return view_dims.x == 0 || view_dims.y == 0; }
    constexpr int width() const noexcept { return view_dims.x; }
    constexpr int height() const noexcept { return view_dims.y; }
    constexpr dims_type dims() const noexcept { return view_dims; }
    constexpr int stride() const noexcept { return row_stride; }
    constexpr bool contiguous() const noexcept { return row_stride == view_dims.x; }

    constexpr grid_view subrect(const rect<int> & r) const noexcept { return {&operator[](r.top_left()), r.dims(), row_stride}; }
    constexpr void swap(grid_view & view) noexcept { const grid_view temp{view}; view=*this; *this=temp; }
};

// A 2D analog to std::vector<T>, with contiguous memory laid out in row-major order
template<class T> class grid
{
    std::unique_ptr<T[]> grid_data;
    int2 grid_dims;
public:
    using value_type = T;
    using dims_type = int2;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    grid() = default;
    explicit grid(dims_type dims) : grid_data{new T[product(dims)]}, grid_dims{dims} {}
    grid(dims_type dims, const T & value) : grid_data{new T[product(dims)]}, grid_dims{dims} { std::fill_n(grid_data.get(), product(dims), value); }
    grid(const grid & other)  : grid_data{new T[product(other.dims)]}, grid_dims{other.dims} { std::copy_n(other.grid_data.get(), product(dims), grid_data.get()); }
    grid(grid && other) noexcept : grid_data{move(other.grid_data)}, grid_dims{other.grid_dims} { other.clear(); }

    grid & operator = (const grid & other) { return *this = grid(other); } // TODO: Reuse memory if possible? Should we have an integral capacity(), reserve(), shrink_to_fit()?
    grid & operator = (grid && other) noexcept { swap(other); return *this; }

    reference at(dims_type pos) { return rect<int>{0,0,grid_dims.x,grid_dims.y}.contains(pos) ? operator[](pos) : throw std::out_of_range(); }
    const_reference at(dims_type pos) const { return rect<int>{0,0,grid_dims.x,grid_dims.y}.contains(pos) ? operator[](pos) : throw std::out_of_range(); }
    reference operator [] (dims_type pos) noexcept { return grid_data.get()[pos.y*grid_dims.x+pos.x]; }
	const_reference operator [] (dims_type pos) const noexcept { return grid_data.get()[pos.y*grid_dims.x+pos.x]; }
    T * data() noexcept { return grid_data.get(); }
    const T * data() const noexcept { return grid_data.get(); }

    bool empty() const noexcept { return grid_dims.x == 0 || grid_dims.y == 0; }
    int width() const noexcept { return grid_dims.x; }
    int height() const noexcept { return grid_dims.y; }
    dims_type dims() const noexcept { return grid_dims; }

    void blit(dims_type pos, const grid_view<T> & other) { const int2 dims = min(other.dims(), grid_dims-pos); for(int2 p; p.y<dims.y; ++p.y) for(p.x=0; p.x<dims.x; ++p.x) operator[](pos+p) = other[p]; }

    void clear() { grid_data.reset(); grid_dims={0,0}; }
    void resize(dims_type dims) { grid g {dims}; g.blit({0,0}, *this); swap(g); }
    void resize(dims_type dims, const value_type & value) { grid g {dims, value}; g.blit({0,0}, *this); swap(g); }
    void swap(grid & other) noexcept { std::swap(grid_data, other.grid_data); std::swap(grid_dims, other.grid_dims); }

    operator grid_view<T> () const noexcept { return {grid_data.get(), grid_dims}; }
    grid_view<T> subrect(const rect<int> & r) const noexcept { return {&operator[](r.top_left()), r.dims(), grid_dims.x}; }
};