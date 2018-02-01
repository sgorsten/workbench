#pragma once
#include "rhi.h" // For rhi::image_format

enum class file_mode { binary, text };
class file
{
    std::string path;
    FILE * f;
    size_t length;
public:
    file(std::string_view path, file_mode mode);
    file(file && r) : path(move(r.path)), f{r.f}, length{r.length} { r.f = nullptr; }
    file(const file & r) = delete;
    file & operator = (file && r) { std::swap(path, r.path); std::swap(f, r.f); std::swap(length, r.length); return *this; }
    file & operator = (const file & r) = delete;
    ~file();

    explicit operator bool () const;
    const std::string & get_path() const { return path; }
    size_t get_length() const { return length; }
    bool eof() const;
    size_t read(void * buffer, size_t size);
    void seek(int64_t offset);
};

struct image 
{ 
    int2 dimensions; rhi::image_format format; std::shared_ptr<void> pixels;
    const uint8_t * get_pixels() const { return static_cast<uint8_t *>(pixels.get()); }
    uint8_t * get_pixels() { return static_cast<uint8_t *>(pixels.get()); }
    static image allocate(int2 dimensions, rhi::image_format format)
    {
        auto memory = std::malloc(product(dimensions) * get_pixel_size(format));
        if(!memory) throw std::bad_alloc();
        return {dimensions, format, std::shared_ptr<void>(memory, std::free)};
    }
};
class loader
{
    std::vector<std::string> roots;
public:
    void register_root(std::string_view root) { roots.push_back(to_string(root, '/')); }

    file open_file(std::string_view filename, file_mode mode) const;
    std::vector<std::byte> load_binary_file(std::string_view filename) const;
    std::vector<char> load_text_file(std::string_view filename) const;

    image load_image(std::string_view filename, bool linear);
};

std::string get_program_binary_path();
