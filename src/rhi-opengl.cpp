#include "rhi.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include "../dep/SPIRV-Cross/spirv_glsl.hpp"
#pragma comment(lib, "opengl32.lib")

namespace gl
{
    struct buffer
    {
        GLuint buffer_object;
    };

    struct vertex_format
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

    struct window : glfw::window
    {
        GLFWwindow * w;
        window(GLFWwindow * w) : glfw::window{w}, w{w} {}
    };

    template<class T> struct interface_type;
    template<class T> struct implementation_type;

    #define CONNECT_TYPES(INTERFACE, IMPLEMENTATION) template<> struct interface_type<IMPLEMENTATION> { using type = INTERFACE; }; template<> struct implementation_type<INTERFACE> { using type = IMPLEMENTATION; }
    CONNECT_TYPES(rhi::buffer, buffer);
    CONNECT_TYPES(rhi::vertex_format, vertex_format);
    CONNECT_TYPES(rhi::shader, shader);
    CONNECT_TYPES(rhi::pipeline, pipeline);
    CONNECT_TYPES(glfw::window, window);
    #undef CONNECT_TYPES

    template<class T> typename interface_type<T>::type * out(T * ptr) { return reinterpret_cast<typename interface_type<T>::type *>(ptr); }
    template<class T> typename implementation_type<T>::type * in(T * ptr) { return reinterpret_cast<typename implementation_type<T>::type *>(ptr); }
    template<class T> typename implementation_type<T>::type & in(T & ref) { return reinterpret_cast<typename implementation_type<T>::type &>(ref); }
    template<class T> const typename interface_type<T>::type * out(const T * ptr) { return reinterpret_cast<const typename interface_type<T>::type *>(ptr); }
    template<class T> const typename implementation_type<T>::type * in(const T * ptr) { return reinterpret_cast<const typename implementation_type<T>::type *>(ptr); }
    template<class T> const typename implementation_type<T>::type & in(const T & ref) { return reinterpret_cast<const typename implementation_type<T>::type &>(ref); }   

    struct device : rhi::device
    {
        std::function<void(const char *)> debug_callback;
        GLFWwindow * hidden_window;
        pipeline * current_pipeline;

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

        glfw::window * create_window(const int2 & dimensions, std::string_view title) override
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);
            auto window = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, hidden_window);
            if(!window) throw std::runtime_error("glfwCreateWindow(...) failed");
            enable_debug_callback(window);
            return new gl::window{window};
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
            return out(new gl::vertex_format{bindings});
        }

        void begin_render_pass(glfw::window & window) override
        {
            const int2 fb_size = window.get_framebuffer_size();
            glfwMakeContextCurrent(in(window).w);
            glViewport(0, 0, fb_size.x, fb_size.y);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        void bind_pipeline(rhi::pipeline & pipe) override
        {
            current_pipeline = in(&pipe);
            glUseProgram(current_pipeline->program_object);
            in(current_pipeline->desc.format)->bind_vertex_array();
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
            glfwSwapBuffers(in(window).w);
        }
    };
}

rhi::device * create_opengl_device(std::function<void(const char *)> debug_callback)
{
    return new gl::device(debug_callback);
}