#include "sprite.h"

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
        grid<uint8_t> line_sprite({i+2,1}, 0xFF);
        line_sprite[{0,0}] = line_sprite[{i+1,0}] = 0;
        line_sprites[i] = sheet.add_sprite(std::move(line_sprite), 0);
    }
}

////////////
// canvas //
////////////

canvas::canvas(const canvas_sprites & sprites, gfx::transient_resource_pool & pool, const int2 & dims) : sprites{sprites}, pool{pool}, dims{dims}, vertex_count{0}
{
    pool.vertices.begin();
    pool.indices.begin();
    scissors.push_back({0,0,dims.x,dims.y});
    lists.push_back({scissors.back(), 0, 0, 0});
}

void canvas::begin_overlay()
{
    scissors.push_back(scissors.front());
    lists.push_back({scissors.back(), lists.back().level+1, lists.back().last, lists.back().last});
}

void canvas::end_overlay()
{
    scissors.pop_back();
    lists.push_back({scissors.back(), lists.back().level-1, lists.back().last, lists.back().last});
}

void canvas::begin_scissor(const rect<int> & r)
{
    const auto & s = scissors.back();
    scissors.push_back({std::max(s.x0, r.x0), std::max(s.y0, r.y0), std::min(s.x1, r.x1), std::min(s.y1, r.y1)});
    lists.push_back({scissors.back(), lists.back().level, lists.back().last, lists.back().last});
}

void canvas::end_scissor()
{
    scissors.pop_back();
    lists.push_back({scissors.back(), lists.back().level, lists.back().last, lists.back().last});
}

void canvas::draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color)
{
    auto it = sprites.line_sprites.find(width);
    if(it == end(sprites.line_sprites)) return;
    const auto & sprite = sprites.sheet.sprites[it->second];

    const float2 perp = normalize(cross(float3(p1-p0,0), float3(0,0,1)).xy()) * (width*0.5f + 1);
    draw_convex_polygon({
        {p0+perp, {sprite.texcoords.x0, (sprite.texcoords.y0+sprite.texcoords.y1)/2}, color},
        {p0-perp, {sprite.texcoords.x1, (sprite.texcoords.y0+sprite.texcoords.y1)/2}, color},
        {p1-perp, {sprite.texcoords.x1, (sprite.texcoords.y0+sprite.texcoords.y1)/2}, color},
        {p1+perp, {sprite.texcoords.x0, (sprite.texcoords.y0+sprite.texcoords.y1)/2}, color}
    });
}

void canvas::draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color)
{
    auto it = sprites.line_sprites.find(width);
    if(it == end(sprites.line_sprites)) return;
    const auto & tc = sprites.sheet.sprites[it->second].texcoords;

    const float2 d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;
    float2 v0, v1;
    for(uint32_t i=0; i<=32; ++i)
    {
        const float t = (float)i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t)) * (width*0.5f + 1);
        pool.vertices.write(ui_vertex{{p.x-d.y, p.y+d.x}, {tc.x0, (tc.y0+tc.y1)/2}, color});
        pool.vertices.write(ui_vertex{{p.x+d.y, p.y-d.x}, {tc.x1, (tc.y0+tc.y1)/2}, color});
        if(i)
        {
            pool.indices.write(vertex_count + uint3(i*2-2, i*2-1, i*2+1));
            pool.indices.write(vertex_count + uint3(i*2-2, i*2+1, i*2+0));
        }
    }
    vertex_count += 33*2;
    lists.back().last += 32*6;
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
        if(corners & top_left_corner) draw_sprite(r2.take_x0(radius), color, {tc.x1, tc.y1, tc.x0, tc.y0});
        if(corners & top_right_corner) draw_sprite(r2.take_x1(radius), color, {tc.x0, tc.y1, tc.x1, tc.y0});
        draw_rect(r2, color);
    }

    if(corners & (bottom_left_corner | bottom_right_corner))
    {
        rect<int> r2 = r.take_y1(radius);
        if(corners & bottom_left_corner) draw_sprite(r2.take_x0(radius), color, {tc.x1, tc.y0, tc.x0, tc.y1});
        if(corners & bottom_right_corner) draw_sprite(r2.take_x1(radius), color, {tc.x0, tc.y0, tc.x1, tc.y1});
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
    lists.back().last += (n-2)*3;
}

void canvas::draw_sprite(const rect<int> & r, const float4 & color, const rect<float> & texcoords)
{
    draw_convex_polygon({
        {{static_cast<float>(r.x0),static_cast<float>(r.y0)}, {texcoords.x0,texcoords.y0}, color}, 
        {{static_cast<float>(r.x0),static_cast<float>(r.y1)}, {texcoords.x0,texcoords.y1}, color}, 
        {{static_cast<float>(r.x1),static_cast<float>(r.y1)}, {texcoords.x1,texcoords.y1}, color}, 
        {{static_cast<float>(r.x1),static_cast<float>(r.y0)}, {texcoords.x1,texcoords.y0}, color}
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

void canvas::draw_text(const int2 & pos, const float4 & color, const font_face & font, std::string_view text)
{
    auto p = pos;
    for(auto ch : text)
    {
        auto it = font.glyphs.find(ch);
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

void canvas::encode_commands(rhi::command_buffer & cmd)
{
    std::sort(lists.begin(), lists.end(), [](const list & a, const list & b) { return a.level < b.level; });
    cmd.bind_vertex_buffer(0, pool.vertices.end());
    cmd.bind_index_buffer(pool.indices.end());
    for(auto & list : lists) 
    {
        cmd.set_scissor_rect(list.scissor.x0, list.scissor.y0, list.scissor.x1, list.scissor.y1);
        cmd.draw_indexed(list.first, list.last - list.first);
    }
}

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

font_face::font_face(sprite_sheet & sheet, const std::vector<std::byte> & font_data, float pixel_height) : sheet{sheet}
{
    stbtt_fontinfo info {};
    if(!stbtt_InitFont(&info, reinterpret_cast<const uint8_t *>(font_data.data()), 0)) throw std::runtime_error("stbtt_InitFont(...) failed");
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);

    line_height = static_cast<int>(std::round((ascent - descent + line_gap) * scale));
    baseline = static_cast<int>(std::round(ascent * scale));

    for(int ch=0; ch<128; ++ch)
    {
        if(!isprint(ch)) continue;
        const int g = stbtt_FindGlyphIndex(&info, ch);
        int advance, lsb, x0, y0, x1, y1;
        stbtt_GetGlyphHMetrics(&info, g, &advance, &lsb);
        stbtt_GetGlyphBitmapBox(&info, g, scale, scale, &x0, &y0, &x1, &y1);

        grid<uint8_t> img({x1-x0, y1-y0});
        stbtt_MakeGlyphBitmap(&info, img.data(), img.width(), img.height(), img.width(), scale, scale, g);
        glyphs[ch].sprite_index = sheet.add_sprite(std::move(img), 0);
        glyphs[ch].offset = {x0,y0 + baseline};
        glyphs[ch].advance = static_cast<int>(std::floor(advance * scale));
    }
}

int font_face::get_text_width(std::string_view text) const
{
    int width = 0;
    for(auto codepoint : text)
    {
        auto g = glyphs.find(codepoint);
        if(g == end(glyphs)) continue;
        width += g->second.advance;
    }
    return width;
}