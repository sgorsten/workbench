#include "rhi-internal.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../../dep/SPIRV-Cross/spirv_glsl.hpp"
#pragma comment(lib, "opengl32.lib")

namespace gl
{
    struct buffer
    {
        GLuint buffer_object = 0;
        char * mapped = 0;
    };

    struct render_pass
    {
        rhi::render_pass_desc desc;
    };

    struct framebuffer
    {
        GLFWwindow * context;
        GLuint framebuffer_object;
        int2 dims;
    };

    struct input_layout
    {
        std::vector<rhi::vertex_binding_desc> bindings;
        mutable std::unordered_map<GLFWwindow *, GLuint> vertex_array_objects; // vertex array objects cannot be shared between OpenGL contexts, so we must cache them per-context
    
        void bind_vertex_array() const
        {
            auto & vertex_array = vertex_array_objects[glfwGetCurrentContext()];
            if(!vertex_array)
            {
                // If vertex array object was not yet created in this context, go ahead and generate it
                glCreateVertexArrays(1, &vertex_array);
                for(auto & buf : bindings)
                {
                    for(auto & attrib : buf.attributes)
                    {
                        glEnableVertexArrayAttrib(vertex_array, attrib.index);
                        glVertexArrayAttribBinding(vertex_array, attrib.index, buf.index);
                        switch(attrib.type)
                        {
                        case rhi::attribute_format::float1: glVertexArrayAttribFormat(vertex_array, attrib.index, 1, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case rhi::attribute_format::float2: glVertexArrayAttribFormat(vertex_array, attrib.index, 2, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case rhi::attribute_format::float3: glVertexArrayAttribFormat(vertex_array, attrib.index, 3, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        case rhi::attribute_format::float4: glVertexArrayAttribFormat(vertex_array, attrib.index, 4, GL_FLOAT, GL_FALSE, attrib.offset); break;
                        }                
                    }
                }
            }
            glBindVertexArray(vertex_array);
        }
    };
    
    struct pipeline
    {
        rhi::pipeline_desc desc;
        GLuint program_object;
    };

    struct window
    {
        GLFWwindow * w;
        rhi::framebuffer fb;
    };

    struct device : rhi::device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        pipeline * current_pipeline=0;
        size_t index_buffer_offset=0;

        template<class T> struct traits;
        template<> struct traits<rhi::buffer> { using type = buffer; };
        template<> struct traits<rhi::render_pass> { using type = render_pass; };
        template<> struct traits<rhi::framebuffer> { using type = framebuffer; };
        template<> struct traits<rhi::input_layout> { using type = input_layout; };
        template<> struct traits<rhi::shader> { using type = shader_module; }; 
        template<> struct traits<rhi::pipeline> { using type = pipeline; };
        template<> struct traits<rhi::window> { using type = window; };
        heterogeneous_object_set<traits, rhi::buffer, rhi::render_pass, rhi::framebuffer, rhi::input_layout, rhi::shader, rhi::pipeline, rhi::window> objects;
        descriptor_emulator desc_emulator;
        command_emulator cmd_emulator;

        void enable_debug_callback(GLFWwindow * window)
        {
            glfwMakeContextCurrent(window);
            if(debug_callback)
            {
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * message, const void * user) { reinterpret_cast<const device *>(user)->debug_callback(message); }, this);
                const GLuint ids = 0;
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &ids, true);
            }
        }

        device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
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
        
        rhi::device_info get_info() const override { return {"OpenGL 4.5 Core", {coord_axis::right, coord_axis::up, coord_axis::forward}, linalg::neg_one_to_one}; }

        rhi::render_pass create_render_pass(const rhi::render_pass_desc & desc) override 
        {
            auto [handle, pass] = objects.create<rhi::render_pass>();
            pass.desc = desc;
            return handle;
        }
        void destroy_render_pass(rhi::render_pass pass) override { objects.destroy(pass); }

