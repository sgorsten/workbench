#include "rhi-internal.h"

#include <map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../dep/SPIRV-Cross/spirv_glsl.hpp"
#pragma comment(lib, "opengl32.lib")

namespace rhi
{
    // Initialize tables
    struct gl_format { GLenum internal_format, format, type; };
    GLenum convert_gl(shader_stage stage) { switch(stage) { default: fail_fast();
        #define RHI_SHADER_STAGE(CASE, VK, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(address_mode mode) { switch(mode) { default: fail_fast();
        #define RHI_ADDRESS_MODE(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(primitive_topology topology) { switch(topology) { default: fail_fast();
        #define RHI_PRIMITIVE_TOPOLOGY(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(compare_op op) { switch(op) { default: fail_fast();
        #define RHI_COMPARE_OP(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(stencil_op op) { switch(op) { default: fail_fast();
        #define RHI_STENCIL_OP(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(blend_op op) { switch(op) { default: fail_fast();
        #define RHI_BLEND_OP(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    GLenum convert_gl(blend_factor factor) { switch(factor) { default: fail_fast();
        #define RHI_BLEND_FACTOR(CASE, VK, DX, GL) case CASE: return GL;
        #include "rhi-tables.inl"
    }}
    gl_format convert_gl(image_format format) { switch(format) { default: fail_fast();
        #define RHI_IMAGE_FORMAT(CASE, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case CASE: return {GLI, GLF, GLT};
        #include "rhi-tables.inl"
    }}

    struct gl_pipeline;
    struct gl_device : device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        
        std::map<uint64_t, GLsync> sync_objects;
        uint64_t submitted_index=0;

        struct context_objects { std::unordered_map<const gl_pipeline *, GLuint> vertex_array_objects;};
        std::map<GLFWwindow *, context_objects> context_specific_objects;

        gl_device(std::function<void(const char *)> debug_callback);
        
        void enable_debug_callback(GLFWwindow * window);
        void destroy_context_objects(GLFWwindow * context);
        void destroy_pipeline_objects(gl_pipeline * pipeline);
        void bind_vertex_array(GLFWwindow * context, const gl_pipeline & pipeline);

