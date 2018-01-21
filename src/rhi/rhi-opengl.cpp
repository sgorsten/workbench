#include "rhi-internal.h"

#include <map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../dep/SPIRV-Cross/spirv_glsl.hpp"
#pragma comment(lib, "opengl32.lib")

namespace rhi
{
    struct gl_format { GLenum internal_format, format, type; };
    gl_format get_gl_format(image_format format)
    {
        switch(format)
        {
        #define X(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return {GLI, GLF, GLT};
        #include "rhi-format.inl"
        #undef X
        default: fail_fast();
        }
    }

    struct gl_device : device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        
        std::map<uint64_t, GLsync> sync_objects;
        uint64_t submitted_index=0;

        void enable_debug_callback(GLFWwindow * window)
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

        gl_device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
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
                std::ostringstream ss;
                ss << "GL_VERSION = " << glGetString(GL_VERSION);
                ss << "\nGL_SHADING_LANGUAGE_VERSION = " << glGetString(GL_SHADING_LANGUAGE_VERSION);
                ss << "\nGL_VENDOR = " << glGetString(GL_VENDOR);
                ss << "\nGL_RENDERER = " << glGetString(GL_RENDERER);
                debug_callback(ss.str().c_str());
            }
            enable_debug_callback(hidden_window);
        }
        
        device_info get_info() const override { return {linalg::neg_one_to_one, false}; }

        ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) override;
        ptr<sampler> create_sampler(const sampler_desc & desc) override;
        ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) override;
        ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) override;
        ptr<window> create_window(const int2 & dimensions, std::string_view title) override;        
        
        ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override { return descriptor_emulator::create_descriptor_set_layout(bindings); }
        ptr<pipeline_layout> create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets) override { return descriptor_emulator::create_pipeline_layout(sets); }
        ptr<shader> create_shader(const shader_module & module) override;
        ptr<pipeline> create_pipeline(const pipeline_desc & desc) override;

        ptr<descriptor_pool> create_descriptor_pool() override { return descriptor_emulator::create_descriptor_pool(); }

        ptr<command_buffer> start_command_buffer() override { return command_emulator::start_command_buffer(); }
        uint64_t submit(command_buffer & cmd) override;
        uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) override;
        void wait_until_complete(uint64_t submit_id) override;
    };

    autoregister_backend<gl_device> autoregister_gl_backend {"OpenGL 4.5 Core"};

    struct gl_buffer : buffer
    {
        GLuint buffer_object = 0;
        char * mapped = 0;

        gl_buffer(const buffer_desc & desc, const void * initial_data)
        {
            glCreateBuffers(1, &buffer_object);
            GLbitfield flags = 0;
            if(desc.dynamic) flags |= GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT;
            glNamedBufferStorage(buffer_object, desc.size, initial_data, flags);
            if(desc.dynamic) mapped = reinterpret_cast<char *>(glMapNamedBuffer(buffer_object, GL_WRITE_ONLY));
        }

        char * get_mapped_memory() override { return mapped; }
    };

    struct gl_sampler : sampler
    {
        GLuint sampler_object = 0;

        gl_sampler(const sampler_desc & desc)
        { 
            auto convert_mode = [](rhi::address_mode mode)
            {
                switch(mode)
                {
                case rhi::address_mode::repeat: return GL_REPEAT;
                case rhi::address_mode::mirrored_repeat: return GL_MIRRORED_REPEAT;
                case rhi::address_mode::clamp_to_edge: return GL_CLAMP_TO_EDGE;
                case rhi::address_mode::mirror_clamp_to_edge: return GL_MIRROR_CLAMP_TO_EDGE;
                case rhi::address_mode::clamp_to_border: return GL_CLAMP_TO_BORDER;
                default: fail_fast();
                }
            };
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
            glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_S, convert_mode(desc.wrap_s));
            glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_T, convert_mode(desc.wrap_t));
            glSamplerParameteri(sampler_object, GL_TEXTURE_WRAP_R, convert_mode(desc.wrap_r));
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
    };

    struct gl_image : image
    {
        image_desc desc;
        GLuint texture_object;

        gl_image(const image_desc & desc, std::vector<const void *> initial_data) : desc{desc}
        {
            auto glf = get_gl_format(desc.format);
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
    };

    struct gl_framebuffer : framebuffer
    {
        GLFWwindow * context;
        GLuint framebuffer_object;
        int2 dims;

        gl_framebuffer() {}
        gl_framebuffer(GLFWwindow * hidden_window, const framebuffer_desc & desc) : context{hidden_window}
        {
            dims = desc.dimensions;
            std::vector<GLenum> draw_buffers;
            glfwMakeContextCurrent(hidden_window); // Framebuffers are not shared between GL contexts, so create them all in the hidden window's context
            glCreateFramebuffers(1, &framebuffer_object);
            for(size_t i=0; i<desc.color_attachments.size(); ++i) 
            {
                if(static_cast<gl_image &>(*desc.color_attachments[i].image).desc.shape == rhi::image_shape::cube) glNamedFramebufferTextureLayer(framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), static_cast<gl_image &>(*desc.color_attachments[i].image).texture_object, desc.color_attachments[i].mip, desc.color_attachments[i].layer);
                else glNamedFramebufferTexture(framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), static_cast<gl_image &>(*desc.color_attachments[i].image).texture_object, desc.color_attachments[i].mip);
                draw_buffers.push_back(exactly(GL_COLOR_ATTACHMENT0+i));
            }
            if(desc.depth_attachment) 
            {
                if(static_cast<gl_image &>(*desc.depth_attachment->image).desc.shape == rhi::image_shape::cube) glNamedFramebufferTextureLayer(framebuffer_object, GL_DEPTH_ATTACHMENT, static_cast<gl_image &>(*desc.depth_attachment->image).texture_object, desc.depth_attachment->mip, desc.depth_attachment->layer);
                else glNamedFramebufferTexture(framebuffer_object, GL_DEPTH_ATTACHMENT, static_cast<gl_image &>(*desc.depth_attachment->image).texture_object, desc.depth_attachment->mip);
            }
            glNamedFramebufferDrawBuffers(framebuffer_object, exactly(draw_buffers.size()), draw_buffers.data());
        }

        coord_system get_ndc_coords() const override { return {coord_axis::right, framebuffer_object ? coord_axis::down : coord_axis::up, coord_axis::forward}; }
    };

    struct gl_window : window
    {
        ptr<gl_device> device;
        GLFWwindow * w;
        ptr<gl_framebuffer> fb;

        gl_window(gl_device * device, const int2 & dimensions, std::string title) : device{device}
        {
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            w = glfwCreateWindow(dimensions.x, dimensions.y, title.c_str(), nullptr, device->hidden_window);
            if(!w) throw std::runtime_error("glfwCreateWindow(...) failed");
            device->enable_debug_callback(w);

            fb = new delete_when_unreferenced<gl_framebuffer>{};
            fb->context = w;
            fb->framebuffer_object = 0;
            fb->dims = dimensions;
        }

        GLFWwindow * get_glfw_window() override { return w; }
        framebuffer & get_swapchain_framebuffer() override { return *fb; }
    };

    struct gl_shader : shader
    {
        shader_module module;
        
        gl_shader(const shader_module & module) : module{module} {}
    };

    struct gl_pipeline : pipeline
    {
        pipeline_desc desc;
        GLuint program_object;
        mutable std::unordered_map<GLFWwindow *, GLuint> vertex_array_objects; // vertex array objects cannot be shared between OpenGL contexts, so we must cache them per-context
    
        gl_pipeline(const pipeline_desc & desc) : desc{desc}
        {
            program_object = glCreateProgram();
            for(auto s : desc.stages)
            {
                auto & shader = static_cast<gl_shader &>(*s);
                spirv_cross::CompilerGLSL compiler(shader.module.spirv);
	            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	            for(auto & resource : resources.uniform_buffers)
	            {
                    compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(descriptor_emulator::get_flat_buffer_binding(*desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
		            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }
	            for(auto & resource : resources.sampled_images)
	            {
		            compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(descriptor_emulator::get_flat_image_binding(*desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
                    compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }

                const auto glsl = compiler.compile();
                const GLchar * source = glsl.c_str();
                GLint length = exactly(glsl.length());
                //debug_callback(source);

                GLuint shader_object = glCreateShader([&shader]() 
                {
                    switch(shader.module.stage)
                    {
                    case shader_stage::vertex: return GL_VERTEX_SHADER;
                    case shader_stage::fragment: return GL_FRAGMENT_SHADER;
                    default: throw std::logic_error("unsupported shader_stage");
                    }
                }());
                glShaderSource(shader_object, 1, &source, &length);
                glCompileShader(shader_object);

                GLint status;
                glGetShaderiv(shader_object, GL_COMPILE_STATUS, &status);
                if(status == GL_FALSE)
                {
                    glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &length);
                    std::vector<char> buffer(length);
                    glGetShaderInfoLog(shader_object, exactly(buffer.size()), &length, buffer.data());
                    throw std::runtime_error(buffer.data());
                }
            
                glAttachShader(program_object, shader_object);
            }
            glLinkProgram(program_object);

            GLint status, length;
            glGetProgramiv(program_object, GL_LINK_STATUS, &status);
            if(status == GL_FALSE)
            {
                glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &length);
                std::vector<char> buffer(length);
                glGetProgramInfoLog(program_object, exactly(buffer.size()), &length, buffer.data());
                throw std::runtime_error(buffer.data());
            }
        }

        void bind_vertex_array(GLFWwindow * context) const
        {
            auto & vertex_array = vertex_array_objects[context];
            if(!vertex_array)
            {
                // If vertex array object was not yet created in this context, go ahead and generate it
                glCreateVertexArrays(1, &vertex_array);
                for(auto & buf : desc.input)
                {
                    for(auto & attrib : buf.attributes)
                    {
                        glEnableVertexArrayAttrib(vertex_array, attrib.index);
                        glVertexArrayAttribBinding(vertex_array, attrib.index, buf.index);
                        switch(attrib.type)
                        {
                        case attribute_format::float1: glVertexArrayAttribFormat(vertex_array, attrib.index, 1, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case attribute_format::float2: glVertexArrayAttribFormat(vertex_array, attrib.index, 2, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case attribute_format::float3: glVertexArrayAttribFormat(vertex_array, attrib.index, 3, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case attribute_format::float4: glVertexArrayAttribFormat(vertex_array, attrib.index, 4, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        }                
                    }
                }
            }
            glBindVertexArray(vertex_array);
        }
    };

    ptr<buffer> gl_device::create_buffer(const buffer_desc & desc, const void * initial_data) { return new delete_when_unreferenced<gl_buffer>{desc, initial_data}; }
    ptr<sampler> gl_device::create_sampler(const sampler_desc & desc) { return new delete_when_unreferenced<gl_sampler>{desc}; }
    ptr<image> gl_device::create_image(const image_desc & desc, std::vector<const void *> initial_data) { return new delete_when_unreferenced<gl_image>{desc, initial_data}; }
    ptr<framebuffer> gl_device::create_framebuffer(const framebuffer_desc & desc) { return new delete_when_unreferenced<gl_framebuffer>{hidden_window, desc}; }
    ptr<window> gl_device::create_window(const int2 & dimensions, std::string_view title) { return new delete_when_unreferenced<gl_window>{this, dimensions, std::string{title}}; }
    ptr<shader> gl_device::create_shader(const shader_module & module) { return new delete_when_unreferenced<gl_shader>{module}; }
    ptr<pipeline> gl_device::create_pipeline(const pipeline_desc & desc) { return new delete_when_unreferenced<gl_pipeline>{desc}; }
}

using namespace rhi;

uint64_t gl_device::submit(command_buffer & cmd)
{
    GLFWwindow * context = hidden_window;
    gl_pipeline * current_pipeline = nullptr;
    size_t index_buffer_offset = 0;
    glfwMakeContextCurrent(context);
    command_emulator::execute(cmd, overload(
        [](const generate_mipmaps_command & c)
        {
            glGenerateTextureMipmap(static_cast<gl_image &>(*c.im).texture_object);
        },
        [&](const begin_render_pass_command & c)
        {
            auto & fb = static_cast<gl_framebuffer &>(*c.framebuffer);
            context = fb.context;
            glfwMakeContextCurrent(fb.context);
            glEnable(GL_FRAMEBUFFER_SRGB);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.framebuffer_object);
            glViewport(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));

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
                    glClearDepthf(op->depth);
                    glClearStencil(op->stencil);
                    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                }
            }
        },
        [&](const bind_pipeline_command & c)
        {
            current_pipeline = &static_cast<gl_pipeline &>(*c.pipe);
            glUseProgram(current_pipeline->program_object);
            current_pipeline->bind_vertex_array(context);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

            // Rasterizer state
            switch(current_pipeline->desc.front_face)
            {
            case rhi::front_face::counter_clockwise: glFrontFace(GL_CCW); break;
            case rhi::front_face::clockwise: glFrontFace(GL_CW); break;
            }
            switch(current_pipeline->desc.cull_mode)
            {
            case rhi::cull_mode::none: glDisable(GL_CULL_FACE); break;
            case rhi::cull_mode::back: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
            case rhi::cull_mode::front: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            }                   

            // Depth stencil state
            (current_pipeline->desc.depth_test ? glEnable : glDisable)(GL_DEPTH_TEST);
            if(current_pipeline->desc.depth_test) glDepthFunc(GL_NEVER | static_cast<int>(*current_pipeline->desc.depth_test));
            glDepthMask(current_pipeline->desc.depth_write ? GL_TRUE : GL_FALSE);

        },
        [](const bind_descriptor_set_command & c)
        {
            descriptor_emulator::bind_descriptor_set(*c.layout, c.set_index, *c.set, 
                [](size_t index, buffer_range range) { glBindBufferRange(GL_UNIFORM_BUFFER, exactly(index), static_cast<gl_buffer &>(*range.buffer).buffer_object, range.offset, range.size); },
                [](size_t index, sampler & sampler, image & image) 
                { 
                    glBindSampler(exactly(index), static_cast<gl_sampler &>(sampler).sampler_object); 
                    glBindTextureUnit(exactly(index), static_cast<gl_image &>(image).texture_object);
                });
        },
        [&](const bind_vertex_buffer_command & c)
        {
            for(auto & buf : current_pipeline->desc.input)
            {
                if(buf.index == c.index)
                {
                    glBindVertexBuffer(c.index, static_cast<gl_buffer &>(*c.range.buffer).buffer_object, c.range.offset, buf.stride);
                }
            }        
        },
        [&](const bind_index_buffer_command & c)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<gl_buffer &>(*c.range.buffer).buffer_object);
            index_buffer_offset = c.range.offset;
        },
        [&](const draw_command & c)
        {
            switch(current_pipeline->desc.topology)
            {
            case primitive_topology::points: glDrawArrays(GL_POINTS, c.first_vertex, c.vertex_count); break;
            case primitive_topology::lines: glDrawArrays(GL_LINES, c.first_vertex, c.vertex_count); break;
            case primitive_topology::triangles: glDrawArrays(GL_TRIANGLES, c.first_vertex, c.vertex_count); break;
            }
        },
        [&](const draw_indexed_command & c)
        {
            auto indices = (const void *)(index_buffer_offset + c.first_index*sizeof(uint32_t));
            switch(current_pipeline->desc.topology)
            {
            case primitive_topology::points: glDrawElements(GL_POINTS, c.index_count, GL_UNSIGNED_INT, indices); break;
            case primitive_topology::lines: glDrawElements(GL_LINES, c.index_count, GL_UNSIGNED_INT, indices); break;
            case primitive_topology::triangles: glDrawElements(GL_TRIANGLES, c.index_count, GL_UNSIGNED_INT, indices); break;
            }
        },
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
