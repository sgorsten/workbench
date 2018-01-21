#include "shader.h"
#include <map>
#include "../dep/glslang/glslang/Public/ShaderLang.h"
#include "../dep/glslang/SPIRV/GlslangToSpv.h"
#include "../dep/glslang/StandAlone/ResourceLimits.h"
#include "../dep/glslang/SPIRV/spirv.hpp"

std::vector<char> load_text_file(const char * filename)
{
    FILE * f = fopen(filename, "r");
    if(!f) throw std::runtime_error(std::string("failed to open ") + filename);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buffer(len);
    buffer.resize(fread(buffer.data(), 1, buffer.size(), f));
    return buffer;
}

std::ostream & operator << (std::ostream & out, const shader_module::scalar_type & s)
{
    switch(s)
    {
    case shader_module::uint_: return out << "uint";
    case shader_module::int_: return out << "int";
    case shader_module::float_: return out << "float";
    case shader_module::double_: return out << "double";
    default: throw std::logic_error("invalid shader_module::scalar_type");
    }
}
std::ostream & operator << (std::ostream & out, const shader_module::sampler & s) 
{ 
    out << "sampler";
    switch(s.dim)
    {
    case sampler_dim::_1d: out << "1d"; break;
    case sampler_dim::_2d: out << "2d"; break;
    case sampler_dim::_3d: out << "3d"; break;
    case sampler_dim::cube: out << "_cube"; break;
    case sampler_dim::rect: out << "_rect"; break;
    case sampler_dim::buffer: out << "_buffer"; break;
    case sampler_dim::subpass_data: out << "_subpass_data"; break;
    default: throw std::logic_error("invalid sampler_dim");
    }
    out << "<" << s.channel;
    if(s.arrayed) out << ",arrayed";
    if(s.multisampled) out << ",multisampled";
    if(s.shadow) out << ",shadow";
    return out << ">";
}
std::ostream & operator << (std::ostream & out, const shader_module::numeric & n) 
{ 
    out << n.scalar;
    if(n.row_count > 1) out << n.row_count;
    if(n.column_count > 1) out << 'x' << n.column_count;
    return out;
}
std::ostream & operator << (std::ostream & out, const shader_module::array & a) { return out << *a.element << '[' << a.length << ']'; }
std::ostream & operator << (std::ostream & out, const shader_module::structure & s) 
{ 
    out << "struct " << s.name << " {";
    for(auto & mem : s.members)
    {
        out << "\n  " << mem.name << " : " << *mem.type;
    }
    return out << "\n}"; 
}
std::ostream & operator << (std::ostream & out, const shader_module::type & t) { return std::visit([&out](const auto & val) -> std::ostream & { return out << val; }, t.contents); }

struct spirv_parser
{
    struct type { spv::Op op; std::vector<uint32_t> contents; };
    struct variable { uint32_t type; spv::StorageClass storage_class; };
    struct constant { uint32_t type; std::vector<uint32_t> literals; };
    struct entrypoint { spv::ExecutionModel execution_model; std::string name; std::vector<uint32_t> interfaces; };
    struct metadata
    {
        std::string name;
        std::map<spv::Decoration, std::vector<uint32_t>> decorations;
        std::map<uint32_t, metadata> members;
        bool has_decoration(spv::Decoration decoration) const { return decorations.find(decoration) != decorations.end(); }
        std::optional<uint32_t> get_decoration(spv::Decoration decoration) const
        {
            auto it = decorations.find(decoration);
            if(it == decorations.end() || it->second.size() != 1) return std::nullopt;
            return it->second[0];
        }
    };

    uint32_t version_number, generator_id, schema_id;
    std::map<uint32_t, type> types;
    std::map<uint32_t, variable> variables;
    std::map<uint32_t, constant> constants;
    std::map<uint32_t, entrypoint> entrypoints;
    std::map<uint32_t, metadata> metadatas;