        device_info get_info() const final { return {linalg::neg_one_to_one, false}; }

        ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) final;
        ptr<sampler> create_sampler(const sampler_desc & desc) final;
        ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) final;
        ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) final;
        ptr<window> create_window(const int2 & dimensions, std::string_view title) final;        
        
        ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) final { return new delete_when_unreferenced<emulated_descriptor_set_layout>{bindings}; }
        ptr<pipeline_layout> create_pipeline_layout(const std::vector<const descriptor_set_layout *> & sets) final { return new delete_when_unreferenced<emulated_pipeline_layout>{sets}; }
        ptr<shader> create_shader(const shader_desc & desc) final;
        ptr<pipeline> create_pipeline(const pipeline_desc & desc) final;

        ptr<descriptor_pool> create_descriptor_pool() final { return new delete_when_unreferenced<emulated_descriptor_pool>{}; }
        ptr<command_buffer> create_command_buffer() final { return new delete_when_unreferenced<emulated_command_buffer>(); }

        uint64_t submit(command_buffer & cmd) final;
        uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) final;
        uint64_t get_last_submission_id() final { return submitted_index; }
        void wait_until_complete(uint64_t submit_id) final;
    };

    autoregister_backend<gl_device> autoregister_gl_backend {"OpenGL 4.5 Core", client_api::opengl};

    struct gl_buffer : buffer
    {
        ptr<gl_device> device;
        GLuint buffer_object = 0;
        char * mapped = 0;

        gl_buffer(gl_device * device, const buffer_desc & desc, const void * initial_data);
        ~gl_buffer();
        size_t get_offset_alignment() final { GLint alignment; glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment); return alignment; }
        char * get_mapped_memory() final { return mapped; }
    };

    struct gl_sampler : sampler
    {
        ptr<gl_device> device;
        GLuint sampler_object = 0;

        gl_sampler(gl_device * device, const sampler_desc & desc);
        ~gl_sampler();
    };

    struct gl_image : image
    {
        ptr<gl_device> device;
        GLuint texture_object = 0;
        bool is_layered;

        gl_image(gl_device * device, const image_desc & desc, std::vector<const void *> initial_data);
        ~gl_image();
    };

    struct gl_framebuffer : framebuffer
    {
        ptr<gl_device> device;
        GLuint framebuffer_object = 0;  // If nonzero, this is a framebuffer object in the context of the hidden window
        GLFWwindow * glfw_window = 0;   // If not null, this is the default framebuffer of the specified window
        int2 dims;

        gl_framebuffer(gl_device * device, const framebuffer_desc & desc);
        gl_framebuffer(gl_device * device, const int2 & dimensions, const std::string & title);
        ~gl_framebuffer();
        coord_system get_ndc_coords() const final { return {coord_axis::right, framebuffer_object ? coord_axis::down : coord_axis::up, coord_axis::forward}; }
    };

    struct gl_window : window
    {
        ptr<gl_framebuffer> fb;

        gl_window(gl_device * device, const int2 & dimensions, std::string title);
        GLFWwindow * get_glfw_window() final { return fb->glfw_window; }
        framebuffer & get_swapchain_framebuffer() final { return *fb; }
    };

    struct gl_shader : shader
    {
        shader_desc desc;
        
        gl_shader(const shader_desc & desc) : desc{desc} {}
    };

    struct gl_pipeline : base_pipeline
    {
        struct gl_stencil { GLenum func=GL_ALWAYS, sfail=GL_KEEP, dpfail=GL_KEEP, dppass=GL_KEEP; };
        struct gl_blend { GLboolean red_mask, green_mask, blue_mask, alpha_mask; GLenum color_op, alpha_op, src_color, dst_color, src_alpha, dst_alpha; };
        ptr<gl_device> device;
        GLuint program_object = 0;
        std::vector<rhi::vertex_binding_desc> input;

        // Fixed function state
        std::vector<GLenum> enable, disable;
        GLenum primitive_mode, front_face, cull_face, depth_func;
        GLboolean depth_mask;
        gl_stencil stencil_front, stencil_back;
        GLuint stencil_read_mask=0xFFFFFFFF, stencil_write_mask=0xFFFFFFFF;
        std::vector<gl_blend> blend;

        gl_pipeline(gl_device * device, const pipeline_desc & desc);
        ~gl_pipeline();

        void set_stencil_ref(uint8_t ref) const
        {
            glStencilFuncSeparate(GL_FRONT, stencil_front.func, ref, stencil_read_mask);
            glStencilFuncSeparate(GL_BACK, stencil_back.func, ref, stencil_read_mask);
        }
    };
}

using namespace rhi;

gl_device::gl_device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    if(debug_callback) glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
    hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    if(!hidden_window) throw std::runtime_error("glfwCreateWindow(...) failed");

    glfwMakeContextCurrent(hidden_window);
    if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) throw std::runtime_error("gladLoadGLLoader(...) failed");
    if(debug_callback)
    {
        debug_callback(to_string("GL_VERSION = ", glGetString(GL_VERSION)).c_str());
        debug_callback(to_string("GL_SHADING_LANGUAGE_VERSION = ", glGetString(GL_SHADING_LANGUAGE_VERSION)).c_str());
        debug_callback(to_string("GL_VENDOR = ", glGetString(GL_VENDOR)).c_str());
        debug_callback(to_string("GL_RENDERER = ", glGetString(GL_RENDERER)).c_str());
    }
    enable_debug_callback(hidden_window);
}

void gl_device::enable_debug_callback(GLFWwindow * window)
{
    glfwMakeContextCurrent(window);
    if(debug_callback)
    {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * user) { reinterpret_cast<const gl_device *>(user)->debug_callback(message); }, this);
        const GLuint ids = 0;
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &ids, true);
    }
}

void gl_device::destroy_context_objects(GLFWwindow * context)
{
    glfwMakeContextCurrent(context);
    for(auto vao : context_specific_objects[context].vertex_array_objects) glDeleteVertexArrays(1, &vao.second);
    context_specific_objects.erase(context);
    glfwMakeContextCurrent(hidden_window);
}

void gl_device::destroy_pipeline_objects(gl_pipeline * pipeline)
{
    for(auto & ctx : context_specific_objects)
    {
        auto it = ctx.second.vertex_array_objects.find(pipeline);
        if(it == ctx.second.vertex_array_objects.end()) continue;

        glfwMakeContextCurrent(ctx.first);
        glDeleteVertexArrays(1, &it->second);
        ctx.second.vertex_array_objects.erase(pipeline);
        glfwMakeContextCurrent(hidden_window);
    }
}

