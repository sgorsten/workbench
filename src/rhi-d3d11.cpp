#include "rhi.h"

#include <variant>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include "../dep/SPIRV-Cross/spirv_hlsl.hpp"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace d3d
{
    struct buffer
    {
        ID3D11Buffer * buffer_object;
    };

    struct vertex_format
    {
        std::vector<rhi::vertex_binding_desc> bindings;
        std::vector<D3D11_INPUT_ELEMENT_DESC> input_descs;        
    };

    struct shader
    {
        ID3DBlob * blob;
        std::variant<ID3D11VertexShader *, ID3D11PixelShader *> shader_object;
    };

    struct pipeline
    {
        rhi::pipeline_desc desc;
        ID3D11InputLayout * layout;
        ID3D11VertexShader * vs;
        ID3D11PixelShader * ps;
        void set_shader(ID3D11VertexShader * s) { vs = s; }
        void set_shader(ID3D11PixelShader * s) { ps = s; }
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

        ID3D11Device1 * dev;
        ID3D11DeviceContext1 * ctx;
        IDXGIFactory * factory;
        ID3D11RasterizerState * rasterizer_state;

        device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
        {
            const D3D_FEATURE_LEVEL feature_levels[] {D3D_FEATURE_LEVEL_11_1};
            ID3D11Device * dev11;
            ID3D11DeviceContext * ctx11;
            auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, feature_levels, 1, D3D11_SDK_VERSION, &dev11, nullptr, &ctx11);
            if(FAILED(hr)) throw std::runtime_error("D3D11CreateDevice(...) failed");

            hr = dev11->QueryInterface(__uuidof(ID3D11Device1), (void **)&dev);
            if(FAILED(hr)) throw std::runtime_error("IUnknown::QueryInterface(...) failed");

            hr = ctx11->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&ctx);
            if(FAILED(hr)) throw std::runtime_error("IUnknown::QueryInterface(...) failed");
            
            IDXGIDevice * dxgi_dev = nullptr;
            hr = dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi_dev);
            if(FAILED(hr)) throw std::runtime_error("IUnknown::QueryInterface(...) failed");

            IDXGIAdapter * adapter = nullptr;
            hr = dxgi_dev->GetAdapter(&adapter);
            if(FAILED(hr)) throw std::runtime_error("IDXGIDevice::GetAdapter(...) failed");

            hr = adapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory);
            if(FAILED(hr)) throw std::runtime_error("IDXGIAdapter::GetParent(...) failed");

            D3D11_RASTERIZER_DESC rdesc {};
            rdesc.FillMode = D3D11_FILL_SOLID;
            rdesc.CullMode = D3D11_CULL_NONE;
            rdesc.FrontCounterClockwise = FALSE;
            hr = dev->CreateRasterizerState(&rdesc, &rasterizer_state);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRasterizerState(...) failed");            
        }

        coord_system get_ndc_coords() const { return {coord_axis::right, coord_axis::up, coord_axis::forward}; }
        linalg::z_range get_z_range() const { return linalg::zero_to_one; }

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
            D3D11_BUFFER_DESC buffer_desc {};
            buffer_desc.ByteWidth = contents.size;
            buffer_desc.Usage = D3D11_USAGE_IMMUTABLE; //D3D11_USAGE_DYNAMIC
            buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // D3D11_BIND_CONSTANT_BUFFER
            buffer_desc.CPUAccessFlags = 0; // D3D11_CPU_ACCESS_WRITE

            D3D11_SUBRESOURCE_DATA data {};
            data.pSysMem = contents.data;

            auto buf = new buffer;
            auto hr = dev->CreateBuffer(&buffer_desc, &data, &buf->buffer_object);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateBuffer(...) failed");
            return {out(buf), 0, contents.size};
        }

        rhi::mapped_buffer_range create_dynamic_buffer(size_t size) override
        {
            D3D11_BUFFER_DESC buffer_desc {};
            buffer_desc.ByteWidth = size;
            buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
            buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            auto buf = new buffer;
            auto hr = dev->CreateBuffer(&buffer_desc, nullptr, &buf->buffer_object);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateBuffer(...) failed");

            D3D11_MAPPED_SUBRESOURCE sub;
            hr = ctx->Map(buf->buffer_object, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
            if(FAILED(hr)) throw std::runtime_error("ID3D11DeviceContext::Map(...) failed");

            rhi::mapped_buffer_range mapped;
            mapped.buffer = out(buf);
            mapped.offset = 0;
            mapped.size = size;
            mapped.memory = reinterpret_cast<char *>(sub.pData);
            return mapped;
        }

        template<class T> rhi::shader * create_shader(ID3DBlob * blob, HRESULT (__stdcall ID3D11Device::*create_func)(const void *, SIZE_T, ID3D11ClassLinkage *, T **))
        {
            T * s;
            auto hr = (dev->*create_func)(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::Create...Shader(...) failed");
            return out(new shader {blob, s});
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
            auto hr = D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", target, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0, &shader_blob, &error_blob);
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
            auto pipe = new pipeline{desc};
            for(auto s : desc.stages) 
            {
                std::visit([pipe](auto * s) { pipe->set_shader(s); }, in(s)->shader_object);
                if(std::holds_alternative<ID3D11VertexShader*>(in(s)->shader_object))
                {
                    auto hr = dev->CreateInputLayout(in(desc.format)->input_descs.data(), in(desc.format)->input_descs.size(), in(s)->blob->GetBufferPointer(), in(s)->blob->GetBufferSize(), &pipe->layout);
                    if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateInputLayout(...) failed");
                }
            }
            return out(pipe);
        }

        rhi::vertex_format * create_vertex_format(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto fmt = new vertex_format{bindings};
            for(auto & buf : bindings)
            {
                for(auto & attrib : buf.attributes)
                {
                    switch(attrib.type)
                    {
                    case rhi::attribute_format::float1: fmt->input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float2: fmt->input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float3: fmt->input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float4: fmt->input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32A32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    default: throw std::logic_error("invalid rhi::attribute_format");
                    }
                }
            }
            return out(fmt);
        }

        void begin_render_pass(glfw::window & window) override
        {
            ctx->ClearState();
            ctx->RSSetState(rasterizer_state);

            FLOAT rgba[] {0,0,0,1};
            ctx->ClearRenderTargetView(in(window).render_target_view, rgba);
            ctx->OMSetRenderTargets(1, &in(window).render_target_view, nullptr);
            D3D11_VIEWPORT vp {0, 0, 512, 512, 0, 1};
            ctx->RSSetViewports(1, &vp);
        }

        void bind_pipeline(rhi::pipeline & pipe) override
        {
            current_pipeline = in(&pipe);
            ctx->IASetInputLayout(current_pipeline->layout);
            ctx->VSSetShader(current_pipeline->vs, nullptr, 0);
            ctx->PSSetShader(current_pipeline->ps, nullptr, 0);            
        }

        void bind_uniform_buffer(int index, rhi::buffer_range range) override
        {
            const UINT first_constant = range.offset/16, num_constants = (range.size+255)/256*16;
            ctx->VSSetConstantBuffers1(index, 1, &in(range.buffer)->buffer_object, &first_constant, &num_constants);
            ctx->PSSetConstantBuffers1(index, 1, &in(range.buffer)->buffer_object, &first_constant, &num_constants);
        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
            for(auto & buf : in(current_pipeline->desc.format)->bindings)
            {
                if(buf.index == index)
                {
                    const UINT stride = buf.stride, offset = range.offset;
                    ctx->IASetVertexBuffers(index, 1, &in(range.buffer)->buffer_object, &stride, &offset);
                }
            }
        }

        void draw(int first_vertex, int vertex_count) override
        {
            switch(current_pipeline->desc.topology)
            {
            case rhi::primitive_topology::points: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); break;
            case rhi::primitive_topology::lines: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); break;
            case rhi::primitive_topology::triangles: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); break;
            }
            ctx->Draw(vertex_count, first_vertex);
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