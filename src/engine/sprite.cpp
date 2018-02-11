#include "sprite.h"
#include "shader.h"

//////////////////
// sprite_sheet //
//////////////////

size_t sprite_sheet::add_sprite(grid<uint8_t> img, int border)
{
    const size_t index = sprites.size();
    sprites.push_back({std::move(img), border});
    return index;
}

void sprite_sheet::prepare_sheet()
{
    // Sort glyphs by descending height, then descending width
    std::vector<sprite *> sorted_sprites;
    for(auto & g : sprites) sorted_sprites.push_back(&g);
    std::sort(begin(sorted_sprites), end(sorted_sprites), [](const sprite * a, const sprite * b)
    {
        return std::make_tuple(a->img.dims().y, a->img.dims().x) > std::make_tuple(b->img.dims().y, b->img.dims().x);
    });

    int2 tex_dims = {64, 64};
    while(true)
    {
        sheet_image.clear();
        sheet_image.resize(tex_dims);

        bool bad_pack = false;
        int2 used {0, 0};
        int next_y = 0;
        for(auto * s : sorted_sprites)
        {
            if(used.x + s->img.width() > sheet_image.width()) used = {0, next_y};
            if(used.x + s->img.width() > sheet_image.width() || used.y + s->img.height() > sheet_image.height()) 
            {
                bad_pack = true;
                break;
            }

            s->texcoords = {float2(used+s->border)/float2(sheet_image.dims()), float2(used+s->img.dims()-s->border)/float2(sheet_image.dims())};

            sheet_image.blit(used, s->img);

            used.x += s->img.width();
            next_y = std::max(next_y, used.y + s->img.height());
        }
        if(bad_pack)
        {
            if(tex_dims.x == tex_dims.y) tex_dims.x *= 2;
            else tex_dims.y *= 2;
        }
        else break;
    }
}

//////////
// utf8 //
//////////

namespace utf8
{
    size_t get_code_length(char ch)
    { 
        const auto byte = static_cast<uint8_t>(ch);
        if(byte < 0x80) return 1;
        if(byte < 0xC0) return 0;
        if(byte < 0xE0) return 2;
        if(byte < 0xF0) return 3;
        if(byte < 0xF8) return 4;
        return 0;
    }

    bool is_continuation_byte(char ch)
    { 
        const auto byte = static_cast<uint8_t>(ch);
        return byte >= 0x80 && byte < 0xC0; 
    }

    const char * prev(const char * units)
    {
        do { --units; } while(is_continuation_byte(*units));
        return units;
    }

    const char * next(const char * units)
    {
        return units + get_code_length(*units);
    }

    uint32_t code(const char * units)
    {
        static const uint8_t masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
        auto length = get_code_length(*units);
        uint32_t codepoint = units[0] & masks[length];
        for(int i=1; i<length; ++i)
        {
            codepoint = (codepoint << 6) | (units[i] & 0x3F);
        }
        return codepoint;
    }

    std::array<char,5> units(uint32_t code)
    {
        if(code < 0x80) return {static_cast<char>(code)};
        if(code < 0x800) return {static_cast<char>(0xC0|((code>>6)&0x1F)), static_cast<char>(0x80|(code&0x3F))};
        if(code < 0x10000) return {static_cast<char>(0xE0|((code>>12)&0x0F)), static_cast<char>(0x80|((code>>6)&0x3F)), static_cast<char>(0x80|(code&0x3F))};
        return {static_cast<char>(0xF0|((code>>18)&0x07)), static_cast<char>(0x80|((code>>12)&0x3F)), static_cast<char>(0x80|((code>>6)&0x3F)), static_cast<char>(0x80|(code&0x3F))};
    }

    bool is_valid(std::string_view units)
    {
        while(!units.empty())
        {
            auto length = get_code_length(units.front());
            if(length == 0 || length > units.size()) return false;
            for(int i=1; i<length; ++i) if(!is_continuation_byte(units[i])) return false;
            units.remove_prefix(length);
        }
        return true;
    }
}

/////////////////
// gui_sprites //
/////////////////

