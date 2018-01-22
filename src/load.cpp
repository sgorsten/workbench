// All filesystem access is controlled through this module
#include "load.h"

std::optional<std::string> loader::find_filepath(std::string_view filename) const
{
    for(auto & root : roots)
    {
        std::string filepath = to_string(root, filename);
        FILE * f = fopen(filepath.c_str(), "r");
        if(f)
        {
            fclose(f);
            return filepath;
        }
    }
    return std::nullopt;
}

std::string loader::find_filepath_or_throw(std::string_view filename) const
{
    auto path = find_filepath(filename);
    if(!path) throw std::runtime_error(to_string("failed to find file \"", filename, '"'));
    return *path;
}

std::vector<char> loader::load_text_file(std::string_view filename)
{
    auto path = find_filepath_or_throw(filename);
    FILE * f = fopen(path.c_str(), "r");
    if(!f) throw std::runtime_error(to_string("failed to open file \"", path, '"'));
    FINALLY( fclose(f); )

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buffer(len);
    buffer.resize(fread(buffer.data(), 1, buffer.size(), f));
    return buffer;
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
image load_image(loader & loader, std::string_view filename)
{
    auto path = loader.find_filepath_or_throw(filename);
    FILE * f = fopen(path.c_str(), "rb");
    if(!f) throw std::runtime_error(to_string("failed to open file \"", path, '"'));
    FINALLY( fclose(f); )

    stbi__context s;
    stbi__start_file(&s,f);
    int width, height;
    if(stbi__hdr_test(&s))
    {
        auto pixelsf = stbi__loadf_main(&s, &width, &height, nullptr, 4);
        if(pixelsf) return {{width,height}, rhi::image_format::rgba_float32, pixelsf};
    }
    else
    {   
        stbi__result_info ri;
        auto pixels = stbi__load_main(&s, &width, &height, nullptr, 4, &ri, 0);
        if(pixels && ri.bits_per_channel==8) return {{width,height}, rhi::image_format::rgba_unorm8, pixels};
        if(pixels && ri.bits_per_channel==16) return {{width,height}, rhi::image_format::rgba_unorm16, pixels};
    }
    throw std::runtime_error(to_string("unknown image format for \"", path, '"'));
}

#ifdef _WIN32
#include <Windows.h>
std::string get_program_binary_path()
{
    char buffer[1024];
    const auto length = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    std::string path {buffer, buffer+length};
    while(true)
    {
        if(path.empty()) throw std::runtime_error("unable to determine program binary path");
        if(path.back() == '\\') return path;
        path.pop_back();
    }
}
#endif