    spirv_parser() {}
    spirv_parser(const std::vector<uint32_t> & words)
    {
        if(words.size() < 5) throw std::runtime_error("not SPIR-V");
        if(words[0] != 0x07230203) throw std::runtime_error("not SPIR-V");    
        version_number = words[1];
        generator_id = words[2];
        schema_id = words[4];
        const uint32_t * it = words.data() + 5, * binary_end = words.data() + words.size();
        while(it != binary_end)
        {
            auto op_code = static_cast<spv::Op>(*it & spv::OpCodeMask);
            const uint32_t op_code_length = *it >> 16;
            const uint32_t * op_code_end = it + op_code_length;
            if(op_code_end > binary_end) throw std::runtime_error("incomplete opcode");
            if(op_code >= spv::OpTypeVoid && op_code <= spv::OpTypeForwardPointer) types[it[1]] = {op_code, {it+2, op_code_end}};
            switch(op_code)
            {
            case spv::OpVariable: variables[it[2]] = {it[1], static_cast<spv::StorageClass>(it[3])}; break;
            case spv::OpConstant: constants[it[2]] = {it[1], {it+3, op_code_end}}; break;
            case spv::OpName: metadatas[it[1]].name = reinterpret_cast<const char *>(&it[2]); break;
            case spv::OpMemberName: metadatas[it[1]].members[it[2]].name = reinterpret_cast<const char *>(&it[3]); break;
            case spv::OpDecorate: metadatas[it[1]].decorations[static_cast<spv::Decoration>(it[2])].assign(it+3, op_code_end); break;
            case spv::OpMemberDecorate: metadatas[it[1]].members[it[2]].decorations[static_cast<spv::Decoration>(it[3])].assign(it+4, op_code_end); break;
            case spv::OpEntryPoint:
                auto & entrypoint = entrypoints[it[2]];
                entrypoint.execution_model = static_cast<spv::ExecutionModel>(it[1]);
                const char * s = reinterpret_cast<const char *>(it+3);
                const size_t max_length = reinterpret_cast<const char *>(op_code_end) - s, length = strnlen(s, max_length);
                if(length == max_length) throw std::runtime_error("missing null terminator");
                entrypoint.name.assign(s, s+length);
                entrypoint.interfaces.assign(it+3+(length+3)/4, op_code_end);
            }
            it = op_code_end;
        }
    }

    shader_module::numeric get_numeric_type(uint32_t id, std::optional<shader_module::matrix_layout> matrix_layout)
    {
        auto & type = types[id];
        switch(type.op)
        {
        case spv::OpTypeInt: 
            if(type.contents[0] != 32) throw std::runtime_error("unsupported int width");
            return {type.contents[1] ? shader_module::int_ : shader_module::uint_, 1, 1, matrix_layout};
        case spv::OpTypeFloat: 
            if(type.contents[0] == 32) return {shader_module::float_, 1, 1, matrix_layout};
            if(type.contents[0] == 64) return {shader_module::double_, 1, 1, matrix_layout};
            throw std::runtime_error("unsupported float width");
        case spv::OpTypeVector: { auto t = get_numeric_type(type.contents[0], matrix_layout); t.row_count = type.contents[1]; return t; }
        case spv::OpTypeMatrix: { auto t = get_numeric_type(type.contents[0], matrix_layout); t.column_count = type.contents[1]; return t; }
        default: throw std::runtime_error("not a numeric type");
        }
    }
    uint32_t get_array_length(uint32_t constant_id)
    {
        auto & constant = constants[constant_id];
        if(constant.literals.size() != 1) throw std::runtime_error("bad constant");
        return constant.literals[0];
    }
    shader_module::type get_type(uint32_t id, std::optional<shader_module::matrix_layout> matrix_layout)
    {
        auto & type = types[id]; auto & meta = metadatas[id];
        if(type.op >= spv::OpTypeInt && type.op <= spv::OpTypeMatrix) return {get_numeric_type(id, matrix_layout)};
        if(type.op == spv::OpTypeImage) 
        {
            auto n = get_numeric_type(type.contents[0], matrix_layout);
            auto dim = static_cast<spv::Dim>(type.contents[1]);
            bool shadow = type.contents[2] == 1, arrayed = type.contents[3] == 1, multisampled = type.contents[4] == 1;
            auto sampled = type.contents[5]; // 0 - unknown, 1 - used with sampler, 2 - used without sampler (i.e. storage image)
            switch(dim)
            {
            case spv::Dim1D: return {shader_module::sampler{n.scalar, sampler_dim::_1d, arrayed, multisampled, shadow}};
            case spv::Dim2D: return {shader_module::sampler{n.scalar, sampler_dim::_2d, arrayed, multisampled, shadow}};
            case spv::Dim3D: return {shader_module::sampler{n.scalar, sampler_dim::_3d, arrayed, multisampled, shadow}};
            case spv::DimCube: return {shader_module::sampler{n.scalar, sampler_dim::cube, arrayed, multisampled, shadow}};
            case spv::DimRect: return {shader_module::sampler{n.scalar, sampler_dim::rect, arrayed, multisampled, shadow}};
            case spv::DimBuffer: return {shader_module::sampler{n.scalar, sampler_dim::buffer, arrayed, multisampled, shadow}};
            case spv::DimSubpassData: return {shader_module::sampler{n.scalar, sampler_dim::subpass_data, arrayed, multisampled, shadow}};
            default: throw std::runtime_error("unsupported image type");
            }
        }
        if(type.op == spv::OpTypeSampledImage) return get_type(type.contents[0], matrix_layout);
        if(type.op == spv::OpTypeArray) return {shader_module::array{shader_module::type(get_type(type.contents[0], matrix_layout)), get_array_length(type.contents[1]), meta.get_decoration(spv::DecorationArrayStride)}};
        if(type.op == spv::OpTypeStruct)
        {
            shader_module::structure s {meta.name};
            // meta.has_decoration(spv::DecorationBlock) is true if this struct is used for VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            // meta.has_decoration(spv::DecorationBufferBlock) is true if this struct is used for VK_DESCRIPTOR_TYPE_STORAGE_BUFFER/VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
            for(size_t i=0; i<type.contents.size(); ++i)
            {
                auto & member_meta = meta.members[exactly(i)];
                std::optional<shader_module::matrix_layout> matrix_layout;
                if(auto stride = member_meta.get_decoration(spv::DecorationMatrixStride)) matrix_layout = shader_module::matrix_layout{*stride, member_meta.has_decoration(spv::DecorationRowMajor)};
                s.members.push_back({member_meta.name, shader_module::type(get_type(type.contents[i], matrix_layout)), member_meta.get_decoration(spv::DecorationOffset)});
            }
            return {std::move(s)};
        }
        throw std::runtime_error("unsupported type");
    }
    shader_module::type get_pointee_type(uint32_t id)
    {
        if(types[id].op != spv::OpTypePointer) throw std::runtime_error("not a pointer type");
        return get_type(types[id].contents[1], std::nullopt);
    }
};

