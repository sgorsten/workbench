#pragma once
#include "load.h"
#include <map>
#include "grid.h"

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

namespace utf8
{
    const char * prev(const char * units);
    const char * next(const char * units);
    uint32_t code(const char * units);
    std::array<char,5> units(uint32_t code);
    bool is_valid(std::string_view units);
}

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

    font_face(sprite_sheet & sheet, const std::vector<std::byte> & font_data, float pixel_height, uint32_t min_codepoint, uint32_t max_codepoint);
    int get_text_width(std::string_view text) const;
    int get_cursor_pos(std::string_view text, int x) const;
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
    void draw_wire_rect(const rect<int> & r, int width, const float4 & color);

    void draw_rect(const rect<int> & r, const float4 & color);
    void draw_circle(const int2 & center, int radius, const float4 & color);
    void draw_rounded_rect(const rect<int> & r, int corner_radius, const float4 & color);
    void draw_partial_rounded_rect(const rect<int> & r, int corner_radius, corner_flags corners, const float4 & color);
    void draw_convex_polygon(array_view<ui_vertex> vertices);

    void draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords);
    void draw_sprite_sheet(const int2 & p);

    void draw_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint);
    void draw_shadowed_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint);
    void draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);
    void draw_shadowed_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text);

    void encode_commands(rhi::command_buffer & cmd);
};
