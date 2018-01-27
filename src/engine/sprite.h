#pragma once
#include "load.h"
#include <map>

template<class T> struct rect 
{ 
    using value_type = T;
    using dims_type = linalg::vec<T,2>;
    using floating_point = std::common_type_t<T,float>;    

    T x0, y0, x1, y1; 

    constexpr rect() : x0{}, y0{}, x1{}, y1{} {}
    constexpr rect(T x0, T y0, T x1, T y1) : x0{x0}, y0{y0}, x1{x1}, y1{y1} {}

    constexpr T width() const noexcept { return x1 - x0; }
    constexpr T height() const noexcept { return y1 - y0; }
    constexpr dims_type dims() const noexcept { return {width(), height()}; }
    constexpr floating_point aspect_ratio() const noexcept { return static_cast<floating_point>(width())/height(); }
    constexpr bool contains(dims_type pos) const noexcept { return x0 <= pos.x && y0 <= pos.y && pos.x < x1 && pos.y < y1; }
    constexpr rect adjusted(int dx0, int dy0, int dx1, int dy1) const noexcept { return {x0+dx0, y0+dy0, x1+dx1, y1+dy1}; }

    constexpr rect take_x0(int x) { rect r {x0, y0, x0+x, y1}; x0 = r.x1; return r; }
    constexpr rect take_x1(int x) { rect r {x1-x, y0, x1, y1}; x1 = r.x0; return r; }
    constexpr rect take_y0(int y) { rect r {x0, y0, x1, y0+y}; y0 = r.y1; return r; }
    constexpr rect take_y1(int y) { rect r {x0, y1-y, x1, y1}; y1 = r.y0; return r; }    
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

    void blit(dims_type pos, const grid & other) { const int2 dims = min(other.grid_dims, grid_dims-pos); for(int2 p; p.y<dims.y; ++p.y) for(p.x=0; p.x<dims.x; ++p.x) operator[](pos+p) = other[p]; }

    void clear() { grid_data.reset(); grid_dims={0,0}; }
    void resize(dims_type dims) { grid g {dims}; g.blit({0,0}, *this); swap(g); }
    void resize(dims_type dims, const value_type & value) { grid g {dims, value}; g.blit({0,0}, *this); swap(g); }
    void swap(grid & other) noexcept { std::swap(grid_data, other.grid_data); std::swap(grid_dims, other.grid_dims); }
};

// sprite_sheet is a simple class which packs a collection of 2D images into a single atlas texture
struct sprite
{
    grid<uint8_t> img;      // The contents of the sprite
    int border;             // Number of pixels from the edge of the image which are not considered part of the sprite (but should be copied to the atlas anyway)
    rect<float> texcoords;  // The subrect of this sprite within the texture atlas
};
struct sprite_sheet
{
    grid<uint8_t> sheet_image;
    std::vector<sprite> sprites;

    size_t add_sprite(grid<uint8_t> img, int border);
    void prepare_sheet();
};

// font_face rasterizes a collection of text glyphs into a sprite_sheet
struct glyph_info
{
    size_t sprite_index;
    int2 offset;
    int advance;
};
struct font_face
{
    sprite_sheet & sheet;
    std::map<int, glyph_info> glyphs;
    int line_height, baseline;

    font_face(sprite_sheet & sheet, const std::vector<std::byte> & font_data, float pixel_height);
    int get_text_width(std::string_view text) const;
};

// canvas_sprites rasterizes a collection of useful shapes into a sprite_sheet
struct canvas_sprites
{
    sprite_sheet & sheet;
    size_t solid_pixel;
    std::map<int, size_t> corner_sprites, line_sprites;

    canvas_sprites(sprite_sheet & sheet);
};

#include "graphics.h"
using corner_flags = int;
enum corner_flag : corner_flags
{
    top_left_corner     = 1<<0,
    top_right_corner    = 1<<1,
    bottom_left_corner  = 1<<2,
    bottom_right_corner = 1<<3,
};
struct ui_vertex { float2 position, texcoord; float4 color; };
class canvas
{
    struct list { rect<int> scissor; uint32_t level,first,last; };
    const canvas_sprites & sprites;
    gfx::transient_resource_pool & pool;
    int2 dims;
    std::vector<list> lists;
    std::vector<rect<int>> scissors;
    uint32_t vertex_count;
public:
    canvas(const canvas_sprites & sprites, gfx::transient_resource_pool & pool, const int2 & dims);
    
    void begin_overlay();
    void end_overlay();
    void begin_scissor(const rect<int> & r);
    void end_scissor();

    void draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color);
    void draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color);

    void draw_rect(const rect<int> & r, const float4 & color);
    void draw_circle(const int2 & center, int radius, const float4 & color);
    void draw_rounded_rect(const rect<int> & r, int corner_radius, const float4 & color);
    void draw_partial_rounded_rect(const rect<int> & r, int corner_radius, corner_flags corners, const float4 & color);
    void draw_convex_polygon(array_view<ui_vertex> vertices);

    void draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords);
    void draw_sprite_sheet(const int2 & p);

    void draw_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint);
    void draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);
    void draw_shadowed_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);

    void encode_commands(rhi::command_buffer & cmd);
};
