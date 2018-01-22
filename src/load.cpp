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
size_t file::read(char * buffer, size_t size) { return f ? fread(buffer, 1, size, f) : 0; }
void file::seek(int64_t offset) { if(f) fseek(f, exactly(offset), SEEK_CUR); }

file loader::open_file(std::string_view filename, file_mode mode)
{
    for(auto & root : roots)
    {
        file f {to_string(root, filename), mode};
        if(f) return f;
    }
    throw std::runtime_error(to_string("failed to find file \"", filename, '"'));
}

std::vector<char> loader::load_text_file(std::string_view filename)
{
    auto f = open_file(filename, file_mode::text);
    std::vector<char> buffer(f.get_length());
    buffer.resize(f.read(buffer.data(), buffer.size()));
    return buffer;
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
image loader::load_image(std::string_view filename)
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
        if(pixelsf) return {{width,height}, rhi::image_format::rgba_float32, pixelsf};
    }
    else
    {   
        stbi__result_info ri;
        auto pixels = stbi__load_main(&context, &width, &height, nullptr, 4, &ri, 0);
        if(pixels && ri.bits_per_channel==8) return {{width,height}, rhi::image_format::rgba_unorm8, pixels};
        if(pixels && ri.bits_per_channel==16) return {{width,height}, rhi::image_format::rgba_unorm16, pixels};
    }
    throw std::runtime_error(to_string("unknown image format for \"", f.get_path(), '"'));
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