struct shader_compiler_impl : glslang::TShader::Includer
{
    struct result_text
    {
        std::vector<char> text;
        IncludeResult result;
        result_text(const std::string & header_name) : text{load_text_file(header_name.c_str())}, result{header_name, text.data(), text.size(), nullptr} {}
    };
    std::vector<std::unique_ptr<result_text>> results;

    IncludeResult * get_header(const std::string & name)
    {
        // Return previously loaded include file
        for(auto & r : results)
        {
            if(r->result.headerName == name)
            {
                return &r->result;
            }
        }

        // Otherwise attempt to load
        try
        {
            results.push_back(std::make_unique<result_text>(name));
            return &results.back()->result;
        }
        catch(const std::runtime_error &)
        {
            return nullptr; 
        }
    }

    // Implement glslang::TShader::Includer
    IncludeResult * includeSystem(const char * header_name, const char * includer_name, size_t inclusion_depth) override { return nullptr; }
    IncludeResult * includeLocal(const char * header_name, const char * includer_name, size_t inclusion_depth) override 
    { 
        std::string path {includer_name};
        size_t off = path.rfind('/');
        if(off != std::string::npos) path.resize(off+1);
        return get_header(path + header_name);
    }
    void releaseInclude(IncludeResult * result) override {}
};

/*std::vector<uint32_t> shader_compiler::compile_glsl(VkShaderStageFlagBits stage, const char * filename)
{    
    glslang::TShader shader([stage]()
    {
        switch(stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT: return EShLangVertex;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return EShLangTessControl;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return EShLangTessEvaluation;
        case VK_SHADER_STAGE_GEOMETRY_BIT: return EShLangGeometry;
        case VK_SHADER_STAGE_FRAGMENT_BIT: return EShLangFragment;
        case VK_SHADER_STAGE_COMPUTE_BIT: return EShLangCompute;
        default: throw std::logic_error("bad stage");
        }
    }());    

    auto buffer = load_text_file(filename);
    const char * s = buffer.data();
    int l = static_cast<int>(buffer.size());
    shader.setStringsWithLengthsAndNames(&s, &l, &filename, 1);

    if(!shader.parse(&glslang::DefaultTBuiltInResource, 450, ENoProfile, false, false, static_cast<EShMessages>(EShMsgSpvRules|EShMsgVulkanRules), *impl))
    {
        throw std::runtime_error(std::string("GLSL compile failure: ") + shader.getInfoLog());
    }
*/

shader_compiler::shader_compiler()
{
    glslang::InitializeProcess();
    impl = std::make_unique<shader_compiler_impl>();
}

shader_compiler::~shader_compiler()
{
    impl.reset();
    glslang::FinalizeProcess();
}

