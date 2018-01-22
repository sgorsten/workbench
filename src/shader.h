#pragma once
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include "core.h"

enum class shader_stage { vertex, fragment };
enum class sampler_dim { _1d, _2d, _3d, cube, rect, buffer, subpass_data };

template<class T> class indirect
{
    std::unique_ptr<T> value;
public:
    indirect() : value{std::make_unique<T>()} {}
    indirect(const T & r) : value{std::make_unique<T>(r)} {}
    indirect(const indirect & r) : value{std::make_unique<T>(*r.value)} {}

    indirect & operator = (const T & r) { value = std::make_unique<T>(r); return *this; }
    indirect & operator = (const indirect & r) { value = std::make_unique<T>(*r.value); return *this; }

    T & operator * () { return *value; }
    T * operator -> () { return value.get(); }
    const T & operator * () const { return *value; }
    const T * operator -> () const { return value.get(); }
};

// Reflection information for a single shader
struct shader_module
{
    enum scalar_type { uint_, int_, float_, double_ };
    struct type;
    struct matrix_layout { uint32_t stride; bool row_major; };
    struct structure_member { std::string name; indirect<type> type; std::optional<uint32_t> offset; };   
    struct sampler { scalar_type channel; sampler_dim dim; bool arrayed, multisampled, shadow; };
    struct numeric { scalar_type scalar; uint32_t row_count, column_count; std::optional<matrix_layout> matrix_layout; };
    struct array { indirect<type> element; uint32_t length; std::optional<uint32_t> stride; };
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

class loader;
struct shader_compiler_impl;
struct shader_compiler
{
    std::unique_ptr<shader_compiler_impl> impl;

    shader_compiler(loader & loader);
    ~shader_compiler();

    shader_module compile(shader_stage stage, const std::string & glsl);
    shader_module compile_file(shader_stage stage, const std::string & filename);
};

std::ostream & operator << (std::ostream & out, const shader_module::scalar_type & s);
std::ostream & operator << (std::ostream & out, const shader_module::sampler & s);
std::ostream & operator << (std::ostream & out, const shader_module::numeric & n);
std::ostream & operator << (std::ostream & out, const shader_module::array & a);
std::ostream & operator << (std::ostream & out, const shader_module::structure & s);
std::ostream & operator << (std::ostream & out, const shader_module::type & t);