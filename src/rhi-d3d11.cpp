#include "rhi.h"

#include <variant>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "../dep/SPIRV-Cross/spirv_hlsl.hpp"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace d3d
{
    struct buffer
    {

    };

    struct vertex_format
    {

    };

    struct shader
    {
        std::variant<ID3D11VertexShader *, ID3D11PixelShader *> shader_object;
    };

    struct pipeline
    {
        rhi::pipeline_desc desc;
    };

    struct window : glfw::window
    {
        window(GLFWwindow * w) : glfw::window{w} {}

        IDXGISwapChain * swap_chain;
        ID3D11RenderTargetView * render_target_view;
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
        pipeline * current_pipeline;

        ID3D11Device * dev;
        ID3D11DeviceContext * ctx;
        IDXGIFactory * factory;

        device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
        {
            auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
            if(FAILED(hr)) throw std::runtime_error("D3D11CreateDevice(...) failed");

            IDXGIDevice * dxgi_dev = nullptr;
            hr = dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi_dev);
            if(FAILED(hr)) throw std::runtime_error("IUnknown::QueryInterface(...) failed");

            IDXGIAdapter * adapter = nullptr;
            hr = dxgi_dev->GetAdapter(&adapter);
            if(FAILED(hr)) throw std::runtime_error("IDXGIDevice::GetAdapter(...) failed");

            hr = adapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory);
            if(FAILED(hr)) throw std::runtime_error("IDXGIAdapter::GetParent(...) failed");
        }

        glfw::window * create_window(const int2 & dimensions, std::string_view title) override
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_OPENGL_API, GLFW_NO_API);
            auto window = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);
            if(!window) throw std::runtime_error("glfwCreateWindow(...) failed");
            auto w = new d3d::window{window};

            DXGI_SWAP_CHAIN_DESC scd {};
            scd.BufferCount = 1;                                    // one back buffer
            scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
            scd.OutputWindow = glfwGetWin32Window(window);          // the window to be used
            scd.SampleDesc.Count = 1;
            scd.Windowed = TRUE;                                    // windowed/full-screen mode
            auto hr = factory->CreateSwapChain(dev, &scd, &w->swap_chain);
            if(FAILED(hr)) throw std::runtime_error("IDXGIFactory::CreateSwapChain(...) failed");

            ID3D11Resource * image;
            hr = w->swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&image);
            if(FAILED(hr)) throw std::runtime_error("IDXGISwapChain::GetBuffer(...) failed");

            hr = dev->CreateRenderTargetView(image, nullptr, &w->render_target_view);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRenderTargetView(...) failed");

            return w;
        }

        rhi::buffer_range create_static_buffer(binary_view contents) override
        {
            return {};
        }

        rhi::mapped_buffer_range create_dynamic_buffer(size_t size) override
        {
            rhi::mapped_buffer_range mapped;
            mapped.buffer = nullptr; // TODO
            mapped.offset = 0;
            mapped.size = size;
            mapped.memory = new char[size];
            return mapped;
        }

        template<class T> rhi::shader * create_shader(ID3DBlob * blob, HRESULT (__stdcall ID3D11Device::*create_func)(const void *, SIZE_T, ID3D11ClassLinkage *, T **))
        {
            T * s;
            auto hr = (dev->*create_func)(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::Create...Shader(...) failed");
            return out(new shader {s});
        }

        rhi::shader * create_shader(const shader_module & module) override
        {
            spirv_cross::CompilerHLSL::Options options;
            options.shader_model = 40;
            spirv_cross::CompilerHLSL compiler(module.spirv);
            compiler.set_options(options);
            const auto hlsl = compiler.compile();
            //debug_callback(hlsl.c_str());

            const char * target = 0;
            switch(module.stage)
            {
            case shader_stage::vertex: target = "vs_4_0"; break;
            case shader_stage::fragment: target = "ps_4_0"; break;
            }
            ID3DBlob * shader_blob, * error_blob;
            auto hr = D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", target, 0, 0, &shader_blob, &error_blob);
            if(FAILED(hr)) throw std::runtime_error("D3DCompile(...) failed");

            switch(module.stage)
            {
            case shader_stage::vertex: return create_shader(shader_blob, &ID3D11Device::CreateVertexShader);
            case shader_stage::fragment: return create_shader(shader_blob, &ID3D11Device::CreatePixelShader);
            default: throw std::logic_error("invalid shader_stage");
            }
        }

        rhi::pipeline * create_pipeline(const rhi::pipeline_desc & desc) override
        {
            return out(new pipeline{desc});
        }

        rhi::vertex_format * create_vertex_format(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            return out(new vertex_format{});
        }

        void begin_render_pass(glfw::window & window) override
        {
            FLOAT rgba[] {1,0,0,1};
            ctx->ClearRenderTargetView(in(window).render_target_view, rgba);
            ctx->OMSetRenderTargets(1, &in(window).render_target_view, nullptr);
        }

        void bind_pipeline(rhi::pipeline & pipe) override
        {
            //current_pipeline = in(&pipe);
            //glUseProgram(current_pipeline->program_object);
            //in(current_pipeline->desc.format)->bind_vertex_array();
            //glEnable(GL_DEPTH_TEST);
            //glDepthFunc(GL_NEVER | static_cast<int>(current_pipeline->desc.depth_test));
        }

        void bind_uniform_buffer(int index, rhi::buffer_range range) override
        {
            //glBindBufferRange(GL_UNIFORM_BUFFER, index, gl::in(range.buffer)->buffer_object, range.offset, range.size);
        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
            //for(auto & buf : in(current_pipeline->desc.format)->bindings)
            //{
            //    if(buf.index == index)
            //    {
            //        glBindVertexBuffer(index, gl::in(range.buffer)->buffer_object, range.offset, buf.stride);
            //    }
            //}        
        }

        void draw(int first_vertex, int vertex_count) override
        {
            //switch(current_pipeline->desc.topology)
            //{
            //case rhi::primitive_topology::points: glDrawArrays(GL_POINTS, first_vertex, vertex_count); break;
            //case rhi::primitive_topology::lines: glDrawArrays(GL_LINES, first_vertex, vertex_count); break;
            //case rhi::primitive_topology::triangles: glDrawArrays(GL_TRIANGLES, first_vertex, vertex_count); break;
            //}
        }

        void end_render_pass() override
        {

        }

        void present(glfw::window & window) override
        {
            in(window).swap_chain->Present(1, 0);
        }
    };
}

rhi::device * create_d3d11_device(std::function<void(const char *)> debug_callback)
{
    return new d3d::device(debug_callback);
}