void gl_device::bind_vertex_array(GLFWwindow * context, const gl_pipeline & pipeline)
{
    auto & vertex_array = context_specific_objects[context].vertex_array_objects[&pipeline];
    if(!vertex_array)
    {
        // If vertex array object was not yet created in this context, go ahead and generate it
        glCreateVertexArrays(1, &vertex_array);
        for(auto & buf : pipeline.input)
        {
            for(auto & attrib : buf.attributes)
            {
                glEnableVertexArrayAttrib(vertex_array, attrib.index);
                glVertexArrayAttribBinding(vertex_array, attrib.index, buf.index);
                switch(attrib.type)
                {
                #define RHI_ATTRIBUTE_FORMAT(CASE, VK, DX, GL_SIZE, GL_TYPE, GL_NORMALIZED) case CASE: \
                    if(GL_TYPE != GL_FLOAT && !GL_NORMALIZED) glVertexArrayAttribIFormat(vertex_array, attrib.index, GL_SIZE, GL_TYPE, attrib.offset); \
                    else glVertexArrayAttribFormat(vertex_array, attrib.index, GL_SIZE, GL_TYPE, GL_NORMALIZED, attrib.offset); \
                    break;
                #include "rhi-tables.inl"
                default: fail_fast();
                }                
            }
        }
    }
    glBindVertexArray(vertex_array);
}

ptr<buffer> gl_device::create_buffer(const buffer_desc & desc, const void * initial_data) { return new delete_when_unreferenced<gl_buffer>{this, desc, initial_data}; }
ptr<sampler> gl_device::create_sampler(const sampler_desc & desc) { return new delete_when_unreferenced<gl_sampler>{this, desc}; }
ptr<image> gl_device::create_image(const image_desc & desc, std::vector<const void *> initial_data) { return new delete_when_unreferenced<gl_image>{this, desc, initial_data}; }
ptr<framebuffer> gl_device::create_framebuffer(const framebuffer_desc & desc) { return new delete_when_unreferenced<gl_framebuffer>{this, desc}; }
ptr<window> gl_device::create_window(const int2 & dimensions, std::string_view title) { return new delete_when_unreferenced<gl_window>{this, dimensions, std::string{title}}; }
ptr<shader> gl_device::create_shader(const shader_desc & desc) { return new delete_when_unreferenced<gl_shader>{desc}; }
ptr<pipeline> gl_device::create_pipeline(const pipeline_desc & desc) { return new delete_when_unreferenced<gl_pipeline>{this, desc}; }