static void compute_circle_quadrant_coverage(float coverage[], int radius)
{
    const float rr = static_cast<float>(radius * radius);
    auto function = [rr](float x) { return sqrt(rr - x*x); };
    auto antiderivative = [rr, function](float x) { return (x * function(x) + rr * atan(x/function(x))) / 2; };
    auto integral = [antiderivative](float x0, float x1) { return antiderivative(x1) - antiderivative(x0); };

    for(int i=0; i<radius; ++i)
    {
        const float x0 = i+0.0f, x1 = i+1.0f;
        const float y0 = function(x0);
        const float y1 = function(x1);
        const int y0i = (int)y0, y1i = (int)y1;

        for(int j=i; j<y1i; ++j)
        {
            coverage[i*radius+j] = coverage[j*radius+i] = 1.0f;
        }

        if(y0i == y1i)
        {
            float c = integral(x0, x1) - y1i*(x1-x0);
            coverage[i*radius+y1i] = c;
            coverage[y1i*radius+i] = c;
        }
        else
        {
            const float cross_x = function(static_cast<float>(y0i)); // X location where curve passes from pixel y0i to pixel y1i

            // Coverage for pixel at (i,y0i) is the area under the curve from x0 to cross_x
            if(y0i < radius) coverage[i*radius+y0i] = coverage[y0i*radius+i] = integral(x0, cross_x) - y0i*(cross_x-x0);

            // Coverage for pixel at (i,y1i) is the area of a rectangle from x0 to cross_x, and the area under the curve from cross_x to x1
            if(y1i == y0i - 1) coverage[i*radius+y1i] = coverage[y1i*radius+i] = (cross_x-x0) + integral(cross_x, x1) - y1i*(x1-cross_x);
            else break; // Stop after the first octant
        }
    }
}

grid<uint8_t> make_bordered_circle_quadrant(int radius)
{
    std::vector<float> coverage(radius*radius);
    compute_circle_quadrant_coverage(coverage.data(), radius);
    auto in = coverage.data();

    const int width = radius+2;
    grid<uint8_t> img({width,width});
    auto out = img.data(); 
    *out++ = 255;
    for(int i=0; i<radius; ++i) *out++ = 255;
    *out++ = 0;
    for(int i=0; i<radius; ++i)
    {
        *out++ = 255;
        for(int i=0; i<radius; ++i) *out++ = static_cast<uint8_t>(*in++ * 255);
        *out++ = 0;
    }
    for(int i=0; i<width; ++i) *out++ = 0;

    return std::move(img);
}

canvas_sprites::canvas_sprites(sprite_sheet & sheet) : sheet{sheet}
{
    solid_pixel = sheet.add_sprite(grid<uint8_t>({3,3}, 0xFF), 1);
    for(int i=1; i<=32; ++i) corner_sprites[i] = sheet.add_sprite(make_bordered_circle_quadrant(i), 1);
    for(int i=1; i<=8; ++i)
    {
        grid<uint8_t> line_sprite({i+4,3}, 0);
        line_sprite.fill({2,0,i+2,3}, 0xFF);
        line_sprites[i] = sheet.add_sprite(std::move(line_sprite), 1);
    }
}

///////////////////////////
// canvas_device_objects //
///////////////////////////

