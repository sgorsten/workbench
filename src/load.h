#pragma once
#include "core.h"
#include "rhi.h"
#include <string_view>
#include <optional>

std::string get_program_binary_path();

class loader
{
    std::vector<std::string> roots;
public:
    std::optional<std::string> find_filepath(std::string_view filename) const;
    std::string find_filepath_or_throw(std::string_view filename) const;

    void register_root(std::string_view root) { roots.push_back(to_string(root, '/')); }
    
    std::vector<char> load_text_file(std::string_view filename);
};

struct image { int2 dimensions; rhi::image_format format; void * pixels; };
image load_image(loader & loader, std::string_view filename);