        rhi::window create_window(rhi::render_pass pass, const int2 & dimensions, std::string_view title) override
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            auto [handle, window] = objects.create<rhi::window>();
            window.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, hidden_window);
            if(!window.w) throw std::runtime_error("glfwCreateWindow(...) failed");
            enable_debug_callback(window.w);

            auto [fb_handle, fb] = objects.create<rhi::framebuffer>();
            fb.context = window.w;
            fb.framebuffer_object = 0;
            fb.dims = dimensions;
            window.fb = fb_handle;
            return {handle};
        }
        GLFWwindow * get_glfw_window(rhi::window window) override { return objects[window].w; }
        rhi::framebuffer get_swapchain_framebuffer(rhi::window window) override { return objects[window].fb; }

        rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) override { return desc_emulator.create_descriptor_set_layout(bindings); }
        rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) override { return desc_emulator.create_pipeline_layout(sets); }
        rhi::descriptor_pool create_descriptor_pool() { return desc_emulator.create_descriptor_pool(); }
        void reset_descriptor_pool(rhi::descriptor_pool pool) { desc_emulator.reset_descriptor_pool(pool); }
        rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) { return desc_emulator.alloc_descriptor_set(pool, layout); }
        void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) { desc_emulator.write_descriptor(set, binding, range); }

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto [handle, layout] = objects.create<rhi::input_layout>();
            layout.bindings = bindings;
            return handle;
        }

        rhi::shader create_shader(const shader_module & module) override
        {
            auto [handle, shader] = objects.create<rhi::shader>();
            shader = module;
            return handle;
        }

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto program = glCreateProgram();
            for(auto s : desc.stages)
            {
                auto & shader = objects[s];
                spirv_cross::CompilerGLSL compiler(shader.spirv);
	            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	            for(auto & resource : resources.uniform_buffers)
	            {
		            const unsigned set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
		            const unsigned binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
		            compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(desc_emulator.get_flat_buffer_binding(desc.layout, set, binding)));
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

            auto [handle, pipeline] = objects.create<rhi::pipeline>();
            pipeline.desc = desc;
            pipeline.program_object = program;
            return handle;
        }

        std::tuple<rhi::buffer, char *> create_buffer(const rhi::buffer_desc & desc, const void * initial_data) override
        {
            auto [handle, buffer] = objects.create<rhi::buffer>();
            glCreateBuffers(1, &buffer.buffer_object);
            GLbitfield flags = 0;
            if(desc.dynamic) flags |= GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT;
            glNamedBufferStorage(buffer.buffer_object, desc.size, initial_data, flags);
            if(desc.dynamic) buffer.mapped = reinterpret_cast<char *>(glMapNamedBuffer(buffer.buffer_object, GL_WRITE_ONLY));
            return {handle, buffer.mapped};
        }

        void destroy_buffer(rhi::buffer buffer) override { objects.destroy(buffer); }
        void destroy_descriptor_pool(rhi::descriptor_pool pool) override { desc_emulator.destroy(pool); }
        void destroy_descriptor_set_layout(rhi::descriptor_set_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_pipeline_layout(rhi::pipeline_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_input_layout(rhi::input_layout layout) override { objects.destroy(layout); }
        void destroy_shader(rhi::shader shader) override { objects.destroy(shader); }
        void destroy_pipeline(rhi::pipeline pipeline)  override { objects.destroy(pipeline); }       
        void destroy_window(rhi::window window) override { objects.destroy(window); }

        void submit_command_buffer(rhi::command_buffer cmd)
        {
            cmd_emulator.execute(cmd, overload(
                [this](const begin_render_pass_command & c)
                {
                    auto & fb = objects[c.framebuffer];
                    glfwMakeContextCurrent(fb.context);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.framebuffer_object);
                    glViewport(0, 0, exactly(fb.dims.x), exactly(fb.dims.y));
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                },
                [this](const bind_pipeline_command & c)
                {
                    auto & p = objects[c.pipe];
                    glUseProgram(p.program_object);
                    objects[p.desc.input].bind_vertex_array();
                    (p.desc.depth_test ? glEnable : glDisable)(GL_DEPTH_TEST);
                    if(p.desc.depth_test) glDepthFunc(GL_NEVER | static_cast<int>(*p.desc.depth_test));
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    current_pipeline = &p;
                },
                [this](const bind_descriptor_set_command & c)
                {
                    desc_emulator.bind_descriptor_set(c.layout, c.set_index, c.set, [this](size_t index, rhi::buffer_range range)
                    {
                        glBindBufferRange(GL_UNIFORM_BUFFER, exactly(index), objects[range.buffer].buffer_object, range.offset, range.size);
                    });
                },
                [this](const bind_vertex_buffer_command & c)
                {
                    for(auto & buf : objects[current_pipeline->desc.input].bindings)
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
                    case rhi::primitive_topology::points: glDrawArrays(GL_POINTS, c.first_vertex, c.vertex_count); break;
                    case rhi::primitive_topology::lines: glDrawArrays(GL_LINES, c.first_vertex, c.vertex_count); break;
                    case rhi::primitive_topology::triangles: glDrawArrays(GL_TRIANGLES, c.first_vertex, c.vertex_count); break;
                    }
                },
                [this](const draw_indexed_command & c)
                {
                    auto indices = (const void *)(index_buffer_offset + c.first_index*sizeof(uint32_t));
                    switch(current_pipeline->desc.topology)
                    {
                    case rhi::primitive_topology::points: glDrawElements(GL_POINTS, c.index_count, GL_UNSIGNED_INT, indices); break;
                    case rhi::primitive_topology::lines: glDrawElements(GL_LINES, c.index_count, GL_UNSIGNED_INT, indices); break;
                    case rhi::primitive_topology::triangles: glDrawElements(GL_TRIANGLES, c.index_count, GL_UNSIGNED_INT, indices); break;
                    }
                },
                [this](const end_render_pass_command &) {}
            ));
        }

        rhi::command_buffer start_command_buffer() override { return cmd_emulator.create_command_buffer(); }
        void begin_render_pass(rhi::command_buffer cmd, rhi::render_pass pass, rhi::framebuffer framebuffer) override { cmd_emulator.begin_render_pass(cmd, pass, framebuffer); }
        void bind_pipeline(rhi::command_buffer cmd, rhi::pipeline pipe) override { return cmd_emulator.bind_pipeline(cmd, pipe); }
        void bind_descriptor_set(rhi::command_buffer cmd, rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) override { return cmd_emulator.bind_descriptor_set(cmd, layout, set_index, set); }
        void bind_vertex_buffer(rhi::command_buffer cmd, int index, rhi::buffer_range range) override { return cmd_emulator.bind_vertex_buffer(cmd, index, range); }
        void bind_index_buffer(rhi::command_buffer cmd, rhi::buffer_range range) override { return cmd_emulator.bind_index_buffer(cmd, range); }
        void draw(rhi::command_buffer cmd, int first_vertex, int vertex_count) override { return cmd_emulator.draw(cmd, first_vertex, vertex_count); }
        void draw_indexed(rhi::command_buffer cmd, int first_index, int index_count) override { return cmd_emulator.draw_indexed(cmd, first_index, index_count); }
        void end_render_pass(rhi::command_buffer cmd) override { return cmd_emulator.end_render_pass(cmd); }

        void present(rhi::command_buffer submit, rhi::window window) override
        {
            submit_command_buffer(submit);
            glfwSwapBuffers(objects[window].w);
            cmd_emulator.destroy_command_buffer(submit);            
        }

        void wait_idle() override { glFlush(); }
    };
}

std::shared_ptr<rhi::device> create_opengl_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<gl::device>(debug_callback);
}