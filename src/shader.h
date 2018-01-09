#pragma once
#include <vector>

enum class shader_stage { vertex, fragment };

struct shader_module
{
    shader_stage stage;
    std::vector<uint32_t> spirv;
};

struct shader_compiler
{
    shader_compiler();
    ~shader_compiler();

    shader_module compile(shader_stage stage, const char * glsl);
};