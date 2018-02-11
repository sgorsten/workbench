// All filesystem access is controlled through this module
#include "load.h"

FILE * fopen_utf8(std::string_view path, file_mode mode);

file::file(std::string_view path, file_mode mode) : path{path}, f{fopen_utf8(path,mode)}, length{0}
{
    if(!f) return;
    fseek(f, 0, SEEK_END);
    length = exactly(ftell(f));
    fseek(f, 0, SEEK_SET);
}
file::~file() { if(f) fclose(f); }
file::operator bool () const { return f != nullptr; }
bool file::eof() const { return f ? feof(f) : 1; }
size_t file::read(void * buffer, size_t size) { return f ? fread(buffer, 1, size, f) : 0; }
void file::seek_set(int64_t position)  { if(f) fseek(f, exactly(position), SEEK_SET); }
void file::seek(int64_t offset) { if(f) fseek(f, exactly(offset), SEEK_CUR); }

file loader::open_file(std::string_view filename, file_mode mode) const
{
    for(auto & root : roots)
    {
        file f {to_string(root, filename), mode};
        if(f) return f;
    }
    throw std::runtime_error(to_string("failed to find file \"", filename, '"'));
}

std::vector<std::byte> loader::load_binary_file(std::string_view filename) const
{
    auto f = open_file(filename, file_mode::binary);
    std::vector<std::byte> buffer(f.get_length());
    buffer.resize(f.read(buffer.data(), buffer.size()));
    return buffer;
}

std::vector<char> loader::load_text_file(std::string_view filename) const
{
    auto f = open_file(filename, file_mode::text);
    std::vector<char> buffer(f.get_length());
    buffer.resize(f.read(buffer.data(), buffer.size()));
    return buffer;
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
image loader::load_image(std::string_view filename, bool linear)
{
    auto f = open_file(filename, file_mode::binary);
    
    stbi__context context;
    stbi_io_callbacks callbacks =
    {
        [](void * user, char * data, int size) { return exact_cast<int>(reinterpret_cast<file *>(user)->read(data, exactly(size))); },
        [](void * user, int n) { return reinterpret_cast<file *>(user)->seek(exactly(n)); },
        [](void * user) { return reinterpret_cast<file *>(user)->eof() ? 1 : 0; }
    };
    stbi__start_callbacks(&context, &callbacks, &f);
    int width, height;
    if(stbi__hdr_test(&context))
    {
        auto pixelsf = stbi__loadf_main(&context, &width, &height, nullptr, 4);
        if(pixelsf) return {{width,height}, rhi::image_format::rgba_float32, std::shared_ptr<void>(pixelsf,stbi_image_free)};
    }
    else
    {   
        stbi__result_info ri;
        auto pixels = stbi__load_main(&context, &width, &height, nullptr, 4, &ri, 0);
        if(pixels && ri.bits_per_channel==8) return {{width,height}, linear ? rhi::image_format::rgba_unorm8 : rhi::image_format::rgba_srgb8, std::shared_ptr<void>(pixels,stbi_image_free)};
        if(pixels && ri.bits_per_channel==16) return {{width,height}, rhi::image_format::rgba_unorm16, std::shared_ptr<void>(pixels,stbi_image_free)};
    }
    throw std::runtime_error(to_string("unknown image format for \"", f.get_path(), '"'));
}

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

pcf_font_info loader::load_ttf_font(std::string_view filename, float pixel_height, uint32_t min_codepoint, uint32_t max_codepoint)
{
    auto font_data = load_binary_file(filename);
    stbtt_fontinfo info {};
    if(!stbtt_InitFont(&info, reinterpret_cast<const uint8_t *>(font_data.data()), 0)) throw std::runtime_error("stbtt_InitFont(...) failed");
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);

    pcf_font_info font;
    font.baseline = static_cast<int>(std::round(ascent * scale));
    font.line_height = static_cast<int>(std::round((ascent - descent + line_gap) * scale));    

    for(uint32_t codepoint=min_codepoint; codepoint<=max_codepoint; ++codepoint)
    {
        const int g = stbtt_FindGlyphIndex(&info, codepoint);

        rect<int> bounds;
        stbtt_GetGlyphBitmapBox(&info, g, scale, scale, &bounds.x0, &bounds.y0, &bounds.x1, &bounds.y1);

        grid<uint8_t> img(bounds.dims());
        stbtt_MakeGlyphBitmap(&info, img.data(), img.width(), img.height(), img.width(), scale, scale, g);
        font.glyphs[codepoint].bitmap = std::move(img);

        int advance, lsb;
        stbtt_GetGlyphHMetrics(&info, g, &advance, &lsb);
        font.glyphs[codepoint].offset = bounds.corner00() + int2{0,font.baseline};
        font.glyphs[codepoint].advance = exactly(std::floor(advance * scale));
    }
    return font;
}

