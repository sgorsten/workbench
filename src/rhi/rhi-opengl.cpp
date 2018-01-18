#include "rhi-internal.h"

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

    struct gl_fence
    {
        GLsync sync_object = 0;
    };

    struct gl_buffer
    {
        GLuint buffer_object = 0;
        char * mapped = 0;
    };

    struct gl_image
    {
        image_desc desc;
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
        template<> struct traits<fence> { using type = gl_fence; };
        template<> struct traits<buffer> { using type = gl_buffer; };
        template<> struct traits<image> { using type = gl_image; };
        template<> struct traits<sampler> { using type = GLuint; };
        template<> struct traits<render_pass> { using type = gl_render_pass; };
        template<> struct traits<framebuffer> { using type = gl_framebuffer; };
        template<> struct traits<shader> { using type = shader_module; }; 
        template<> struct traits<pipeline> { using type = gl_pipeline; };
        template<> struct traits<window> { using type = gl_window; };
        heterogeneous_object_set<traits, fence, buffer, image, sampler, render_pass, framebuffer, shader, pipeline, window> objects;
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

        
        fence create_fence(bool signaled) override
        {
            auto [handle, f] = objects.create<fence>();
            if(signaled) f.sync_object = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            return handle;
        }
        void wait_for_fence(fence fence) override 
        { 
            auto & f = objects[fence];
            switch(glClientWaitSync(f.sync_object, 0, GL_TIMEOUT_IGNORED))
            {
            case GL_TIMEOUT_EXPIRED: case GL_WAIT_FAILED: throw std::runtime_error("glClientWaitSync(...) failed");
            }
            glDeleteSync(f.sync_object);
            f.sync_object = nullptr;
        }
        void destroy_fence(fence fence) override 
        {
            objects.destroy(fence);
        }

        image create_image(const image_desc & desc, std::vector<const void *> initial_data) override
        {
            auto glf = get_gl_format(desc.format);
            auto [handle, im] = objects.create<image>();
            im.desc = desc;
            switch(desc.shape)
            {
            case rhi::image_shape::_1d:
                glCreateTextures(GL_TEXTURE_1D, 1, &im.texture_object);
                glTextureStorage1D(im.texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x);
                if(initial_data.size() == 1) glTextureSubImage1D(im.texture_object, 0, 0, desc.dimensions.x, glf.format, glf.type, initial_data[0]);
                break;
            case rhi::image_shape::_2d:
                glCreateTextures(GL_TEXTURE_2D, 1, &im.texture_object);
                glTextureStorage2D(im.texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y);
                if(initial_data.size() == 1) glTextureSubImage2D(im.texture_object, 0, 0, 0, desc.dimensions.x, desc.dimensions.y, glf.format, glf.type, initial_data[0]);
                break;
            case rhi::image_shape::_3d:
                glCreateTextures(GL_TEXTURE_3D, 1, &im.texture_object);
                glTextureStorage3D(im.texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y, desc.dimensions.z);
                if(initial_data.size() == 1) glTextureSubImage3D(im.texture_object, 0, 0, 0, 0, desc.dimensions.x, desc.dimensions.y, desc.dimensions.z, glf.format, glf.type, initial_data[0]);
                break;
            case rhi::image_shape::cube:
                glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &im.texture_object);
                glTextureStorage2D(im.texture_object, desc.mip_levels, glf.internal_format, desc.dimensions.x, desc.dimensions.y);
                if(initial_data.size() == 6)
                {
                    GLuint view;
                    glGenTextures(1, &view);
                    glTextureView(view, GL_TEXTURE_2D_ARRAY, im.texture_object, glf.internal_format, 0, 1, 0, 6);
                    for(size_t i=0; i<6; ++i) glTextureSubImage3D(view, 0, 0, 0, exactly(i), desc.dimensions.x, desc.dimensions.y, 1, glf.format, glf.type, initial_data[i]);
                    glDeleteTextures(1, &view);
                }
                break;
            default: fail_fast();
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
                if(objects[desc.color_attachments[i].image].desc.shape == rhi::image_shape::cube) glNamedFramebufferTextureLayer(fb.framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), objects[desc.color_attachments[i].image].texture_object, desc.color_attachments[i].mip, desc.color_attachments[i].layer);
                else glNamedFramebufferTexture(fb.framebuffer_object, exactly(GL_COLOR_ATTACHMENT0+i), objects[desc.color_attachments[i].image].texture_object, desc.color_attachments[i].mip);
                draw_buffers.push_back(exactly(GL_COLOR_ATTACHMENT0+i));
            }
            if(desc.depth_attachment) 
            {
                if(objects[desc.depth_attachment->image].desc.shape == rhi::image_shape::cube) glNamedFramebufferTextureLayer(fb.framebuffer_object, GL_DEPTH_ATTACHMENT, objects[desc.depth_attachment->image].texture_object, desc.depth_attachment->mip, desc.depth_attachment->layer);
                else glNamedFramebufferTexture(fb.framebuffer_object, GL_DEPTH_ATTACHMENT, objects[desc.depth_attachment->image].texture_object, desc.depth_attachment->mip);
            }
            glNamedFramebufferDrawBuffers(fb.framebuffer_object, exactly(draw_buffers.size()), draw_buffers.data());
            return handle;
        }
        coord_system get_ndc_coords(framebuffer framebuffer) override { return {coord_axis::right, objects[framebuffer].framebuffer_object ? coord_axis::down : coord_axis::up, coord_axis::forward}; }
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
                [this](const generate_mipmaps_command & c)
                {
                    glGenerateTextureMipmap(objects[c.im].texture_object);
                },
                [this, &context](const begin_render_pass_command & c)
                {
                    auto & pass = objects[c.pass];
                    auto & fb = objects[c.framebuffer];
                    context = fb.context;
                    glfwMakeContextCurrent(fb.context);
                    glEnable(GL_FRAMEBUFFER_SRGB);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.framebuffer_object);
                    glViewport(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));

                    glClearColor(c.clear.color[0], c.clear.color[1], c.clear.color[2], c.clear.color[3]);
                    glClearDepthf(c.clear.depth);
                    glClearStencil(c.clear.stencil);

                    // Clear render targets if specified by render pass
                    for(size_t i=0; i<pass.desc.color_attachments.size(); ++i)
                    {
                        if(std::holds_alternative<clear>(pass.desc.color_attachments[i].load_op))
                        {
                            glClear(GL_COLOR_BUFFER_BIT); // TODO: Use glClearTexImage(...) when we use multiple color attachments                            
                        }
                    }
                    if(pass.desc.depth_attachment)
                    {
                        if(std::holds_alternative<clear>(pass.desc.depth_attachment->load_op))
                        {
                            glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                        }
                    }
                },
                [this, context](const bind_pipeline_command & c)
                {
                    auto & p = objects[c.pipe];
                    glUseProgram(p.program_object);
                    p.bind_vertex_array(context);
                    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

                    // Rasterizer state
                    switch(p.desc.front_face)
                    {
                    case rhi::front_face::counter_clockwise: glFrontFace(GL_CCW); break;
                    case rhi::front_face::clockwise: glFrontFace(GL_CW); break;
                    }
                    switch(p.desc.cull_mode)
                    {
                    case rhi::cull_mode::none: glDisable(GL_CULL_FACE); break;
                    case rhi::cull_mode::back: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
                    case rhi::cull_mode::front: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
                    }                   

                    // Depth stencil state
                    (p.desc.depth_test ? glEnable : glDisable)(GL_DEPTH_TEST);
                    if(p.desc.depth_test) glDepthFunc(GL_NEVER | static_cast<int>(*p.desc.depth_test));
                    glDepthMask(p.desc.depth_write ? GL_TRUE : GL_FALSE);

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

        command_buffer start_command_buffer() override { return cmd_emulator.start_command_buffer(); }
        void generate_mipmaps(command_buffer cmd, image image) override { cmd_emulator.generate_mipmaps(cmd, image); }
        void begin_render_pass(command_buffer cmd, render_pass pass, framebuffer framebuffer, const clear_values & clear) override { cmd_emulator.begin_render_pass(cmd, pass, framebuffer, clear); }
        void bind_pipeline(command_buffer cmd, pipeline pipe) override { return cmd_emulator.bind_pipeline(cmd, pipe); }
        void bind_descriptor_set(command_buffer cmd, pipeline_layout layout, int set_index, descriptor_set set) override { return cmd_emulator.bind_descriptor_set(cmd, layout, set_index, set); }
        void bind_vertex_buffer(command_buffer cmd, int index, buffer_range range) override { return cmd_emulator.bind_vertex_buffer(cmd, index, range); }
        void bind_index_buffer(command_buffer cmd, buffer_range range) override { return cmd_emulator.bind_index_buffer(cmd, range); }
        void draw(command_buffer cmd, int first_vertex, int vertex_count) override { return cmd_emulator.draw(cmd, first_vertex, vertex_count); }
        void draw_indexed(command_buffer cmd, int first_index, int index_count) override { return cmd_emulator.draw_indexed(cmd, first_index, index_count); }
        void end_render_pass(command_buffer cmd) override { return cmd_emulator.end_render_pass(cmd); }

        void submit(command_buffer submit) override
        {
            submit_command_buffer(submit);
        }
        void acquire_and_submit_and_present(command_buffer submit, window window, fence fence) override
        {
            submit_command_buffer(submit);
            glfwSwapBuffers(objects[window].w);
            if(fence) objects[fence].sync_object = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        }

        void wait_idle() override { glFlush(); }
    };

    autoregister_backend<gl_device> autoregister_gl_backend {"OpenGL 4.5 Core"};
}