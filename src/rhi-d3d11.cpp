#include "rhi-internal.h"

#include <variant>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
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
        char * mapped = 0;
    };

    struct descriptor_set_layout
    {
        std::vector<rhi::descriptor_binding> bindings;
    };

    struct input_layout
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

    struct window
    {
        GLFWwindow * w;

        IDXGISwapChain * swap_chain;
        ID3D11RenderTargetView * render_target_view;

        ID3D11Texture2D * depth_texture;
        ID3D11DepthStencilView * depth_stencil_view;
    };

    struct device : rhi::device
    {
        std::function<void(const char *)> debug_callback;    

        ID3D11Device1 * dev;
        ID3D11DeviceContext1 * ctx;
        IDXGIFactory * factory;
        ID3D11RasterizerState * rasterizer_state;

        descriptor_set_emulator desc_emulator;
        object_set<rhi::input_layout, input_layout> input_layouts;
        object_set<rhi::shader, shader> shaders;
        object_set<rhi::pipeline, pipeline> pipelines;
        object_set<rhi::buffer, buffer> buffers;
        object_set<rhi::window, window> windows;

        pipeline * current_pipeline;   

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
            rdesc.CullMode = D3D11_CULL_BACK;
            rdesc.FrontCounterClockwise = TRUE;
            hr = dev->CreateRasterizerState(&rdesc, &rasterizer_state);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRasterizerState(...) failed");            
        }

        rhi::device_info get_info() const override { return {"Direct3D 11.1", {coord_axis::right, coord_axis::up, coord_axis::forward}, linalg::zero_to_one}; }

        auto create_window(const int2 & dimensions, std::string_view title) -> std::tuple<rhi::window, GLFWwindow *> override 
        {
            auto [handle, window] = windows.create();

            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_OPENGL_API, GLFW_NO_API);
            window.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);
            if(!window.w) throw std::runtime_error("glfwCreateWindow(...) failed");
            
            DXGI_SWAP_CHAIN_DESC scd {};
            scd.BufferCount = 1;                                    // one back buffer
            scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
            scd.OutputWindow = glfwGetWin32Window(window.w);        // the window to be used
            scd.SampleDesc.Count = 1;
            scd.Windowed = TRUE;                                    // windowed/full-screen mode
            auto hr = factory->CreateSwapChain(dev, &scd, &window.swap_chain);
            if(FAILED(hr)) throw std::runtime_error("IDXGIFactory::CreateSwapChain(...) failed");

            ID3D11Resource * image;
            hr = window.swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&image);
            if(FAILED(hr)) throw std::runtime_error("IDXGISwapChain::GetBuffer(...) failed");

            hr = dev->CreateRenderTargetView(image, nullptr, &window.render_target_view);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRenderTargetView(...) failed");

            D3D11_TEXTURE2D_DESC desc {};
            desc.Width = dimensions.x;
            desc.Height = dimensions.y;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_D32_FLOAT;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            hr = dev->CreateTexture2D(&desc, nullptr, &window.depth_texture);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateTexture2D(...) failed");

            hr = dev->CreateDepthStencilView(window.depth_texture, nullptr, &window.depth_stencil_view);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRenderTargetView(...) failed");

            return {handle, window.w};
        }

        std::tuple<rhi::buffer, char *> create_buffer(const rhi::buffer_desc & desc, const void * initial_data) override
        {
            auto [handle, buffer] = buffers.create();

            D3D11_BUFFER_DESC buffer_desc {};
            buffer_desc.ByteWidth = desc.size;
            buffer_desc.Usage = desc.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
            buffer_desc.BindFlags = 0;
            switch(desc.usage)
            {
            case rhi::buffer_usage::vertex: buffer_desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER; break;
            case rhi::buffer_usage::index: buffer_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER; break;
            case rhi::buffer_usage::uniform: buffer_desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER; break;
            case rhi::buffer_usage::storage: buffer_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE; break;
            }
            buffer_desc.CPUAccessFlags = desc.dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

            D3D11_SUBRESOURCE_DATA data {};
            data.pSysMem = initial_data;

            auto hr = dev->CreateBuffer(&buffer_desc, initial_data ? &data : 0, &buffer.buffer_object);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateBuffer(...) failed");

            if(desc.dynamic)
            {
                D3D11_MAPPED_SUBRESOURCE sub;
                hr = ctx->Map(buffer.buffer_object, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
                if(FAILED(hr)) throw std::runtime_error("ID3D11DeviceContext::Map(...) failed");
                buffer.mapped = reinterpret_cast<char *>(sub.pData);
            }

            return {handle, buffer.mapped};
        }

        rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) override { return desc_emulator.create_descriptor_set_layout(bindings); }
        rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) override { return desc_emulator.create_pipeline_layout(sets); }
        rhi::descriptor_pool create_descriptor_pool() { return desc_emulator.create_descriptor_pool(); }
        void reset_descriptor_pool(rhi::descriptor_pool pool) { desc_emulator.reset_descriptor_pool(pool); }
        rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) { return desc_emulator.alloc_descriptor_set(pool, layout); }
        void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) { desc_emulator.write_descriptor(set, binding, range); }

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto [handle, input_layout] = input_layouts.create();

            input_layout.bindings = bindings;
            for(auto & buf : bindings)
            {
                for(auto & attrib : buf.attributes)
                {
                    switch(attrib.type)
                    {
                    case rhi::attribute_format::float1: input_layout.input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float2: input_layout.input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float3: input_layout.input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    case rhi::attribute_format::float4: input_layout.input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32A32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                    default: throw std::logic_error("invalid rhi::attribute_format");
                    }
                }
            }
            return handle;
        }

        template<class T> rhi::shader create_shader(ID3DBlob * blob, HRESULT (__stdcall ID3D11Device::*create_func)(const void *, SIZE_T, ID3D11ClassLinkage *, T **))
        {
            T * s;
            auto hr = (dev->*create_func)(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::Create...Shader(...) failed");

            auto [handle, shader] = shaders.create();
            shader.blob = blob;
            shader.shader_object = s;
            return handle;
        }

        rhi::shader create_shader(const shader_module & module) override
        {
            spirv_cross::CompilerHLSL::Options options;
            options.shader_model = 50;
            spirv_cross::CompilerHLSL compiler(module.spirv);
            compiler.set_options(options);
            const auto hlsl = compiler.compile();
            //debug_callback(hlsl.c_str());

            const char * target = 0;
            switch(module.stage)
            {
            case shader_stage::vertex: target = "vs_5_0"; break;
            case shader_stage::fragment: target = "ps_5_0"; break;
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

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto [handle, pipeline] = pipelines.create();
            pipeline.desc = desc;
            for(auto s : desc.stages) 
            {
                std::visit([&pipeline](auto * s) { pipeline.set_shader(s); }, shaders[s].shader_object);
                if(std::holds_alternative<ID3D11VertexShader*>(shaders[s].shader_object))
                {
                    const auto & input_descs = input_layouts[desc.input].input_descs;
                    auto hr = dev->CreateInputLayout(input_descs.data(), input_descs.size(), shaders[s].blob->GetBufferPointer(), shaders[s].blob->GetBufferSize(), &pipeline.layout);
                    if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateInputLayout(...) failed");
                }
            }
            return handle;
        }

        void begin_render_pass(rhi::window window) override
        {
            ctx->ClearState();
            ctx->RSSetState(rasterizer_state);

            FLOAT rgba[] {0,0,0,1};
            ctx->ClearRenderTargetView(windows[window].render_target_view, rgba);
            ctx->ClearDepthStencilView(windows[window].depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0);

            ctx->OMSetRenderTargets(1, &windows[window].render_target_view, windows[window].depth_stencil_view);
            D3D11_VIEWPORT vp {0, 0, 512, 512, 0, 1};
            ctx->RSSetViewports(1, &vp);
        }

        void bind_pipeline(rhi::pipeline pipe) override
        {
            current_pipeline = &pipelines[pipe];
            ctx->IASetInputLayout(current_pipeline->layout);
            ctx->VSSetShader(current_pipeline->vs, nullptr, 0);
            ctx->PSSetShader(current_pipeline->ps, nullptr, 0);      
            switch(current_pipeline->desc.topology)
            {
            case rhi::primitive_topology::points: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); break;
            case rhi::primitive_topology::lines: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); break;
            case rhi::primitive_topology::triangles: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); break;
            }
        }

        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) override
        {
            desc_emulator.bind_descriptor_set(layout, set_index, set, [this](int index, rhi::buffer_range range) { bind_uniform_buffer(index, range); });
        }

        void bind_uniform_buffer(int index, rhi::buffer_range range) override
        {
            const UINT first_constant = range.offset/16, num_constants = (range.size+255)/256*16;
            ctx->VSSetConstantBuffers1(index, 1, &buffers[range.buffer].buffer_object, &first_constant, &num_constants);
            ctx->PSSetConstantBuffers1(index, 1, &buffers[range.buffer].buffer_object, &first_constant, &num_constants);
        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
            for(auto & buf : input_layouts[current_pipeline->desc.input].bindings)
            {
                if(buf.index == index)
                {
                    const UINT stride = buf.stride, offset = range.offset;
                    ctx->IASetVertexBuffers(index, 1, &buffers[range.buffer].buffer_object, &stride, &offset);
                }
            }
        }

        void bind_index_buffer(rhi::buffer_range range) override
        {
            ctx->IASetIndexBuffer(buffers[range.buffer].buffer_object, DXGI_FORMAT_R32_UINT, range.offset);
        }

        void draw(int first_vertex, int vertex_count) override
        {
            ctx->Draw(vertex_count, first_vertex);
        }

        void draw_indexed(int first_index, int index_count) override
        {
            ctx->DrawIndexed(index_count, first_index, 0);
        }

        void end_render_pass() override
        {

        }

        void present(rhi::window window) override
        {
            windows[window].swap_chain->Present(1, 0);
        }
    };
}

rhi::device * create_d3d11_device(std::function<void(const char *)> debug_callback)
{
    return new d3d::device(debug_callback);
}