canvas_device_objects::canvas_device_objects(rhi::device & device, shader_compiler & compiler, const sprite_sheet & sheet)
{
    // Convert sprite sheet from alpha only to srgb_alpha
    grid<byte4> pixels {sheet.sheet_image.dims(), {255,255,255,255}};
    for(int y=0; y<pixels.height(); ++y)
    {
        for(int x=0; x<pixels.width(); ++x)
        {
            pixels[{x,y}].w = sheet.sheet_image[{x,y}];
        }
    }

    sprites = device.create_image({rhi::image_shape::_2d, {pixels.dims(),1}, 1, rhi::image_format::rgba_srgb8, rhi::sampled_image_bit}, {pixels.data()});
    sampler = device.create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});
    per_window_layout = device.create_descriptor_set_layout({{0, rhi::descriptor_type::uniform_buffer, 1}});
    per_texture_layout = device.create_descriptor_set_layout({{0, rhi::descriptor_type::combined_image_sampler, 1}});
    pipe_layout = device.create_pipeline_layout({per_window_layout, per_texture_layout});
    const auto vs = device.create_shader(compiler.compile_file(rhi::shader_stage::vertex, "standard/gui/ui.vert"));
    const auto fs = device.create_shader(compiler.compile_file(rhi::shader_stage::fragment, "standard/gui/ui.frag")); 
    const rhi::blend_state translucent {true, true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

    rhi::pipeline_desc desc {};
    desc.layout = pipe_layout;
    desc.input = {gfx::vertex_binder<ui_vertex>(0)
        .attribute(0, &ui_vertex::position)
        .attribute(1, &ui_vertex::texcoord)
        .attribute(2, &ui_vertex::color)};
    desc.stages = {vs, fs};
    desc.topology = rhi::primitive_topology::triangles;
    desc.front_face = rhi::front_face::counter_clockwise;
    desc.cull_mode = rhi::cull_mode::back;
    desc.blend = {translucent};
    pipe = device.create_pipeline(desc);
}

////////////
// canvas //
////////////

canvas::canvas(const canvas_sprites & sprites, const canvas_device_objects & device_objects, gfx::transient_resource_pool & pool) : sprites{sprites}, device_objects{device_objects}, pool{pool}, vertex_count{0}
{
    pool.vertices.begin();
    pool.indices.begin();
}

void canvas::set_target(int layer, rect<int> scissor, rhi::image * texture)
{
    auto & set = sets[texture];
    if(!set)
    {
        set = pool.descriptors->alloc(*device_objects.per_texture_layout);
        set->write(0, *device_objects.sampler, texture ? *texture : *device_objects.sprites);
    }
    if(!lists.empty() && lists.back().index_count == 0) lists.pop_back();
    lists.push_back({layer, scissor, set, lists.empty() ? 0 : lists.back().first_index + lists.back().index_count, 0});
}

void canvas::draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color)
{
    auto it = sprites.line_sprites.find(width);
    if(it == end(sprites.line_sprites)) return;
    const auto & sprite = sprites.sheet.sprites[it->second];

    const float2 d = normalize(perp(p1-p0)) * (width*0.5f + 1);
    draw_convex_polygon({
        {p0+d, sprite.texcoords.corner00(), color},
        {p0-d, sprite.texcoords.corner10(), color},
        {p1-d, sprite.texcoords.corner11(), color},
        {p1+d, sprite.texcoords.corner01(), color}
    });
}

void canvas::draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color)
{
    auto it = sprites.line_sprites.find(width);
    if(it == end(sprites.line_sprites)) return;
    const auto & tc = sprites.sheet.sprites[it->second].texcoords;

    const float2 d0 = p1-p0, d1 = p2-p1, d2 = p3-p2;
    for(uint32_t i=0; i<=32; ++i)
    {
        const float t = (float)i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(perp(d0*(3*s*s) + d1*(6*s*t) + d2*(3*t*t))) * (width*0.5f + 1);
        pool.vertices.write(ui_vertex{p+d, tc.corner00(), color});
        pool.vertices.write(ui_vertex{p-d, tc.corner10(), color});
        if(i)
        {
            pool.indices.write(vertex_count + uint3(i*2-2, i*2-1, i*2+1));
            pool.indices.write(vertex_count + uint3(i*2-2, i*2+1, i*2+0));
        }
    }
    vertex_count += 33*2;
    lists.back().index_count += 32*6;
}

void canvas::draw_wire_rect(const rect<int> & r, int width, const float4 & color)
{
    auto rr = r;
    draw_rect(rr.take_x0(width), color);
    draw_rect(rr.take_y0(width), color);
    draw_rect(rr.take_x1(width), color);
    draw_rect(rr.take_y1(width), color);
}

void canvas::draw_rect(const rect<int> & r, const float4 & color)
{
    draw_sprite(r, color, sprites.sheet.sprites[sprites.solid_pixel].texcoords);
}

void canvas::draw_circle(const int2 & center, int radius, const float4 & color)
{ 
    draw_rounded_rect({center.x-radius, center.y-radius, center.x+radius, center.y+radius}, radius, color);
}