static uint32_t decode_le(size_t size, const std::byte * data) { uint32_t value = 0; for(size_t i=0; i<size; ++i) value |= static_cast<uint8_t>(data[i]) << i*8; return value; }
static uint32_t decode_be(size_t size, const std::byte * data) { uint32_t value = 0; for(size_t i=0; i<size; ++i) value |= static_cast<uint8_t>(data[i]) << (size-1-i)*8; return value; }
template<class T> T read_le(file & f) { std::byte buffer[sizeof(T)]; f.read(buffer, sizeof(T)); return static_cast<T>(decode_le(sizeof(T), buffer)); }
template<class T> T read_be(file & f) { std::byte buffer[sizeof(T)]; f.read(buffer, sizeof(T)); return static_cast<T>(decode_be(sizeof(T), buffer)); }

pcf_font_info loader::load_pcf_font(std::string_view filename, bool condense)
{
    auto f = open_file(filename, file_mode::binary);

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
        f.seek_set(entry.offset);
        int32_t format = read_le<int32_t>(f);
        if(format != entry.format) throw std::runtime_error("malformed pcf - mismatched table format");

        auto read_uint32 = [&] { return format & 4 ? read_be<uint32_t>(f) : read_le<uint32_t>(f); };
        auto read_uint16 = [&] { return format & 4 ? read_be<uint16_t>(f) : read_le<uint16_t>(f); };
        auto read_uint8 = [&] { return format & 4 ? read_be<uint8_t>(f) : read_le<uint8_t>(f); };
        auto read_int32 = [&] { return format & 4 ? read_be<int32_t>(f) : read_le<int32_t>(f); };
        auto read_int16 = [&] { return format & 4 ? read_be<int16_t>(f) : read_le<int16_t>(f); };
        auto read_int8 = [&] { return format & 4 ? read_be<int8_t>(f) : read_le<int8_t>(f); };
        auto read_metrics = [=]
        {
            const int left_side_bearing = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int right_side_bearing = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_width = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_ascent = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const int character_descent = format & 0x100 ? read_uint8() - 0x80 : read_int16();
            const unsigned character_attributes = format & 0x100 ? 0 : read_uint16();
            pcf_glyph_info info;
            info.bitmap.resize({right_side_bearing - left_side_bearing, character_ascent + character_descent}, 0);
            info.offset = {left_side_bearing, -character_ascent};
            info.advance = character_width;
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
            return pcf_font_info{{}, font_ascent, font_ascent + font_descent};
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
                f.read(bitmap_data.data(), bitmap_data.size());

                for(size_t i=0; i<glyphs.size(); ++i)
                {    
                    const int row_width = (glyphs[i].bitmap.width()+row_alignment*8-1)/8/row_alignment*row_alignment;
                    for(int y=0; y<glyphs[i].bitmap.height(); ++y)
                    {
                        auto * p = bitmap_data.data() + bitmap_offsets[i] + row_width*y;
                        for(int x=0; x<glyphs[i].bitmap.width();)
                        {
                            uint32_t bits = bitmap_big_endian_bytes ? decode_be(scan_unit_size, p) : decode_le(scan_unit_size, p);
                            for(int j=0; j<scan_unit_size*8 && x<glyphs[i].bitmap.width(); ++j, ++x)
                            {
                                if(bits & 1<<(bitmap_big_endian_bits ? scan_unit_size*8-1-j : j)) glyphs[i].bitmap[{x,y}] = 0xFF;
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
                const int2 skip = ink.offset - g.offset;
                ink.bitmap.blit({0,0}, g.bitmap.subrect({skip, skip+ink.bitmap.dims()}));
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
    for(auto & g : glyphs) g.offset.y += font.baseline;
    if(condense)
    {
        int space_advance = glyphs[glyph_indices[' ']].advance;
        for(auto & g : glyphs) 
        {
            g.offset.x = 0;
            g.advance = g.bitmap.width()+1;
        }
        glyphs[glyph_indices[' ']].advance = space_advance-1;
    }

    for(auto & gi : glyph_indices) font.glyphs[gi.first] = glyphs[gi.second];
    return font;
}

#ifdef _WIN32
#include <Windows.h>

std::string win_to_utf8(std::wstring_view s)
{
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), exactly(s.size()), nullptr, 0, nullptr, nullptr);
    if(length == 0 && s.size() > 0) throw std::runtime_error("invalid utf-16");
    std::string result(exact_cast<size_t>(length), 0);
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), exactly(s.size()), &result[0], length, nullptr, nullptr);
    return result;
}

std::wstring utf8_to_win(std::string_view s)
{
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), exactly(s.size()), nullptr, 0);
    if(length == 0 && s.size() > 0) throw std::runtime_error("invalid utf-8");
    std::wstring result(exact_cast<size_t>(length), 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), exactly(s.size()), &result[0], length);
    return result;
}

std::string get_program_binary_path()
{
    wchar_t buffer[1024];
    const auto length = GetModuleFileNameW(nullptr, buffer, sizeof(buffer));
    auto path = win_to_utf8({buffer, length});
    while(true)
    {
        if(path.empty()) throw std::runtime_error("unable to determine program binary path");
        if(path.back() == '\\') return path;
        path.pop_back();
    }
}

FILE * fopen_utf8(std::string_view path, file_mode mode)
{
    auto buf = utf8_to_win(path);
    switch(mode)
    {
    case file_mode::binary: return _wfopen(buf.c_str(), L"rb");
    case file_mode::text: return _wfopen(buf.c_str(), L"r");
    default: fail_fast();
    }
}
#endif