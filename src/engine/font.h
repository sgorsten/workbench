#pragma once
#include "grid.h"

struct pcf_glyph_info
{
    grid<uint8_t> bitmap;
    int2 offset;
    int advance;
};

struct pcf_font_info
{
    std::unordered_map<unsigned, pcf_glyph_info> glyphs; // codepoint -> glyph
    int baseline, line_height;
};