void canvas::draw_rounded_rect(const rect<int> & r, int radius, const float4 & color)
{
    draw_partial_rounded_rect(r, radius, 0xF, color);
}

void canvas::draw_partial_rounded_rect(const rect<int> & rr, int radius, corner_flags corners, const float4 & color)
{
    auto it = sprites.corner_sprites.find(radius);
    if(it == end(sprites.corner_sprites)) return;
    const auto & tc = sprites.sheet.sprites[it->second].texcoords;
    auto r = rr;
    if(corners & (top_left_corner | top_right_corner))
    {
        rect<int> r2 = r.take_y0(radius);
        if(corners & top_left_corner) draw_sprite(r2.take_x0(radius), color, tc.rotated_180());
        if(corners & top_right_corner) draw_sprite(r2.take_x1(radius), color, tc.mirrored_y());
        draw_rect(r2, color);
    }

    if(corners & (bottom_left_corner | bottom_right_corner))
    {
        rect<int> r2 = r.take_y1(radius);
        if(corners & bottom_left_corner) draw_sprite(r2.take_x0(radius), color, tc.mirrored_x());
        if(corners & bottom_right_corner) draw_sprite(r2.take_x1(radius), color, tc);
        draw_rect(r2, color);
    }

    draw_rect(r, color);
}

void canvas::draw_convex_polygon(array_view<ui_vertex> vertices)
{
    const uint32_t n = exactly(vertices.size());
    pool.vertices.write(vertices);
    for(uint32_t i=2; i<n; ++i) pool.indices.write(vertex_count + uint3{0,i-1,i});
    vertex_count += n;
    lists.back().index_count += (n-2)*3;
}

void canvas::draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords)
{
    draw_convex_polygon({
        {float2(r.corner00()), texcoords.corner00(), color}, 
        {float2(r.corner01()), texcoords.corner01(), color}, 
        {float2(r.corner11()), texcoords.corner11(), color}, 
        {float2(r.corner10()), texcoords.corner10(), color}
    });
}

void canvas::draw_sprite_sheet(const int2 & p)
{
    draw_sprite({p.x, p.y, p.x+sprites.sheet.sheet_image.width(), p.y+sprites.sheet.sheet_image.height()}, {1,1,1,1}, {0,0,1,1});
}

void canvas::draw_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint)
{
    auto it = font.glyphs.find(codepoint);
    if(it == font.glyphs.end()) return;
    auto & b = it->second;
    auto & s = font.sheet.sprites[b.sprite_index];
    const int2 b0 = pos + b.offset, b1 = b0 + s.img.dims();
    draw_sprite({b0.x+s.border, b0.y+s.border, b1.x-s.border, b1.y-s.border}, color, s.texcoords);
}

void canvas::draw_shadowed_glyph(const int2 & pos, const float4 & color, const font_face & font, uint32_t codepoint)
{
    draw_glyph(pos+1,{0,0,0,color.w},font,codepoint);
    draw_glyph(pos,color,font,codepoint);
}

void canvas::draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text)
{
    auto p = pos;
    for(auto str = text.data(); str != text.data()+text.size(); str = utf8::next(str))
    {
        auto it = font.glyphs.find(utf8::code(str));
        if(it == font.glyphs.end()) continue;
        auto & b = it->second;
        auto & s = font.sheet.sprites[b.sprite_index];
        const int2 b0 = p + b.offset, b1 = b0 + s.img.dims();
        draw_sprite({b0.x+s.border, b0.y+s.border, b1.x-s.border, b1.y-s.border}, color, s.texcoords);
        p.x += b.advance;
    }
}

void canvas::draw_shadowed_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text)
{
    draw_text(pos+1,{0,0,0,color.w},font,text);
    draw_text(pos,color,font,text);
}

