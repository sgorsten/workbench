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
    };

    struct device : rhi::device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        pipeline * current_pipeline=0;
        size_t index_buffer_offset=0;

        descriptor_set_emulator desc_emulator;
        object_set<rhi::input_layout, input_layout> input_layouts;
        object_set<rhi::shader, shader_module> shaders;
        object_set<rhi::pipeline, pipeline> pipelines;
        object_set<rhi::buffer, buffer> buffers;
        object_set<rhi::window, window> windows;

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

        std::tuple<rhi::window, GLFWwindow *> create_window(const int2 & dimensions, std::string_view title) override 
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            auto [handle, window] = windows.create();
            window.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, hidden_window);
            if(!window.w) throw std::runtime_error("glfwCreateWindow(...) failed");
            enable_debug_callback(window.w);
            return {handle, window.w};
        }

        rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) override { return desc_emulator.create_descriptor_set_layout(bindings); }
        rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) override { return desc_emulator.create_pipeline_layout(sets); }
        rhi::descriptor_pool create_descriptor_pool() { return desc_emulator.create_descriptor_pool(); }
        void reset_descriptor_pool(rhi::descriptor_pool pool) { desc_emulator.reset_descriptor_pool(pool); }
        rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) { return desc_emulator.alloc_descriptor_set(pool, layout); }
        void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) { desc_emulator.write_descriptor(set, binding, range); }

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto [handle, layout] = input_layouts.create();
            layout.bindings = bindings;
            return handle;
        }

        rhi::shader create_shader(const shader_module & module) override
        {
            auto [handle, shader] = shaders.create();
            shader = module;
            return handle;
        }

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto program = glCreateProgram();
            for(auto s : desc.stages)
            {
                auto & shader = shaders[s];
                spirv_cross::CompilerGLSL compiler(shader.spirv);
	            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	            for(auto & resource : resources.uniform_buffers)
	            {
		            const unsigned set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
		            const unsigned binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
		            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
		            compiler.set_decoration(resource.id, spv::DecorationBinding, desc_emulator.get_flat_buffer_binding(desc.layout, set, binding));
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

            auto [handle, pipeline] = pipelines.create();
            pipeline.desc = desc;
            pipeline.program_object = program;
            return handle;
        }

        std::tuple<rhi::buffer, char *> create_buffer(const rhi::buffer_desc & desc, const void * initial_data) override
        {
            auto [handle, buffer] = buffers.create();
            glCreateBuffers(1, &buffer.buffer_object);
            GLbitfield flags = 0;
            if(desc.dynamic) flags |= GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT;
            glNamedBufferStorage(buffer.buffer_object, desc.size, initial_data, flags);
            if(desc.dynamic) buffer.mapped = reinterpret_cast<char *>(glMapNamedBuffer(buffer.buffer_object, GL_WRITE_ONLY));
            return {handle, buffer.mapped};
        }

        void destroy_descriptor_set_layout(rhi::descriptor_set_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_pipeline_layout(rhi::pipeline_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_descriptor_pool(rhi::descriptor_pool pool) override { desc_emulator.destroy(pool); }
        void destroy_input_layout(rhi::input_layout layout) override { input_layouts.destroy(layout); }
        void destroy_shader(rhi::shader shader) override { shaders.destroy(shader); }
        void destroy_pipeline(rhi::pipeline pipeline)  override { pipelines.destroy(pipeline); }
        void destroy_buffer(rhi::buffer buffer) override { buffers.destroy(buffer); }
        void destroy_window(rhi::window window) override { windows.destroy(window); }

        void begin_render_pass(rhi::window window) override
        {
            auto w = windows[window].w;
            int width, height;
            glfwGetFramebufferSize(w, &width, &height);            
            glfwMakeContextCurrent(w);
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void bind_pipeline(rhi::pipeline pipe) override
        {
            current_pipeline = &pipelines[pipe];
            glUseProgram(current_pipeline->program_object);
            input_layouts[current_pipeline->desc.input].bind_vertex_array();
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_NEVER | static_cast<int>(current_pipeline->desc.depth_test));
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }

        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) override
        {
            desc_emulator.bind_descriptor_set(layout, set_index, set, [this](size_t index, rhi::buffer_range range)
            {
                glBindBufferRange(GL_UNIFORM_BUFFER, exactly(index), buffers[range.buffer].buffer_object, range.offset, range.size);
            });
        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
            for(auto & buf : input_layouts[current_pipeline->desc.input].bindings)
            {
                if(buf.index == index)
                {
                    glBindVertexBuffer(index, buffers[range.buffer].buffer_object, range.offset, buf.stride);
                }
            }        
        }

        void bind_index_buffer(rhi::buffer_range range) override
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[range.buffer].buffer_object);
            index_buffer_offset = range.offset;
        }

        void draw(int first_vertex, int vertex_count) override
        {
            switch(current_pipeline->desc.topology)
            {
            case rhi::primitive_topology::points: glDrawArrays(GL_POINTS, first_vertex, vertex_count); break;
            case rhi::primitive_topology::lines: glDrawArrays(GL_LINES, first_vertex, vertex_count); break;
            case rhi::primitive_topology::triangles: glDrawArrays(GL_TRIANGLES, first_vertex, vertex_count); break;
            }
        }

        void draw_indexed(int first_index, int index_count) override
        {
            auto indices = (const void *)(index_buffer_offset + first_index*sizeof(uint32_t));
            switch(current_pipeline->desc.topology)
            {
            case rhi::primitive_topology::points: glDrawElements(GL_POINTS, index_count, GL_UNSIGNED_INT, indices); break;
            case rhi::primitive_topology::lines: glDrawElements(GL_LINES, index_count, GL_UNSIGNED_INT, indices); break;
            case rhi::primitive_topology::triangles: glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, indices); break;
            }
        }

        void end_render_pass() override
        {

        }

        void present(rhi::window window) override
        {
            glfwSwapBuffers(windows[window].w);
        }

        void wait_idle() override { glFlush(); }
    };
}

std::shared_ptr<rhi::device> create_opengl_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<gl::device>(debug_callback);
}