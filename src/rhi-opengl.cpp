#include "rhi.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include "../dep/SPIRV-Cross/spirv_glsl.hpp"
#include <iostream>

namespace gl
{
    struct buffer
    {
        GLuint buffer_object;
    };

    struct vertex_format
    {
        GLuint vertex_array_object;
        std::vector<rhi::vertex_binding_desc> bindings;
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

    template<class T> struct interface_type;
    template<class T> struct implementation_type;

    #define CONNECT_TYPES(T) template<> struct interface_type<T> { using type = rhi::T; }; template<> struct implementation_type<rhi::T> { using type = T; }
    CONNECT_TYPES(buffer);
    CONNECT_TYPES(vertex_format);
    CONNECT_TYPES(shader);
    CONNECT_TYPES(pipeline);
    #undef CONNECT_TYPES

    template<class T> typename interface_type<T>::type * out(T * ptr) { return reinterpret_cast<typename interface_type<T>::type *>(ptr); }
    template<class T> typename implementation_type<T>::type * in(T * ptr) { return reinterpret_cast<typename implementation_type<T>::type *>(ptr); }
    template<class T> typename implementation_type<T>::type & in(T & ref) { return reinterpret_cast<typename implementation_type<T>::type &>(ref); }
    template<class T> const typename interface_type<T>::type * out(const T * ptr) { return reinterpret_cast<const typename interface_type<T>::type *>(ptr); }
    template<class T> const typename implementation_type<T>::type * in(const T * ptr) { return reinterpret_cast<const typename implementation_type<T>::type *>(ptr); }
    template<class T> const typename implementation_type<T>::type & in(const T & ref) { return reinterpret_cast<const typename implementation_type<T>::type &>(ref); }   

    struct device : rhi::device
    {
        GLFWwindow * hidden_window;
        pipeline * current_pipeline;

        device()
        {
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_VISIBLE, 0);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    
            glfwMakeContextCurrent(hidden_window);
            glewInit();
            std::cout << "GL_VERSION = " << glGetString(GL_VERSION) << std::endl;
            std::cout << "GL_SHADING_LANGUAGE_VERSION = " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
            std::cout << "GL_VENDOR = " << glGetString(GL_VENDOR) << std::endl;
            std::cout << "GL_RENDERER = " << glGetString(GL_RENDERER) << std::endl;
        }

        glfw::window * create_window(const int2 & dimensions, std::string_view title) override
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            return new glfw::window{glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, hidden_window)};
        }

        rhi::buffer_range create_static_buffer(binary_view contents) override
        {
            GLuint buffer;
            glCreateBuffers(1, &buffer);
            glNamedBufferStorage(buffer, contents.size, contents.data, 0);
            return {out(new gl::buffer{buffer}), 0, contents.size};
        }

        rhi::mapped_buffer_range create_dynamic_buffer(size_t size) override
        {
            GLuint buffer;
            glCreateBuffers(1, &buffer);
            glNamedBufferStorage(buffer, size, nullptr, GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);

            rhi::mapped_buffer_range mapped;
            mapped.buffer = out(new gl::buffer{buffer});
            mapped.offset = 0;
            mapped.size = size;
            mapped.memory = reinterpret_cast<char *>(glMapNamedBuffer(buffer, GL_WRITE_ONLY));
            return mapped;
        }

        rhi::shader * create_shader(const shader_module & module) override
        {
            spirv_cross::CompilerGLSL compiler(module.spirv);
            const auto glsl = compiler.compile();
            const GLchar * source = glsl.c_str();
            GLint length = glsl.length();

            auto shader = glCreateShader([&module]() 
            {
                switch(module.stage)
                {
                case shader_stage::vertex: return GL_VERTEX_SHADER;
                case shader_stage::fragment: return GL_FRAGMENT_SHADER;
                default: throw std::logic_error("unsupported shader_stage");
                }
            }());
            glShaderSource(shader, 1, &source, &length);
            glCompileShader(shader);

            GLint status;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
            if(status == GL_FALSE)
            {
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                std::vector<char> buffer(length);
                glGetShaderInfoLog(shader, buffer.size(), &length, buffer.data());
                throw std::runtime_error(buffer.data());
            }
            return out(new gl::shader{shader});
        }

        rhi::pipeline * create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto program = glCreateProgram();
            for(auto shader : desc.stages) glAttachShader(program, gl::in(shader)->shader_object);
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
            return out(new gl::pipeline{desc, program});
        }

        rhi::vertex_format * create_vertex_format(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            GLuint vertex_array;
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
            return out(new gl::vertex_format{vertex_array, bindings});
        }

        void begin_render_pass(glfw::window & window) override
        {
            const int2 fb_size = window.get_framebuffer_size();
            window.make_context_current();
            glViewport(0, 0, fb_size.x, fb_size.y);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void bind_pipeline(rhi::pipeline & pipe) override
        {
            current_pipeline = in(&pipe);
            glUseProgram(current_pipeline->program_object);
            glBindVertexArray(in(current_pipeline->desc.format)->vertex_array_object);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_NEVER | static_cast<int>(current_pipeline->desc.depth_test));
        }

        void bind_uniform_buffer(int index, rhi::buffer_range range) override
        {
            glBindBufferRange(GL_UNIFORM_BUFFER, index, gl::in(range.buffer)->buffer_object, range.offset, range.size);
        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
            for(auto & buf : in(current_pipeline->desc.format)->bindings)
            {
                if(buf.index == index)
                {
                    glBindVertexBuffer(index, gl::in(range.buffer)->buffer_object, range.offset, buf.stride);
                }
            }        
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

        void end_render_pass() override
        {

        }

        void present(glfw::window & window) override
        {
            window.swap_buffers();
        }
    };
}

rhi::device * create_opengl_device()
{
    return new gl::device();
}