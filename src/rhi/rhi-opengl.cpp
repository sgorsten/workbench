#include "rhi-internal.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../dep/SPIRV-Cross/spirv_glsl.hpp"
#pragma comment(lib, "opengl32.lib")

namespace rhi
{
    GLenum get_gl_format(image_format format)
    {
        switch(format)
        {
        #define X(FORMAT, SIZE, TYPE, VK, GL, DX) case FORMAT: return GL;
        #include "rhi-format.inl"
        #undef X
        default: fail_fast();
        }
    }

    struct gl_buffer
    {
        GLuint buffer_object = 0;
        char * mapped = 0;
    };

    struct gl_image
    {
        GLuint texture_object;
    };

    struct gl_render_pass
    {
        render_pass_desc desc;
    };

    struct gl_framebuffer
    {
        GLFWwindow * context;
        GLuint framebuffer_object;
        int2 dims;
    };

    struct gl_pipeline
    {
        pipeline_desc desc;
        GLuint program_object;
        mutable std::unordered_map<GLFWwindow *, GLuint> vertex_array_objects; // vertex array objects cannot be shared between OpenGL contexts, so we must cache them per-context
    
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

    struct gl_window
    {
        GLFWwindow * w;
        framebuffer fb;
    };

    struct gl_device : device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        gl_pipeline * current_pipeline=0;
        size_t index_buffer_offset=0;

        template<class T> struct traits;
        template<> struct traits<buffer> { using type = gl_buffer; };
        template<> struct traits<image> { using type = gl_image; };
        template<> struct traits<sampler> { using type = GLuint; };
        template<> struct traits<render_pass> { using type = gl_render_pass; };
        template<> struct traits<framebuffer> { using type = gl_framebuffer; };
        template<> struct traits<shader> { using type = shader_module; }; 
        template<> struct traits<pipeline> { using type = gl_pipeline; };
        template<> struct traits<window> { using type = gl_window; };
        heterogeneous_object_set<traits, buffer, image, sampler, render_pass, framebuffer, shader, pipeline, window> objects;
        descriptor_emulator desc_emulator;
        command_emulator cmd_emulator;

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
            if(debug_callback) glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
            if(!hidden_window) throw std::runtime_error("glfwCreateWindow(...) failed");

