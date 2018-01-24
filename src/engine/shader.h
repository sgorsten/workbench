#pragma once
#include "rhi.h"

class loader;
struct shader_compiler_impl;
struct shader_compiler
{
    std::unique_ptr<shader_compiler_impl> impl;

    shader_compiler(loader & loader);
    ~shader_compiler();

    rhi::shader_desc compile_file(rhi::shader_stage stage, const std::string & filename);
};