void canvas::encode_commands(rhi::command_buffer & cmd, gfx::window & win)
{
    std::sort(lists.begin(), lists.end(), [](const list & a, const list & b) { return a.layer < b.layer; });

    constexpr coord_system ui_coords {coord_axis::right, coord_axis::down, coord_axis::forward};
    const coord_system ndc_coords = win.get_rhi_window().get_swapchain_framebuffer().get_ndc_coords();
    const int2 dims = win.get_window_size();   
    const auto ortho = float4x4{{2.0f/dims.x,0,0,0}, {0,2.0f/dims.y,0,0}, {0,0,1,0}, {-1,-1,0,1}};
    const auto transform = mul(get_transform_matrix(coord_transform{ui_coords, ndc_coords}), ortho);

    auto per_window_set = pool.descriptors->alloc(*device_objects.per_window_layout);
    per_window_set->write(0, pool.uniforms.upload(transform));

    cmd.set_viewport_rect(0, 0, dims.x, dims.y);
    cmd.bind_pipeline(*device_objects.pipe);
    cmd.bind_descriptor_set(*device_objects.pipe_layout, 0, *per_window_set);
    cmd.bind_vertex_buffer(0, pool.vertices.end());
    cmd.bind_index_buffer(pool.indices.end());
    for(auto & list : lists) 
    {
        cmd.set_scissor_rect(list.scissor.x0, list.scissor.y0, list.scissor.x1, list.scissor.y1);
        cmd.bind_descriptor_set(*device_objects.pipe_layout, 1, *list.set);
        cmd.draw_indexed(list.first_index, list.index_count);
    }
}

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

font_face::font_face(sprite_sheet & sheet, const std::vector<std::byte> & font_data, float pixel_height, uint32_t min_codepoint, uint32_t max_codepoint) : sheet{sheet}
{
    stbtt_fontinfo info {};
    if(!stbtt_InitFont(&info, reinterpret_cast<const uint8_t *>(font_data.data()), 0)) throw std::runtime_error("stbtt_InitFont(...) failed");
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);

    line_height = static_cast<int>(std::round((ascent - descent + line_gap) * scale));
    baseline = static_cast<int>(std::round(ascent * scale));

    for(uint32_t codepoint=min_codepoint; codepoint<=max_codepoint; ++codepoint)
    {
        const int g = stbtt_FindGlyphIndex(&info, codepoint);

        rect<int> bounds;
        stbtt_GetGlyphBitmapBox(&info, g, scale, scale, &bounds.x0, &bounds.y0, &bounds.x1, &bounds.y1);

        grid<uint8_t> img(bounds.dims());
        stbtt_MakeGlyphBitmap(&info, img.data(), img.width(), img.height(), img.width(), scale, scale, g);
        glyphs[codepoint].sprite_index = sheet.add_sprite(std::move(img), 0);

        int advance, lsb;
        stbtt_GetGlyphHMetrics(&info, g, &advance, &lsb);
        glyphs[codepoint].offset = bounds.corner00() + int2{0,baseline};
        glyphs[codepoint].advance = exactly(std::floor(advance * scale));
    }
}

static uint32_t decode_le(size_t size, const std::byte * data) { uint32_t value = 0; for(size_t i=0; i<size; ++i) value |= static_cast<uint8_t>(data[i]) << i*8; return value; }
static uint32_t decode_be(size_t size, const std::byte * data) { uint32_t value = 0; for(size_t i=0; i<size; ++i) value |= static_cast<uint8_t>(data[i]) << (size-1-i)*8; return value; }
template<class T> T read_le(FILE * f) { std::byte buffer[sizeof(T)]; fread(buffer, 1, sizeof(T), f); return static_cast<T>(decode_le(sizeof(T), buffer)); }
template<class T> T read_be(FILE * f) { std::byte buffer[sizeof(T)]; fread(buffer, 1, sizeof(T), f); return static_cast<T>(decode_be(sizeof(T), buffer)); }

struct pcf_glyph_info
{
    int width, height;
    int offset_x, offset_y;
    int advance_x;
    std::vector<uint8_t> bitmap;
};

struct pcf_font_info
{
    int baseline, line_height;
};

