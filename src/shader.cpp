#include "shader.h"

#include "../dep/glslang/glslang/Public/ShaderLang.h"
#include "../dep/glslang/SPIRV/GlslangToSpv.h"
#include "../dep/glslang/StandAlone/ResourceLimits.h"

shader_compiler::shader_compiler()
{
    glslang::InitializeProcess();
}

shader_compiler::~shader_compiler()
{
    glslang::FinalizeProcess();
}

shader_module shader_compiler::compile(shader_stage stage, const char * glsl)
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
    shader.setStrings(&glsl, 1);
    if(!shader.parse(&glslang::DefaultTBuiltInResource, 450, ECoreProfile, false, false, static_cast<EShMessages>(EShMsgSpvRules|EShMsgVulkanRules)))
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
    return {stage, spirv};
}