            glfwMakeContextCurrent(hidden_window);
            if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) throw std::runtime_error("gladLoadGLLoader(...) failed");
            if(debug_callback)
            {
                std::ostringstream ss;
                ss << "GL_VERSION = " << glGetString(GL_VERSION) << std::endl;
                ss << "GL_SHADING_LANGUAGE_VERSION = " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
                ss << "GL_VENDOR = " << glGetString(GL_VENDOR) << std::endl;
                ss << "GL_RENDERER = " << glGetString(GL_RENDERER) << std::endl;
                debug_callback(ss.str().c_str());
            }
            enable_debug_callback(hidden_window);
        }
        
        device_info get_info() const override { return {{coord_axis::right, coord_axis::up, coord_axis::forward}, linalg::neg_one_to_one}; }

        image create_image(const image_desc & desc, std::vector<const void *> initial_data) override
        {
            auto [handle, im] = objects.create<image>();
            switch(desc.shape)
            {
            case rhi::image_shape::_2d:
                glCreateTextures(GL_TEXTURE_2D, 1, &im.texture_object);
                glTextureStorage2D(im.texture_object, desc.mip_levels, get_gl_format(desc.format), desc.dimensions.x, desc.dimensions.y);
                if(initial_data.size() == 1) glTextureSubImage2D(im.texture_object, 0, 0, 0, desc.dimensions.x, desc.dimensions.y, GL_RGBA, GL_UNSIGNED_BYTE, initial_data[0]);
                break;
            }
            return handle;
        }
        void destroy_image(image image) override { objects.destroy(image); }

        sampler create_sampler(const sampler_desc & desc) override 
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

            auto [handle, samp] = objects.create<sampler>();
            glCreateSamplers(1, &samp);
            glSamplerParameteri(samp, GL_TEXTURE_WRAP_S, convert_mode(desc.wrap_s));
            glSamplerParameteri(samp, GL_TEXTURE_WRAP_T, convert_mode(desc.wrap_t));
            glSamplerParameteri(samp, GL_TEXTURE_WRAP_R, convert_mode(desc.wrap_r));
            glSamplerParameteri(samp, GL_TEXTURE_MAG_FILTER, convert_filter(desc.mag_filter));
            if(desc.mip_filter)
            {
                if(desc.min_filter == rhi::filter::nearest && *desc.mip_filter == rhi::filter::nearest) glSamplerParameteri(samp, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                if(desc.min_filter == rhi::filter::linear && *desc.mip_filter == rhi::filter::nearest) glSamplerParameteri(samp, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
                if(desc.min_filter == rhi::filter::nearest && *desc.mip_filter == rhi::filter::linear) glSamplerParameteri(samp, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                if(desc.min_filter == rhi::filter::linear && *desc.mip_filter == rhi::filter::linear) glSamplerParameteri(samp, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            }
            else glSamplerParameteri(samp, GL_TEXTURE_MIN_FILTER, convert_filter(desc.min_filter));
            return handle;        
        }
        void destroy_sampler(sampler sampler) override { objects.destroy(sampler); }

        render_pass create_render_pass(const render_pass_desc & desc) override 
        {
            auto [handle, pass] = objects.create<render_pass>();
            pass.desc = desc;
            return handle;
        }
        void destroy_render_pass(render_pass pass) override { objects.destroy(pass); }

        framebuffer create_framebuffer(const framebuffer_desc & desc) override
        {
            auto [handle, fb] = objects.create<framebuffer>();
            fb.context = hidden_window;
            fb.dims = desc.dimensions;
            std::vector<GLenum> draw_buffers;
            glfwMakeContextCurrent(hidden_window); // Framebuffers are not shared between GL contexts, so create them all in the hidden window's context
            glCreateFramebuffers(1, &fb.framebuffer_object);
            for(size_t i=0; i<desc.color_attachments.size(); ++i) 
            {
                glNamedFramebufferTexture(fb.framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), objects[desc.color_attachments[i]].texture_object, 0);
                draw_buffers.push_back(exactly(GL_COLOR_ATTACHMENT0+i));
            }
            if(desc.depth_attachment) glNamedFramebufferTexture(fb.framebuffer_object, GL_DEPTH_ATTACHMENT, objects[*desc.depth_attachment].texture_object, 0);
            glNamedFramebufferDrawBuffers(fb.framebuffer_object, exactly(draw_buffers.size()), draw_buffers.data());
            return handle;
        }
        void destroy_framebuffer(framebuffer framebuffer) override { objects.destroy(framebuffer); }

        descriptor_set_layout create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override { return desc_emulator.create_descriptor_set_layout(bindings); }
        descriptor_pool create_descriptor_pool() override { return desc_emulator.create_descriptor_pool(); }
        void reset_descriptor_pool(descriptor_pool pool) override { desc_emulator.reset_descriptor_pool(pool); }
        descriptor_set alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout layout) override { return desc_emulator.alloc_descriptor_set(pool, layout); }
        void write_descriptor(descriptor_set set, int binding, buffer_range range) override { desc_emulator.write_descriptor(set, binding, range); }
        void write_descriptor(descriptor_set set, int binding, sampler sampler, image image) override { desc_emulator.write_descriptor(set, binding, sampler, image); }

        window create_window(render_pass pass, const int2 & dimensions, std::string_view title) override
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            auto [handle, win] = objects.create<window>();
            win.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, hidden_window);
            if(!win.w) throw std::runtime_error("glfwCreateWindow(...) failed");
            enable_debug_callback(win.w);

            auto [fb_handle, fb] = objects.create<framebuffer>();
            fb.context = win.w;
            fb.framebuffer_object = 0;
            fb.dims = dimensions;
            win.fb = fb_handle;
            return {handle};
        }
        GLFWwindow * get_glfw_window(window window) override { return objects[window].w; }
        framebuffer get_swapchain_framebuffer(window window) override { return objects[window].fb; }

        pipeline_layout create_pipeline_layout(const std::vector<descriptor_set_layout> & sets) override { return desc_emulator.create_pipeline_layout(sets); }

        shader create_shader(const shader_module & module) override
        {
            auto [handle, s] = objects.create<shader>();
            s = module;
            return handle;
        }

        pipeline create_pipeline(const pipeline_desc & desc) override
        {
            auto program = glCreateProgram();
            for(auto s : desc.stages)
            {
                auto & shader = objects[s];
                spirv_cross::CompilerGLSL compiler(shader.spirv);
	            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	            for(auto & resource : resources.uniform_buffers)
	            {
                    compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(desc_emulator.get_flat_buffer_binding(desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
		            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }
	            for(auto & resource : resources.sampled_images)
	            {
		            compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(desc_emulator.get_flat_image_binding(desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
                    compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }

                const auto glsl = compiler.compile();
                const GLchar * source = glsl.c_str();
                GLint length = exactly(glsl.length());
                //debug_callback(source);

                GLuint shader_object = glCreateShader([&shader]() 
                {
                    switch(shader.stage)
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
            
                glAttachShader(program, shader_object);
            }
            glLinkProgram(program);

            GLint status, length;
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if(status == GL_FALSE)
            {
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                std::vector<char> buffer(length);
                glGetProgramInfoLog(program, exactly(buffer.size()), &length, buffer.data());
                throw std::runtime_error(buffer.data());
            }

            auto [handle, pipe] = objects.create<pipeline>();
            pipe.desc = desc;
            pipe.program_object = program;
            return handle;
        }

        buffer create_buffer(const buffer_desc & desc, const void * initial_data) override
        {
            auto [handle, buf] = objects.create<buffer>();
            glCreateBuffers(1, &buf.buffer_object);
            GLbitfield flags = 0;
            if(desc.dynamic) flags |= GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT;
            glNamedBufferStorage(buf.buffer_object, desc.size, initial_data, flags);
            if(desc.dynamic) buf.mapped = reinterpret_cast<char *>(glMapNamedBuffer(buf.buffer_object, GL_WRITE_ONLY));
            return handle;
        }
        char * get_mapped_memory(buffer buffer) override { return objects[buffer].mapped; }
        void destroy_buffer(buffer buffer) override { objects.destroy(buffer); }

        void destroy_descriptor_pool(descriptor_pool pool) override { desc_emulator.destroy(pool); }
        void destroy_descriptor_set_layout(descriptor_set_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_pipeline_layout(pipeline_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_shader(shader shader) override { objects.destroy(shader); }
        void destroy_pipeline(pipeline pipeline)  override { objects.destroy(pipeline); }       
        void destroy_window(window window) override { objects.destroy(window); }

        void submit_command_buffer(command_buffer cmd)
        {
            GLFWwindow * context = hidden_window;
            cmd_emulator.execute(cmd, overload(
                [this, &context](const begin_render_pass_command & c)
                {
                    auto & pass = objects[c.pass];
                    auto & fb = objects[c.framebuffer];
                    context = fb.context;
                    glfwMakeContextCurrent(fb.context);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.framebuffer_object);
                    glViewport(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));

                    // Clear render targets if specified by render pass
                    for(size_t i=0; i<pass.desc.color_attachments.size(); ++i)
                    {
                        if(std::holds_alternative<clear>(pass.desc.color_attachments[i].load_op))
                        {
                            glClearColor(0,0,0,1);
                            glClear(GL_COLOR_BUFFER_BIT); // TODO: Use glClearTexImage(...) when we use multiple color attachments                            
                        }
                    }
                    if(pass.desc.depth_attachment)
                    {
                        if(std::holds_alternative<clear>(pass.desc.depth_attachment->load_op))
                        {
                            glClearDepthf(1.0f);
                            glClear(GL_DEPTH_BUFFER_BIT);
                        }
                    }
                },
                [this, context](const bind_pipeline_command & c)
                {
                    auto & p = objects[c.pipe];
                    glUseProgram(p.program_object);
                    p.bind_vertex_array(context);
                    (p.desc.depth_test ? glEnable : glDisable)(GL_DEPTH_TEST);
                    if(p.desc.depth_test) glDepthFunc(GL_NEVER | static_cast<int>(*p.desc.depth_test));
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    current_pipeline = &p;
                },
                [this](const bind_descriptor_set_command & c)
                {
                    desc_emulator.bind_descriptor_set(c.layout, c.set_index, c.set, 
                        [this](size_t index, buffer_range range) { glBindBufferRange(GL_UNIFORM_BUFFER, exactly(index), objects[range.buffer].buffer_object, range.offset, range.size); },
                        [this](size_t index, sampler sampler, image image) { glBindSampler(exactly(index), objects[sampler]); glBindTextureUnit(exactly(index), objects[image].texture_object); });
                },
                [this](const bind_vertex_buffer_command & c)
                {
                    for(auto & buf : current_pipeline->desc.input)
                    {
                        if(buf.index == c.index)
                        {
                            glBindVertexBuffer(c.index, objects[c.range.buffer].buffer_object, c.range.offset, buf.stride);
                        }
                    }        
                },
                [this](const bind_index_buffer_command & c)
                {
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, objects[c.range.buffer].buffer_object);
                    index_buffer_offset = c.range.offset;
                },
                [this](const draw_command & c)
                {
                    switch(current_pipeline->desc.topology)
                    {
                    case primitive_topology::points: glDrawArrays(GL_POINTS, c.first_vertex, c.vertex_count); break;
                    case primitive_topology::lines: glDrawArrays(GL_LINES, c.first_vertex, c.vertex_count); break;
                    case primitive_topology::triangles: glDrawArrays(GL_TRIANGLES, c.first_vertex, c.vertex_count); break;
                    }
                },
                [this](const draw_indexed_command & c)
                {
                    auto indices = (const void *)(index_buffer_offset + c.first_index*sizeof(uint32_t));
                    switch(current_pipeline->desc.topology)
                    {
                    case primitive_topology::points: glDrawElements(GL_POINTS, c.index_count, GL_UNSIGNED_INT, indices); break;
                    case primitive_topology::lines: glDrawElements(GL_LINES, c.index_count, GL_UNSIGNED_INT, indices); break;
                    case primitive_topology::triangles: glDrawElements(GL_TRIANGLES, c.index_count, GL_UNSIGNED_INT, indices); break;
                    }
                },
                [this](const end_render_pass_command &) {}
            ));
        }

        command_buffer start_command_buffer() override { return cmd_emulator.create_command_buffer(); }
        void begin_render_pass(command_buffer cmd, render_pass pass, framebuffer framebuffer) override { cmd_emulator.begin_render_pass(cmd, pass, framebuffer); }
        void bind_pipeline(command_buffer cmd, pipeline pipe) override { return cmd_emulator.bind_pipeline(cmd, pipe); }
        void bind_descriptor_set(command_buffer cmd, pipeline_layout layout, int set_index, descriptor_set set) override { return cmd_emulator.bind_descriptor_set(cmd, layout, set_index, set); }
        void bind_vertex_buffer(command_buffer cmd, int index, buffer_range range) override { return cmd_emulator.bind_vertex_buffer(cmd, index, range); }
        void bind_index_buffer(command_buffer cmd, buffer_range range) override { return cmd_emulator.bind_index_buffer(cmd, range); }
        void draw(command_buffer cmd, int first_vertex, int vertex_count) override { return cmd_emulator.draw(cmd, first_vertex, vertex_count); }
        void draw_indexed(command_buffer cmd, int first_index, int index_count) override { return cmd_emulator.draw_indexed(cmd, first_index, index_count); }
        void end_render_pass(command_buffer cmd) override { return cmd_emulator.end_render_pass(cmd); }

        void present(command_buffer submit, window window) override
        {
            submit_command_buffer(submit);
            glfwSwapBuffers(objects[window].w);
            cmd_emulator.destroy_command_buffer(submit);            
        }

        void wait_idle() override { glFlush(); }
    };

    autoregister_backend<gl_device> autoregister_gl_backend {"OpenGL 4.5 Core"};
}