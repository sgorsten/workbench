#pragma once
#include "load.h"
#include <map>

struct rect 
{ 
    int x0, y0, x1, y1; 

    int width() const { return x1 - x0; }
    int height() const { return y1 - y0; }
    int2 dims() const { return {width(), height()}; }
    float aspect_ratio() const { return (float)width()/height(); }

    rect adjusted(int dx0, int dy0, int dx1, int dy1) const { return {x0+dx0, y0+dy0, x1+dx1, y1+dy1}; }

    rect take_x0(int x) { rect r {x0, y0, x0+x, y1}; x0 = r.x1; return r; }
    rect take_x1(int x) { rect r {x1-x, y0, x1, y1}; x1 = r.x0; return r; }
    rect take_y0(int y) { rect r {x0, y0, x1, y0+y}; y0 = r.y1; return r; }
    rect take_y1(int y) { rect r {x0, y1-y, x1, y1}; y1 = r.y0; return r; }
};

// sprite_sheet is a simple class which packs a collection of 2D images into a single atlas texture
struct sprite
{
    image img;              // The contents of the sprite
    int border;             // Number of pixels from the edge of the image which are not considered part of the sprite (but should be copied to the atlas anyway)
    float s0, t0, s1, t1;   // The subrect of this sprite within the texture atlas
};
struct sprite_sheet
{
    image img;
    std::vector<sprite> sprites;

    size_t add_sprite(image img, int border);
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

// gui_sprites rasterizes a collection of useful shapes into a sprite_sheet
struct gui_sprites
{
    sprite_sheet & sheet;
    size_t solid_pixel;
    std::map<int, size_t> corner_sprites, line_sprites;

    gui_sprites(sprite_sheet & sheet);
};

#include "graphics.h"
struct ui_vertex { float2 position, texcoord; float4 color; };
class gui_context
{
    struct list { rect scissor; uint32_t level,first,last; };
    const gui_sprites & sprites;
    gfx::transient_resource_pool & pool;
    int2 dims;
    std::vector<list> lists;
    std::vector<rect> scissors;
    uint32_t vertex_count;
public:
    gui_context(const gui_sprites & sprites, gfx::transient_resource_pool & pool, const int2 & dims);
    
    void begin_overlay();
    void end_overlay();
    void begin_scissor(const rect & r);
    void end_scissor();

    void draw_quad(ui_vertex v0, ui_vertex v1, ui_vertex v2, ui_vertex v3);
    void draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color);
    void draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color);

    void draw_rect(const rect & r, const float4 & color);
    void draw_rounded_rect(rect r, int radius, const float4 & color);
    void draw_partial_rounded_rect(rect r, int radius, const float4 & color, bool tl, bool tr, bool bl, bool br);
    void draw_circle(const int2 & center, int radius, const float4 & color);

    void draw_sprite_sheet(const int2 & p);
    void draw_sprite(const rect & r, float s0, float t0, float s1, float t1, const float4 & color);
    void draw_text(const font_face & font, const float4 & color, int2 pos, std::string_view text);
    void draw_shadowed_text(const font_face & font, const float4 & color, int2 pos, std::string_view text);

    void draw(rhi::command_buffer & cmd);
};