uint64_t gl_device::submit(command_buffer & cmd)
{
    GLFWwindow * context = hidden_window;
    const gl_pipeline * current_pipeline = nullptr;
    const char * base_indices_pointer = 0;
    int framebuffer_height = 0;
    uint8_t stencil_ref = 0;

    glfwMakeContextCurrent(context);
    static_cast<const emulated_command_buffer &>(cmd).execute(overload(
        [](const generate_mipmaps_command & c)
        {
            glGenerateTextureMipmap(static_cast<gl_image &>(*c.im).texture_object);
        },
        [&](const begin_render_pass_command & c)
        {
            auto & fb = static_cast<gl_framebuffer &>(*c.framebuffer);
            framebuffer_height = fb.dims.y;
            context = fb.glfw_window ? fb.glfw_window : hidden_window;
            glfwMakeContextCurrent(context);
            glEnable(GL_FRAMEBUFFER_SRGB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.framebuffer_object);
            glViewport(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));
            glScissor(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));
            glEnable(GL_SCISSOR_TEST);

            // Clear render targets if specified by render pass
            for(size_t i=0; i<c.pass.color_attachments.size(); ++i)
            {
                if(auto op = std::get_if<clear_color>(&c.pass.color_attachments[i].load_op))
                {
                    glClearColor(op->r, op->g, op->b, op->a);
                    glClear(GL_COLOR_BUFFER_BIT); // TODO: Use glClearTexImage(...) when we use multiple color attachments                            
                }
            }
            if(c.pass.depth_attachment)
            {
                if(auto op = std::get_if<clear_depth>(&c.pass.depth_attachment->load_op))
                {
                    glDepthMask(GL_TRUE);
                    glClearDepthf(op->depth);
                    glClearStencil(op->stencil);
                    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                }
            }
        },
        [](const clear_depth_command & c)
        {
            GLint prev_mask;
            glGetIntegerv(GL_DEPTH_WRITEMASK, &prev_mask);
            glDepthMask(GL_TRUE);
            glClearDepthf(c.depth);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDepthMask(prev_mask);
        },
        [](const clear_stencil_command & c)
        {
            GLint prev_front, prev_back;
            glGetIntegerv(GL_STENCIL_WRITEMASK, &prev_front);
            glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &prev_back);
            glStencilMask(0xFF);
            glClearStencil(c.stencil);
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilMaskSeparate(GL_FRONT, prev_front);
            glStencilMaskSeparate(GL_BACK, prev_back);
        },
        [&](const set_viewport_rect_command & c)
        {
            glViewport(c.x0, framebuffer_height-c.y1, c.x1-c.x0, c.y1-c.y0);
        },
        [&](const set_scissor_rect_command & c)
        {
            glScissor(c.x0, framebuffer_height-c.y1, c.x1-c.x0, c.y1-c.y0);
        },
        [&](const set_stencil_ref_command & c)
        {
            stencil_ref = c.ref;
            if(current_pipeline) current_pipeline->set_stencil_ref(c.ref);
        },
        [&](const bind_pipeline_command & c)
        {
            auto & pipe = static_cast<const gl_pipeline &>(*c.pipe);
            glUseProgram(pipe.program_object);
            bind_vertex_array(context, pipe);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

            for(auto cap : pipe.enable) glEnable(cap);
            for(auto cap : pipe.disable) glDisable(cap);
            glFrontFace(pipe.front_face);
            glCullFace(pipe.cull_face);
            glDepthFunc(pipe.depth_func);
            glDepthMask(pipe.depth_mask);
            pipe.set_stencil_ref(stencil_ref);
            glStencilOpSeparate(GL_FRONT, pipe.stencil_front.sfail, pipe.stencil_front.dpfail, pipe.stencil_front.dppass);
            glStencilOpSeparate(GL_BACK, pipe.stencil_back.sfail, pipe.stencil_back.dpfail, pipe.stencil_back.dppass);
            glStencilMask(pipe.stencil_write_mask);
            for(size_t i=0; i<pipe.blend.size(); ++i)
            {
                glColorMaski(exactly(i), pipe.blend[i].red_mask, pipe.blend[i].green_mask, pipe.blend[i].blue_mask, pipe.blend[i].alpha_mask);
                glBlendEquationSeparatei(exactly(i), pipe.blend[i].color_op, pipe.blend[i].alpha_op);
                glBlendFuncSeparatei(exactly(i), pipe.blend[i].src_color, pipe.blend[i].dst_color, pipe.blend[i].src_alpha, pipe.blend[i].dst_alpha);
            }
            current_pipeline = &pipe;
        },
        [](const bind_descriptor_set_command & c)
        {
            bind_descriptor_set(*c.layout, c.set_index, *c.set, 
                [](size_t index, buffer & buffer, size_t offset, size_t size) { glBindBufferRange(GL_UNIFORM_BUFFER, exactly(index), static_cast<gl_buffer &>(buffer).buffer_object, offset, size); },
                [](size_t index, sampler & sampler, image & image) 
                { 
                    glBindSampler(exactly(index), static_cast<gl_sampler &>(sampler).sampler_object); 
                    glBindTextureUnit(exactly(index), static_cast<gl_image &>(image).texture_object);
                });
        },
        [&](const bind_vertex_buffer_command & c)
        {
            for(auto & buf : current_pipeline->input)
            {
                if(buf.index == c.index)
                {
                    glBindVertexBuffer(c.index, static_cast<gl_buffer &>(c.range.buffer).buffer_object, c.range.offset, buf.stride);
                }
            }        
        },
        [&](const bind_index_buffer_command & c)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<gl_buffer &>(c.range.buffer).buffer_object);
            base_indices_pointer = (const char *)c.range.offset;
        },
        [&](const draw_command & c) { glDrawArrays(current_pipeline->primitive_mode, c.first_vertex, c.vertex_count); },
        [&](const draw_indexed_command & c) { glDrawElements(current_pipeline->primitive_mode, c.index_count, GL_UNSIGNED_INT, base_indices_pointer + c.first_index*sizeof(uint32_t)); },
        [](const end_render_pass_command &) {}
    ));
    sync_objects[++submitted_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    return submitted_index;
}
uint64_t gl_device::acquire_and_submit_and_present(command_buffer & cmd, window & window)
{
    submit(cmd);
    glfwSwapBuffers(window.get_glfw_window());
    return submitted_index;
}