font_face::font_face(sprite_sheet & sheet, const char * pcf_path) : sheet{sheet}
{
    FILE * f = fopen(pcf_path, "rb");

    if(read_le<uint32_t>(f) != 0x70636601) throw std::runtime_error("not pcf");

    // Load table of contents, and sort in ascending order of type. This will ensure metrics always precedes bitmaps.
    struct toc_entry { int32_t type, format, size, offset; };
    std::vector<toc_entry> table_of_contents(read_le<int32_t>(f));
    for(auto & entry : table_of_contents)
    {
        entry.type = read_le<int32_t>(f);
        entry.format = read_le<int32_t>(f);
        entry.size = read_le<int32_t>(f);
        entry.offset = read_le<int32_t>(f);
    }
    std::sort(begin(table_of_contents), end(table_of_contents), [](const toc_entry & a, const toc_entry & b) { return a.type < b.type; });

    pcf_font_info font;
    std::vector<pcf_glyph_info> glyphs;
    std::unordered_map<int, int> glyph_indices;
    for(auto & entry : table_of_contents)
    {
        fseek(f, entry.offset, SEEK_SET);
        int32_t format = read_le<int32_t>(f);
        if(format != entry.format) throw std::runtime_error("malformed pcf - mismatched table format");

        auto read_uint32 = [=] { return format & 4 ? read_be<uint32_t>(f) : read_le<uint32_t>(f); };
        auto read_uint16 = [=] { return format & 4 ? read_be<uint16_t>(f) : read_le<uint16_t>(f); };
        auto read_uint8 = [=] { return format & 4 ? read_be<uint8_t>(f) : read_le<uint8_t>(f); };
        auto read_int32 = [=] { return format & 4 ? read_be<int32_t>(f) : read_le<int32_t>(f); };
        auto read_int16 = [=] { return format & 4 ? read_be<int16_t>(f) : read_le<int16_t>(f); };
        auto read_int8 = [=] { return format & 4 ? read_be<int8_t>(f) : read_le<int8_t>(f); };
        auto read_metrics = [=]
        {
            const int left_side_bearing = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int right_side_bearing = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_width = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_ascent = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_descent = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const unsigned character_attributes = format & 0x100 ? 0 : read_uint16();
            pcf_glyph_info info;
            info.width = right_side_bearing - left_side_bearing;
            info.height = character_ascent + character_descent;
            info.offset_x = left_side_bearing;
            info.offset_y = -character_ascent;
            info.advance_x = character_width;
            info.bitmap.resize(info.width * info.height);
            return info;
        };
        auto read_accelerators = [=]
        {
            const uint8_t no_overlap = read_uint8();
            const uint8_t constant_metrics = read_uint8();
            const uint8_t terminal_font = read_uint8();
            const uint8_t constant_width = read_uint8();
            const uint8_t ink_inside = read_uint8();
            const uint8_t ink_metrics = read_uint8();
            const uint8_t draw_direction = read_uint8();
            const uint8_t padding = read_uint8();
            const int font_ascent = read_int32();
            const int font_descent = read_int32();
            const int max_overlap = read_int32();
            const auto min_bounds = read_metrics();
            const auto max_bounds = read_metrics();
            const auto ink_min_bounds = format & 0x100 ? read_metrics() : min_bounds;
            const auto ink_max_bounds = format & 0x100 ? read_metrics() : max_bounds;
            return pcf_font_info{font_ascent, font_ascent + font_descent};
        };

        switch(entry.type)
        {
        case 1<<0: break; // properties
        case 1<<1: // accelerators
            font = read_accelerators();
            break;
        case 1<<2: // metrics
            glyphs.resize(entry.format & 0x100 ? read_int16() : read_int32());
            for(auto & g : glyphs) g = read_metrics();
            break;
        case 1<<3: // bitmaps
            {
                const int row_alignment = 1 << (entry.format & 3);
                const bool bitmap_big_endian_bytes = (entry.format & 4) != 0;
                const bool bitmap_big_endian_bits = (entry.format & 8) != 0;
                const int scan_unit_size = 1 << (entry.format>>4 & 3);

                std::vector<int> bitmap_offsets(read_int32());
                for(auto & offset : bitmap_offsets) offset = read_int32();
                const int32_t bitmap_sizes[4] {read_int32(), read_int32(), read_int32(), read_int32()};
                std::vector<std::byte> bitmap_data(bitmap_sizes[entry.format & 3]);
                fread(bitmap_data.data(), 1, bitmap_data.size(), f);

                for(size_t i=0; i<glyphs.size(); ++i)
                {    
                    const int row_width = (glyphs[i].width+row_alignment*8-1)/8/row_alignment*row_alignment;
                    for(int y=0; y<glyphs[i].height; ++y)
                    {
                        auto * p = bitmap_data.data() + bitmap_offsets[i] + row_width*y;
                        for(int x=0; x<glyphs[i].width;)
                        {
                            uint32_t bits = bitmap_big_endian_bytes ? decode_be(scan_unit_size, p) : decode_le(scan_unit_size, p);
                            for(int j=0; j<scan_unit_size*8 && x<glyphs[i].width; ++j, ++x)
                            {
                                if(bits & 1<<(bitmap_big_endian_bits ? scan_unit_size*8-1-j : j)) glyphs[i].bitmap[y*glyphs[i].width+x] = 0xFF;
                            }
                            p += scan_unit_size;
                        }
                    }            
                }
            } 
            break;
        case 1<<4: // ink metrics
            glyphs.resize(entry.format & 0x100 ? read_int16() : read_int32());
            for(auto & g : glyphs)
            {
                auto ink = read_metrics();
                const int skip_x = ink.offset_x - g.offset_x, skip_y = ink.offset_y - g.offset_y;
                for(int y=0; y<ink.height; ++y) std::copy_n(g.bitmap.data() + (skip_y+y)*g.width + skip_x, ink.width, ink.bitmap.data() + y*ink.width);
                g = std::move(ink);
            }
            break;
        case 1<<5: // bdf encodings
            {
                const int min_byte2 = read_int16(), max_byte2 = read_int16();
                const int min_byte1 = read_int16(), max_byte1 = read_int16();
                const int default_character = read_int16();
                for(int byte1 = min_byte1; byte1 <= max_byte1; ++byte1)
                {
                    for(int byte2 = min_byte2; byte2 <= max_byte2; ++byte2)
                    {
                        const int glyph_index = read_uint16();
                        if(glyph_index != 0xFFFF) glyph_indices[byte1 << 8 | byte2] = glyph_index;
                    }
                }
            } 
            break;
        case 1<<6: break; // swidths
        case 1<<7: break; // glyph_names
        case 1<<8: // bdf_accelerators
            font = read_accelerators();
            break;
        }
    }
    int space_advance = glyphs[glyph_indices[' ']].advance_x;
    for(auto & g : glyphs) 
    {
        g.offset_x = 0;
        g.offset_y += font.baseline;
        g.advance_x = g.width+1;
    }
    glyphs[glyph_indices[' ']].advance_x = space_advance-1;

    for(auto & gi : glyph_indices)
    {
        const auto & g = glyphs[gi.second];
        grid<uint8_t> img(int2(g.width, g.height));
        for(int y=0; y<g.height; ++y)
        {
            for(int x=0; x<g.width; ++x)
            {
                img[{x,y}] = g.bitmap[y*g.width+x];
            }
        }
        this->glyphs[gi.first].sprite_index = sheet.add_sprite(img, 0);
        this->glyphs[gi.first].offset = int2{g.offset_x, g.offset_y};
        this->glyphs[gi.first].advance = g.advance_x;
    }
    this->baseline = font.baseline;
    this->line_height = font.line_height;
}

int font_face::get_text_width(std::string_view text) const
{         
    int width = 0;
    for(auto s = text.data(); s != text.data()+text.size(); s = utf8::next(s))
    {
        auto it = glyphs.find(utf8::code(s));
        if(it == glyphs.end()) continue;
        width += it->second.advance;
    }
    return width;
}

int font_face::get_cursor_pos(std::string_view text, int x) const
{
    for(auto s = text.data(); s != text.data()+text.size(); s = utf8::next(s))
    {
        auto it = glyphs.find(utf8::code(s));
        if(it == end(glyphs)) continue;  
        if(x*2 < it->second.advance) return exactly(s - text.data());
        x -= it->second.advance;
    }
    return exactly(text.end() - text.begin());
}