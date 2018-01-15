#include "rhi-internal.h"
#include <sstream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include "../../dep/SPIRV-Cross/spirv_hlsl.hpp"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace rhi
{
    struct d3d_error : public std::error_category
    {
        const char * name() const noexcept override { return "HRESULT"; }
        std::string message(int value) const override { return std::to_string(value); }
        static const std::error_category & instance() { static d3d_error inst; return inst; }
    };
    void check(const char * func, HRESULT result)
    {
        if(FAILED(result))
        {
            std::ostringstream ss; ss << func << "(...) failed";
            throw std::system_error(std::error_code(exactly(result), d3d_error::instance()), ss.str());
        }
    }

    struct d3d_buffer
    {
        ID3D11Buffer * buffer_object;
        char * mapped = 0;
    };
    
    struct d3d_image
    {
        ID3D11Resource * resource;
        ID3D11ShaderResourceView * shader_resource_view;
        ID3D11RenderTargetView * render_target_view;
    };

    struct d3d_render_pass
    {
        rhi::render_pass_desc desc;
    };

    struct d3d_framebuffer
    {
        int2 dims;
        ID3D11RenderTargetView * render_target_view;
        ID3D11DepthStencilView * depth_stencil_view;
    };

    struct d3d_descriptor_set_layout
    {
        std::vector<rhi::descriptor_binding> bindings;
    };
    
    struct d3d_pipeline
    {
        rhi::pipeline_desc desc;
        ID3D11InputLayout * layout;
        ID3D11VertexShader * vs;
        ID3D11PixelShader * ps;
        void set_shader(ID3D11VertexShader * s) { vs = s; }
        void set_shader(ID3D11PixelShader * s) { ps = s; }
    };

    struct d3d_window
    {
        GLFWwindow * w;
        IDXGISwapChain * swap_chain;
        ID3D11Texture2D * depth_texture;
        rhi::framebuffer fb;
    };

    struct d3d_device : device
    {
        std::function<void(const char *)> debug_callback;    

        ID3D11Device1 * dev;
        ID3D11DeviceContext1 * ctx;
        IDXGIFactory * factory;
        ID3D11RasterizerState * rasterizer_state;

        template<class T> struct traits;
        template<> struct traits<buffer> { using type = d3d_buffer; };
        template<> struct traits<image> { using type = d3d_image; };
        template<> struct traits<render_pass> { using type = d3d_render_pass; };
        template<> struct traits<framebuffer> { using type = d3d_framebuffer; };
        template<> struct traits<shader> { using type = shader_module; }; 
        template<> struct traits<pipeline> { using type = d3d_pipeline; };
        template<> struct traits<window> { using type = d3d_window; };
        heterogeneous_object_set<traits, buffer, image, render_pass, framebuffer, shader, pipeline, window> objects;
        descriptor_emulator desc_emulator;
        command_emulator cmd_emulator;

        d3d_pipeline * current_pipeline;   

        d3d_device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
        {
            const D3D_FEATURE_LEVEL feature_levels[] {D3D_FEATURE_LEVEL_11_1};
            ID3D11Device * dev11;
            ID3D11DeviceContext * ctx11;
            IDXGIDevice * dxgi_dev;
            check("D3D11CreateDevice", D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, feature_levels, 1, D3D11_SDK_VERSION, &dev11, nullptr, &ctx11));
            check("IUnknown::QueryInterface", dev11->QueryInterface(__uuidof(ID3D11Device1), (void **)&dev));
            check("IUnknown::QueryInterface", ctx11->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&ctx));
            check("IUnknown::QueryInterface", dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi_dev));

            IDXGIAdapter * adapter = nullptr;
            check("IDXGIDevice::GetAdapter", dxgi_dev->GetAdapter(&adapter));
            check("IDXGIAdapter::GetParent", adapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory));

            D3D11_RASTERIZER_DESC rdesc {};
            rdesc.FillMode = D3D11_FILL_SOLID;
            rdesc.CullMode = D3D11_CULL_BACK;
            rdesc.FrontCounterClockwise = TRUE;
            check("ID3D11Device::CreateRasterizerState", dev->CreateRasterizerState(&rdesc, &rasterizer_state));
        }

        device_info get_info() const override { return {"Direct3D 11.1", {coord_axis::right, coord_axis::up, coord_axis::forward}, linalg::zero_to_one}; }

        std::tuple<buffer, char *> create_buffer(const buffer_desc & desc, const void * initial_data) override
        {
            D3D11_BUFFER_DESC buffer_desc {};
            buffer_desc.ByteWidth = exactly(desc.size);
            buffer_desc.Usage = desc.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
            buffer_desc.BindFlags = 0;
            switch(desc.usage)
            {
            case buffer_usage::vertex: buffer_desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER; break;
            case buffer_usage::index: buffer_desc.BindFlags |= D3D11_BIND_INDEX_BUFFER; break;
            case buffer_usage::uniform: buffer_desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER; break;
            case buffer_usage::storage: buffer_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE; break;
            }
            buffer_desc.CPUAccessFlags = desc.dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

            D3D11_SUBRESOURCE_DATA data {};
            data.pSysMem = initial_data;
            
            auto [handle, buf] = objects.create<buffer>();
            auto hr = dev->CreateBuffer(&buffer_desc, initial_data ? &data : 0, &buf.buffer_object);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateBuffer(...) failed");

            if(desc.dynamic)
            {
                D3D11_MAPPED_SUBRESOURCE sub;
                hr = ctx->Map(buf.buffer_object, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
                if(FAILED(hr)) throw std::runtime_error("ID3D11DeviceContext::Map(...) failed");
                buf.mapped = reinterpret_cast<char *>(sub.pData);
            }

            return {handle, buf.mapped};
        }
        void destroy_buffer(buffer buffer) override { objects.destroy(buffer); }

        image create_image(const image_desc & desc, std::vector<const void *> initial_data) override
        {
            auto [handle, im] = objects.create<image>();
            return handle;
        }
        void destroy_image(image image) override { objects.destroy(image); }

        render_pass create_render_pass(const render_pass_desc & desc) override 
        {
            auto [handle, pass] = objects.create<render_pass>();
            pass.desc = desc;
            return handle;
        }
        void destroy_render_pass(render_pass pass) override { objects.destroy(pass); }

        window create_window(render_pass pass, const int2 & dimensions, std::string_view title) override
        {
            auto [fb_handle, fb] = objects.create<framebuffer>();
            auto [handle, window] = objects.create<window>();
            fb.dims = dimensions;
            window.fb = fb_handle;

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

            hr = dev->CreateRenderTargetView(image, nullptr, &fb.render_target_view);
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

            hr = dev->CreateDepthStencilView(window.depth_texture, nullptr, &fb.depth_stencil_view);
            if(FAILED(hr)) throw std::runtime_error("ID3D11Device::CreateRenderTargetView(...) failed");

            return {handle};
        }
        GLFWwindow * get_glfw_window(window window) override { return objects[window].w; }
        framebuffer get_swapchain_framebuffer(window window) override { return objects[window].fb; }

        descriptor_set_layout create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override { return desc_emulator.create_descriptor_set_layout(bindings); }
        pipeline_layout create_pipeline_layout(const std::vector<descriptor_set_layout> & sets) override { return desc_emulator.create_pipeline_layout(sets); }
        descriptor_pool create_descriptor_pool() { return desc_emulator.create_descriptor_pool(); }
        void reset_descriptor_pool(descriptor_pool pool) { desc_emulator.reset_descriptor_pool(pool); }
        descriptor_set alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout layout) { return desc_emulator.alloc_descriptor_set(pool, layout); }
        void write_descriptor(descriptor_set set, int binding, buffer_range range) { desc_emulator.write_descriptor(set, binding, range); }
        void write_descriptor(descriptor_set set, int binding, image image) override { desc_emulator.write_descriptor(set, binding, image); }

        shader create_shader(const shader_module & module) override
        {
            auto [handle, shader] = objects.create<shader>();
            shader = module;
            return handle;
        }

        pipeline create_pipeline(const pipeline_desc & desc) override
        {
            auto [handle, pipe] = objects.create<pipeline>();
            pipe.desc = desc;
            for(auto & s : desc.stages)
            {
                auto & shader = objects[s];

                // Compile SPIR-V to HLSL
                spirv_cross::CompilerHLSL compiler(shader.spirv);
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
                spirv_cross::CompilerHLSL::Options options;
                options.shader_model = 50;
                compiler.set_options(options);
                const auto hlsl = compiler.compile();
                //debug_callback(hlsl.c_str());

                // Compile HLSL and create shader stages and input layout
                ID3DBlob * shader_blob, * error_blob;
                switch(shader.stage)
                {
                    
                default: throw std::logic_error("invalid shader_stage");
                case shader_stage::fragment:
                    check("D3DCompile", D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &shader_blob, &error_blob));
                    check("ID3D11Device::CreatePixelShader", dev->CreatePixelShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &pipe.ps));
                    break;
                case shader_stage::vertex: 
                    check("D3DCompile", D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &shader_blob, &error_blob));
                    check("ID3D11Device::CreateVertexShader", dev->CreateVertexShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &pipe.vs));           
                    std::vector<D3D11_INPUT_ELEMENT_DESC> input_descs;  
                    for(auto & buf : desc.input)
                    {
                        for(auto & attrib : buf.attributes)
                        {
                            switch(attrib.type)
                            {
                            case attribute_format::float1: input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                            case attribute_format::float2: input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                            case attribute_format::float3: input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                            case attribute_format::float4: input_descs.push_back({"TEXCOORD", (UINT)attrib.index, DXGI_FORMAT_R32G32B32A32_FLOAT, (UINT)buf.index, (UINT)attrib.offset, D3D11_INPUT_PER_VERTEX_DATA, 0}); break;
                            default: throw std::logic_error("invalid attribute_format");
                            }
                        }
                    }
                    check("ID3D11Device::CreateInputLayout", dev->CreateInputLayout(input_descs.data(), exactly(input_descs.size()), shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), &pipe.layout));
                    break;
                }
            }
            return handle;
        }

        void destroy_descriptor_pool(descriptor_pool pool) override { desc_emulator.destroy(pool); }
        void destroy_descriptor_set_layout(descriptor_set_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_pipeline_layout(pipeline_layout layout) override { desc_emulator.destroy(layout); }
        void destroy_shader(shader shader) override { objects.destroy(shader); }
        void destroy_pipeline(pipeline pipeline)  override { objects.destroy(pipeline); }        
        void destroy_window(window window) override { objects.destroy(window); }
        
        void submit_command_buffer(command_buffer cmd)
        {
            cmd_emulator.execute(cmd, overload(
                [this](const begin_render_pass_command & c)
                {
                    auto & fb = objects[c.framebuffer];

                    ctx->ClearState();
                    ctx->RSSetState(rasterizer_state);

                    FLOAT rgba[] {0,0,0,1};
                    ctx->ClearRenderTargetView(fb.render_target_view, rgba);
                    ctx->ClearDepthStencilView(fb.depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0);

                    ctx->OMSetRenderTargets(1, &fb.render_target_view, fb.depth_stencil_view);
                    D3D11_VIEWPORT vp {0, 0, exactly(fb.dims.x), exactly(fb.dims.y), 0, 1};
                    ctx->RSSetViewports(1, &vp);
                },
                [this](const bind_pipeline_command & c)
                {
                    current_pipeline = &objects[c.pipe];
                    ctx->IASetInputLayout(current_pipeline->layout);
                    ctx->VSSetShader(current_pipeline->vs, nullptr, 0);
                    ctx->PSSetShader(current_pipeline->ps, nullptr, 0);      
                    switch(current_pipeline->desc.topology)
                    {
                    case primitive_topology::points: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); break;
                    case primitive_topology::lines: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); break;
                    case primitive_topology::triangles: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); break;
                    }
                },
                [this](const bind_descriptor_set_command & c)
                {
                    desc_emulator.bind_descriptor_set(c.layout, c.set_index, c.set, [this](size_t index, buffer_range range) 
                    { 
                        const UINT first_constant = exactly(range.offset/16), num_constants = exactly((range.size+255)/256*16);
                        ctx->VSSetConstantBuffers1(exactly(index), 1, &objects[range.buffer].buffer_object, &first_constant, &num_constants);
                        ctx->PSSetConstantBuffers1(exactly(index), 1, &objects[range.buffer].buffer_object, &first_constant, &num_constants);
                    }, [this](size_t index, image image) {});
                },
                [this](const bind_vertex_buffer_command & c)
                {
                    for(auto & buf : current_pipeline->desc.input)
                    {
                        if(buf.index == c.index)
                        {
                            const UINT stride = exactly(buf.stride), offset = exactly(c.range.offset);
                            ctx->IASetVertexBuffers(c.index, 1, &objects[c.range.buffer].buffer_object, &stride, &offset);
                        }
                    }      
                },
                [this](const bind_index_buffer_command & c) { ctx->IASetIndexBuffer(objects[c.range.buffer].buffer_object, DXGI_FORMAT_R32_UINT, exactly(c.range.offset)); },
                [this](const draw_command & c) { ctx->Draw(c.vertex_count, c.first_vertex); },
                [this](const draw_indexed_command & c) { ctx->DrawIndexed(c.index_count, c.first_index, 0); },
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
            objects[window].swap_chain->Present(1, 0);
            cmd_emulator.destroy_command_buffer(submit);
        }

        void wait_idle() override { ctx->Flush(); }
    };
}

std::shared_ptr<rhi::device> create_d3d11_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<rhi::d3d_device>(debug_callback);
}