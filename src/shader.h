#pragma once
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>

enum class shader_stage { vertex, fragment };
enum class sampler_dim { _1d, _2d, _3d, cube, rect, buffer, subpass_data };

// Reflection information for a single shader
struct shader_module
{
    enum scalar_type { uint_, int_, float_, double_ };
    struct type;
    struct matrix_layout { uint32_t stride; bool row_major; };
    struct structure_member { std::string name; std::unique_ptr<const type> type; std::optional<uint32_t> offset; };   
    struct sampler { scalar_type channel; sampler_dim dim; bool arrayed, multisampled, shadow; };
    struct numeric { scalar_type scalar; uint32_t row_count, column_count; std::optional<matrix_layout> matrix_layout; };
    struct array { std::unique_ptr<const type> element; uint32_t length; std::optional<uint32_t> stride; };
    struct structure { std::string name; std::vector<structure_member> members; };
    struct type { std::variant<sampler, numeric, array, structure> contents; };
    struct interface { uint32_t location; std::string name; type type; };
    struct descriptor { uint32_t set, binding; std::string name; type type; };

    std::vector<uint32_t> spirv;
    // Note: For now, only one entry point supported
    shader_stage stage;
    std::string name;
    std::vector<descriptor> descriptors;
    std::vector<interface> inputs, outputs;
};

struct shader_compiler
{
    shader_compiler();
    ~shader_compiler();

    shader_module compile(shader_stage stage, const char * glsl);
};

std::ostream & operator << (std::ostream & out, const shader_module::scalar_type & s);
std::ostream & operator << (std::ostream & out, const shader_module::sampler & s);
std::ostream & operator << (std::ostream & out, const shader_module::numeric & n);
std::ostream & operator << (std::ostream & out, const shader_module::array & a);
std::ostream & operator << (std::ostream & out, const shader_module::structure & s);
std::ostream & operator << (std::ostream & out, const shader_module::type & t);