void gl_device::wait_until_complete(uint64_t submit_id)
{
    for(auto it = sync_objects.begin(); it != sync_objects.end(); it = sync_objects.erase(it))
    {
        if(submit_id < it->first) return;
        switch(glClientWaitSync(it->second, 0, GL_TIMEOUT_IGNORED))
        {
        case GL_TIMEOUT_EXPIRED: case GL_WAIT_FAILED: throw std::runtime_error("glClientWaitSync(...) failed");
        }
        glDeleteSync(it->second);
    }
}

gl_buffer::gl_buffer(gl_device * device, const buffer_desc & desc, const void * initial_data) : device{device}
{
    glCreateBuffers(1, &buffer_object);
    GLbitfield flags = 0;
    if(desc.flags & rhi::mapped_memory_bit) flags |= GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(buffer_object, desc.size, initial_data, flags);
    if(desc.flags & rhi::mapped_memory_bit) mapped = reinterpret_cast<char *>(glMapNamedBufferRange(buffer_object, 0, desc.size, flags));
}
gl_buffer::~gl_buffer()
{
    if(mapped) glUnmapNamedBuffer(buffer_object);
    glDeleteBuffers(1, &buffer_object);
}

gl_sampler::gl_sampler(gl_device * device, const sampler_desc & desc) : device{device}
{ 
    auto convert_filter = [](rhi::filter filter)
    {
        switch(filter)
        {
        case rhi::filter::nearest: return GL_NEAREST;
        case rhi::filter::linear: return GL_LINEAR;
        default: fail_fast();
        }
    };

    glCreateSamplers(1, &sampler_object);
    glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_S, convert_gl(desc.wrap_s));
    glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_T, convert_gl(desc.wrap_t));
    glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_R, convert_gl(desc.wrap_r));
    glSamplerParameteri(sampler_object, GL_TEXTURE_MAG_FILTER, convert_filter(desc.mag_filter));
    if(desc.mip_filter)
    {
        if(desc.min_filter == rhi::filter::nearest && *desc.mip_filter == rhi::filter::nearest) glSamplerParameteri(sampler_object, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        if(desc.min_filter == rhi::filter::linear && *desc.mip_filter == rhi::filter::nearest) glSamplerParameteri(sampler_object, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        if(desc.min_filter == rhi::filter::nearest && *desc.mip_filter == rhi::filter::linear) glSamplerParameteri(sampler_object, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        if(desc.min_filter == rhi::filter::linear && *desc.mip_filter == rhi::filter::linear) glSamplerParameteri(sampler_object, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else glSamplerParameteri(sampler_object, GL_TEXTURE_MIN_FILTER, convert_filter(desc.min_filter)); 
}
gl_sampler::~gl_sampler()
{
    glDeleteSamplers(1, &sampler_object);
}

gl_image::gl_image(gl_device * device, const image_desc & desc, std::vector<const void *> initial_data) : device{device}, is_layered{desc.shape == rhi::image_shape::cube}
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    auto glf = convert_gl(desc.format);
    switch(desc.shape)
    {
    case rhi::image_shape::_1d:
        glCreateTextures(GL_TEXTURE_1D, 1, &texture_object);
        glTextureStorage1D(texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x);
        if(initial_data.size() == 1) glTextureSubImage1D(texture_object, 0, 0, desc.dimensions.x, glf.format, glf.type, initial_data[0]);
        break;
    case rhi::image_shape::_2d:
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_object);
        glTextureStorage2D(texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y);
        if(initial_data.size() == 1) glTextureSubImage2D(texture_object, 0, 0, 0, desc.dimensions.x, desc.dimensions.y, glf.format, glf.type, initial_data[0]);
        break;
    case rhi::image_shape::_3d:
        glCreateTextures(GL_TEXTURE_3D, 1, &texture_object);
        glTextureStorage3D(texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y, desc.dimensions.z);
        if(initial_data.size() == 1) glTextureSubImage3D(texture_object, 0, 0, 0, 0, desc.dimensions.x, desc.dimensions.y, desc.dimensions.z, glf.format, glf.type, initial_data[0]);
        break;
    case rhi::image_shape::cube:
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &texture_object);
        glTextureStorage2D(texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y);
        if(initial_data.size() == 6)
        {
            GLuint view;
            glGenTextures(1, &view);
            glTextureView(view, GL_TEXTURE_2D_ARRAY, texture_object, glf.internal_format, 0, 1, 0, 6);
            for(size_t i=0; i<6; ++i) glTextureSubImage3D(view, 0, 0, 0, exactly(i), desc.dimensions.x, desc.dimensions.y, 1, glf.format, glf.type, initial_data[i]);
            glDeleteTextures(1, &view);
        }
        break;
    default: fail_fast();
    }
}
gl_image::~gl_image()
{
    glDeleteTextures(1, &texture_object);
}

gl_framebuffer::gl_framebuffer(gl_device * device, const framebuffer_desc & desc) : device{device}, dims{desc.dimensions}
{
    std::vector<GLenum> draw_buffers;
    glfwMakeContextCurrent(device->hidden_window); // Framebuffers are not shared between GL contexts, so create them all in the hidden window's context
    glCreateFramebuffers(1, &framebuffer_object);
    for(size_t i=0; i<desc.color_attachments.size(); ++i) 
    {
        auto & im = static_cast<gl_image &>(*desc.color_attachments[i].image);
        if(im.is_layered) glNamedFramebufferTextureLayer(framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), im.texture_object, desc.color_attachments[i].mip, desc.color_attachments[i].layer);
        else glNamedFramebufferTexture(framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), im.texture_object, desc.color_attachments[i].mip);
        draw_buffers.push_back(exactly(GL_COLOR_ATTACHMENT0+i));
    }
    if(desc.depth_attachment) 
    {
        auto & im = static_cast<gl_image &>(*desc.depth_attachment->image);
        if(im.is_layered) glNamedFramebufferTextureLayer(framebuffer_object, GL_DEPTH_ATTACHMENT, im.texture_object, desc.depth_attachment->mip, desc.depth_attachment->layer);
        else glNamedFramebufferTexture(framebuffer_object, GL_DEPTH_ATTACHMENT, im.texture_object, desc.depth_attachment->mip);
    }
    glNamedFramebufferDrawBuffers(framebuffer_object, exactly(draw_buffers.size()), draw_buffers.data());
}
gl_framebuffer::gl_framebuffer(gl_device * device, const int2 & dimensions, const std::string & title) : device{device}, dims{dimensions} 
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
    glfw_window = glfwCreateWindow(dimensions.x, dimensions.y, title.c_str(), nullptr, device->hidden_window);
    if(!glfw_window) throw std::runtime_error("glfwCreateWindow(...) failed");
    device->enable_debug_callback(glfw_window);
}
gl_framebuffer::~gl_framebuffer()
{
    if(framebuffer_object)
    {
        glfwMakeContextCurrent(device->hidden_window);
        glDeleteFramebuffers(1, &framebuffer_object);
    }
    if(glfw_window) 
    {
        device->destroy_context_objects(glfw_window);
        glfwDestroyWindow(glfw_window);
    }
}

