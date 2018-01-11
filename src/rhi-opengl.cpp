#include "rhi-internal.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "../dep/SPIRV-Cross/spirv_glsl.hpp"
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

    struct shader
    {
        GLuint shader_object;
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

        object_set<rhi::input_layout, input_layout> input_layouts;
        object_set<rhi::shader, shader> shaders;
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
            if(glewInit() != GLEW_OK) throw std::runtime_error("glewInit() failed");
            if(debug_callback)
            {
                std::ostringstream ss;
                ss << "GL_VERSION = " << glGetString(GL_VERSION) << std::endl;
                ss << "GL_SHADING_LANGUAGE_VERSION = " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
                ss << "GL_VENDOR = " << glGetString(GL_VENDOR) << std::endl;
                ss << "GL_RENDERER = " << glGetString(GL_RENDERER) << std::endl;
                ss << "GLEW_VERSION = " << glewGetString(GLEW_VERSION) << std::endl;
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

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto [handle, input_layout] = input_layouts.create();
            input_layout.bindings = bindings;
            return handle;
        }

        rhi::shader create_shader(const shader_module & module) override
        {
            auto [handle, shader] = shaders.create();

            spirv_cross::CompilerGLSL compiler(module.spirv);
            const auto glsl = compiler.compile();
            const GLchar * source = glsl.c_str();
            GLint length = glsl.length();

            shader.shader_object = glCreateShader([&module]() 
            {
                switch(module.stage)
                {
                case shader_stage::vertex: return GL_VERTEX_SHADER;
                case shader_stage::fragment: return GL_FRAGMENT_SHADER;
                default: throw std::logic_error("unsupported shader_stage");
                }
            }());
            glShaderSource(shader.shader_object, 1, &source, &length);
            glCompileShader(shader.shader_object);

            GLint status;
            glGetShaderiv(shader.shader_object, GL_COMPILE_STATUS, &status);
            if(status == GL_FALSE)
            {
                glGetShaderiv(shader.shader_object, GL_INFO_LOG_LENGTH, &length);
                std::vector<char> buffer(length);
                glGetShaderInfoLog(shader.shader_object, buffer.size(), &length, buffer.data());
                throw std::runtime_error(buffer.data());
            }
            return handle;
        }

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto program = glCreateProgram();
            for(auto shader : desc.stages) glAttachShader(program, shaders[shader].shader_object);
            glLinkProgram(program);

            GLint status, length;
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if(status == GL_FALSE)
            {
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                std::vector<char> buffer(length);
                glGetProgramInfoLog(program, buffer.size(), &length, buffer.data());
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

        void bind_uniform_buffer(int index, rhi::buffer_range range) override
        {
            glBindBufferRange(GL_UNIFORM_BUFFER, index, buffers[range.buffer].buffer_object, range.offset, range.size);
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
    };
}

rhi::device * create_opengl_device(std::function<void(const char *)> debug_callback)
{
    return new gl::device(debug_callback);
}