shader_module load_shader_info_from_spirv(const std::vector<uint32_t> & words)
{
    // Analyze SPIR-V
    spirv_parser mod(words);
    if(mod.entrypoints.size() != 1) throw std::runtime_error("SPIR-V module should have exactly one entrypoint");
    auto & entrypoint = mod.entrypoints.begin()->second;

    // Determine shader stage
    shader_module info {words};
    switch(entrypoint.execution_model)
    {
    case spv::ExecutionModelVertex: info.stage = shader_stage::vertex; break;
    //case spv::ExecutionModelTessellationControl: info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; break;
    //case spv::ExecutionModelTessellationEvaluation: info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
    //case spv::ExecutionModelGeometry: info.stage = VK_SHADER_STAGE_GEOMETRY_BIT; break;
    case spv::ExecutionModelFragment: info.stage = shader_stage::fragment; break;
    //case spv::ExecutionModelGLCompute: info.stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
    default: throw std::runtime_error("invalid execution model");
    }
    info.name = entrypoint.name;

    // Harvest descriptors
    for(auto & v : mod.variables)
    {
        auto & meta = mod.metadatas[v.first];
        auto set = meta.get_decoration(spv::DecorationDescriptorSet);
        auto binding = meta.get_decoration(spv::DecorationBinding);
        if(set && binding) info.descriptors.push_back({*set, *binding, meta.name, mod.get_pointee_type(v.second.type)});

        if(auto loc = meta.get_decoration(spv::DecorationLocation))
        {
            if(v.second.storage_class == spv::StorageClass::StorageClassInput) info.inputs.push_back({*loc, meta.name, mod.get_pointee_type(v.second.type)});
            if(v.second.storage_class == spv::StorageClass::StorageClassOutput) info.outputs.push_back({*loc, meta.name, mod.get_pointee_type(v.second.type)});
        }
    }
    std::sort(begin(info.descriptors), end(info.descriptors), [](const shader_module::descriptor & a, const shader_module::descriptor & b) { return std::tie(a.set, a.binding) < std::tie(b.set, b.binding); });
    std::sort(begin(info.inputs), end(info.inputs), [](const shader_module::interface & a, const shader_module::interface & b) { return a.location < b.location; });
    std::sort(begin(info.outputs), end(info.outputs), [](const shader_module::interface & a, const shader_module::interface & b) { return a.location < b.location; });
    return info;
}

shader_module shader_compiler::compile_file(shader_stage stage, const std::string & filename)
{
    glslang::TShader shader([stage]()
    {
        switch(stage)
        {
        case shader_stage::vertex: return EShLangVertex;
        case shader_stage::fragment: return EShLangFragment;
        default: throw std::logic_error("unsupported shader_stage");
        }
    }());

    const auto text = load_text_file(filename.c_str());
    const auto string = text.data();
    const int length = exactly(text.size());
    const auto name = filename.c_str();
    shader.setStringsWithLengthsAndNames(&string, &length, &name, 1);

    if(!shader.parse(&glslang::DefaultTBuiltInResource, 450, ECoreProfile, false, false, static_cast<EShMessages>(EShMsgSpvRules|EShMsgVulkanRules), *impl))
    {
        throw std::runtime_error(std::string("GLSL compile failure: ") + shader.getInfoLog());
    }
    
    glslang::TProgram program;
    program.addShader(&shader);
    if(!program.link(EShMsgVulkanRules))
    {
        throw std::runtime_error(std::string("GLSL link failure: ") + program.getInfoLog());
    }

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), spirv, nullptr);
    return load_shader_info_from_spirv(spirv);
}

shader_module shader_compiler::compile(shader_stage stage, const std::string & glsl)
{
    glslang::TShader shader([stage]()
    {
        switch(stage)
        {
        case shader_stage::vertex: return EShLangVertex;
        case shader_stage::fragment: return EShLangFragment;
        default: throw std::logic_error("unsupported shader_stage");
        }
    }());
    const char * text = glsl.c_str();
    shader.setStrings(&text, 1);
    if(!shader.parse(&glslang::DefaultTBuiltInResource, 450, ECoreProfile, false, false, static_cast<EShMessages>(EShMsgSpvRules|EShMsgVulkanRules), *impl))
    {
        throw std::runtime_error(std::string("GLSL compile failure: ") + shader.getInfoLog() + "\nsource:\n" + text);
    }
    
    glslang::TProgram program;
    program.addShader(&shader);
    if(!program.link(EShMsgVulkanRules))
    {
        throw std::runtime_error(std::string("GLSL link failure: ") + program.getInfoLog());
    }

    std::vector<uint32_t> spirv;
    glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), spirv, nullptr);
    return load_shader_info_from_spirv(spirv);
}