gl_window::gl_window(gl_device * device, const int2 & dimensions, std::string title)
{
    fb = new delete_when_unreferenced<gl_framebuffer>{device, dimensions, title};
}

gl_pipeline::gl_pipeline(gl_device * device, const pipeline_desc & desc) : base_pipeline{*desc.layout}, device{device}, input{desc.input}
{
    std::vector<GLenum> shaders;
    for(auto s : desc.stages)
    {
        auto & shader = static_cast<const gl_shader &>(*s);
        spirv_cross::CompilerGLSL compiler(shader.desc.spirv);
	    spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	    for(auto & resource : resources.uniform_buffers)
	    {
            compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(static_cast<const emulated_pipeline_layout &>(*desc.layout).get_flat_buffer_binding(compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
		    compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	    }
	    for(auto & resource : resources.sampled_images)
	    {
		    compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(static_cast<const emulated_pipeline_layout &>(*desc.layout).get_flat_image_binding(compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	    }

        const auto glsl = compiler.compile();
        const GLchar * source = glsl.c_str();
        GLint length = exactly(glsl.length());
        //debug_callback(source);

        GLuint shader_object = glCreateShader(convert_gl(shader.desc.stage));
        glShaderSource(shader_object, 1, &source, &length);
        glCompileShader(shader_object);
        shaders.push_back(shader_object);

        GLint status;
        glGetShaderiv(shader_object, GL_COMPILE_STATUS, &status);
        if(status == GL_FALSE)
        {
            glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &length);
            std::vector<char> buffer(length);
            glGetShaderInfoLog(shader_object, exactly(buffer.size()), &length, buffer.data());
            for(auto shader : shaders) glDeleteShader(shader);
            throw std::runtime_error(buffer.data());
        }
    }
    program_object = glCreateProgram();
    for(auto shader : shaders) glAttachShader(program_object, shader);
    glLinkProgram(program_object);
    for(auto shader : shaders) glDeleteShader(shader);

    GLint status, length;
    glGetProgramiv(program_object, GL_LINK_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(length);
        glGetProgramInfoLog(program_object, exactly(buffer.size()), &length, buffer.data());
        glDeleteProgram(program_object);
        throw std::runtime_error(buffer.data());
    }

    // Rasterizer state
    switch(desc.topology)
    {
    #define RHI_PRIMITIVE_TOPOLOGY(TOPOLOGY, VK, DX, GL) case TOPOLOGY: primitive_mode = GL; break;
    #include "rhi-tables.inl"
    default: fail_fast();
    }
    switch(desc.front_face)
    {
    case rhi::front_face::counter_clockwise: front_face = GL_CCW; break;
    case rhi::front_face::clockwise: front_face = GL_CW; break;
    default: fail_fast();
    }
    switch(desc.cull_mode)
    {
    case rhi::cull_mode::none: disable.push_back(GL_CULL_FACE); cull_face = GL_BACK; break;
    case rhi::cull_mode::back: enable.push_back(GL_CULL_FACE); cull_face = GL_BACK; break;
    case rhi::cull_mode::front: enable.push_back(GL_CULL_FACE); cull_face = GL_FRONT; break;
    default: fail_fast();
    }

    // Depth state
    if(desc.depth)
    {
        enable.push_back(GL_DEPTH_TEST);
        depth_func = convert_gl(desc.depth->test);
        depth_mask = desc.depth->write_mask ? GL_TRUE : GL_FALSE;
    }
    else
    {
        disable.push_back(GL_DEPTH_TEST);
        depth_func = GL_ALWAYS;
        depth_mask = GL_FALSE;
    }

    // Stencil state
    if(desc.stencil)
    {
        enable.push_back(GL_STENCIL_TEST);
        stencil_front.func = convert_gl(desc.stencil->front.test);
        stencil_front.sfail = convert_gl(desc.stencil->front.stencil_fail_op);
        stencil_front.dpfail = convert_gl(desc.stencil->front.stencil_pass_depth_fail_op);
        stencil_front.dppass = convert_gl(desc.stencil->front.stencil_pass_depth_pass_op);
        stencil_back.func = convert_gl(desc.stencil->back.test);
        stencil_back.sfail = convert_gl(desc.stencil->back.stencil_fail_op);
        stencil_back.dpfail = convert_gl(desc.stencil->back.stencil_pass_depth_fail_op);
        stencil_back.dppass = convert_gl(desc.stencil->back.stencil_pass_depth_pass_op);
        stencil_read_mask = desc.stencil->read_mask;
        stencil_write_mask = desc.stencil->write_mask;
    }
    else
    {
        disable.push_back(GL_STENCIL_TEST);
    }

    // Blending state
    bool enable_blend = false;
    for(auto & b : desc.blend)
    {
        GLboolean mr = b.write_mask ? GL_TRUE : GL_FALSE;
        GLboolean mg = b.write_mask ? GL_TRUE : GL_FALSE;
        GLboolean mb = b.write_mask ? GL_TRUE : GL_FALSE;
        GLboolean ma = b.write_mask ? GL_TRUE : GL_FALSE;
        if(b.enable)
        {
            enable_blend = true;
            blend.push_back({mr, mg, mb, ma, convert_gl(b.color.op), convert_gl(b.alpha.op), convert_gl(b.color.source_factor), convert_gl(b.color.dest_factor), convert_gl(b.alpha.source_factor), convert_gl(b.alpha.dest_factor)});
        }
        else blend.push_back({mr, mg, mb, ma, GL_FUNC_ADD, GL_FUNC_ADD, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO});
    }
    if(enable_blend) enable.push_back(GL_BLEND);
    else disable.push_back(GL_BLEND);
}
gl_pipeline::~gl_pipeline()
{
    device->destroy_pipeline_objects(this);
    glDeleteProgram(program_object);
}

