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
    DXGI_FORMAT get_dx_format(image_format format)
    {
        switch(format)
        {
        #define X(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return DX;
        #include "rhi-format.inl"
        #undef X
        default: fail_fast();
        }
    }

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
        ID3D11DepthStencilView * depth_stencil_view;
    };

    struct d3d_sampler
    {
        ID3D11SamplerState * sampler_state;
    };

    struct d3d_render_pass
    {
        rhi::render_pass_desc desc;
    };

    struct d3d_framebuffer
    {
        int2 dims;
        std::vector<ID3D11RenderTargetView *> render_target_views; // non-owning
        ID3D11DepthStencilView * depth_stencil_view; // non-owning
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
        ID3D11RenderTargetView * swap_chain_view;
        image depth_image;
        framebuffer fb;
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
        template<> struct traits<sampler> { using type = d3d_sampler; };
        template<> struct traits<render_pass> { using type = d3d_render_pass; };
        template<> struct traits<framebuffer> { using type = d3d_framebuffer; };
        template<> struct traits<shader> { using type = shader_module; }; 
        template<> struct traits<pipeline> { using type = d3d_pipeline; };
        template<> struct traits<window> { using type = d3d_window; };
        heterogeneous_object_set<traits, buffer, image, sampler, render_pass, framebuffer, shader, pipeline, window> objects;
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
            rdesc.DepthClipEnable = TRUE;
            check("ID3D11Device::CreateRasterizerState", dev->CreateRasterizerState(&rdesc, &rasterizer_state));
        }

        device_info get_info() const override { return {{coord_axis::right, coord_axis::up, coord_axis::forward}, linalg::zero_to_one}; }

        buffer create_buffer(const buffer_desc & desc, const void * initial_data) override
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

            return handle;
        }
        char * get_mapped_memory(buffer buffer) override { return objects[buffer].mapped; }
        void destroy_buffer(buffer buffer) override { objects.destroy(buffer); }

        image create_image(const image_desc & desc, std::vector<const void *> initial_data) override
        {
            if(desc.shape != rhi::image_shape::_2d) throw std::logic_error("shape not supported");
            D3D11_TEXTURE2D_DESC tex_desc {exactly(desc.dimensions.x), exactly(desc.dimensions.y), exactly(desc.mip_levels), 1, get_dx_format(desc.format), {1,0}, D3D11_USAGE_DEFAULT};
            if(desc.flags & rhi::image_flag::sampled_image_bit) tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if(desc.flags & rhi::image_flag::color_attachment_bit) tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            if(desc.flags & rhi::image_flag::depth_attachment_bit) tex_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
            if(desc.flags & rhi::image_flag::generate_mips_bit)
            {
                tex_desc.BindFlags |= (D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET);
                tex_desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
            }
            ID3D11Texture2D * tex;
            if(initial_data.empty()) check("ID3D11Device::CreateTexture2D", dev->CreateTexture2D(&tex_desc, nullptr, &tex));
            else
            {              
                const D3D11_SUBRESOURCE_DATA data {initial_data[0], exactly(get_pixel_size(desc.format)*desc.dimensions.x)};
                check("ID3D11Device::CreateTexture2D", dev->CreateTexture2D(&tex_desc, &data, &tex));
            }

            auto [handle, im] = objects.create<image>();
            im.resource = tex;
            if(desc.flags & rhi::image_flag::sampled_image_bit) check("ID3D11Device::CreateShaderResourceView", dev->CreateShaderResourceView(im.resource, nullptr, &im.shader_resource_view));
            if(desc.flags & rhi::image_flag::color_attachment_bit) check("ID3D11Device::CreateShaderResourceView", dev->CreateRenderTargetView(im.resource, nullptr, &im.render_target_view));
            if(desc.flags & rhi::image_flag::depth_attachment_bit) check("ID3D11Device::CreateShaderResourceView", dev->CreateDepthStencilView(im.resource, nullptr, &im.depth_stencil_view));
            if(desc.flags & rhi::image_flag::generate_mips_bit) ctx->GenerateMips(im.shader_resource_view);
            return handle;
        }
        void destroy_image(image image) override { objects.destroy(image); }

        sampler create_sampler(const sampler_desc & desc) override 
        {
            auto convert_mode = [](rhi::address_mode mode)
            {
                switch(mode)
                {
                case rhi::address_mode::repeat: return D3D11_TEXTURE_ADDRESS_WRAP;
                case rhi::address_mode::mirrored_repeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
                case rhi::address_mode::clamp_to_edge: return D3D11_TEXTURE_ADDRESS_CLAMP;
                case rhi::address_mode::mirror_clamp_to_edge: return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
                case rhi::address_mode::clamp_to_border: return D3D11_TEXTURE_ADDRESS_BORDER;
                default: fail_fast();
                }
            };
            D3D11_SAMPLER_DESC samp_desc {};
            if(desc.mag_filter == filter::linear) samp_desc.Filter = static_cast<D3D11_FILTER>(samp_desc.Filter|D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT);
            if(desc.min_filter == filter::linear) samp_desc.Filter = static_cast<D3D11_FILTER>(samp_desc.Filter|D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT);
            if(desc.mip_filter)
            {
                samp_desc.MaxLOD = 1000;
                if(desc.mip_filter == filter::linear) samp_desc.Filter = static_cast<D3D11_FILTER>(samp_desc.Filter|D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR);
            }
            samp_desc.AddressU = convert_mode(desc.wrap_s);
            samp_desc.AddressV = convert_mode(desc.wrap_t);
            samp_desc.AddressW = convert_mode(desc.wrap_r);
            auto [handle, samp] = objects.create<sampler>();
            dev->CreateSamplerState(&samp_desc, &samp.sampler_state);
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
            fb.dims = desc.dimensions;
            for(auto attachment : desc.color_attachments) fb.render_target_views.push_back(objects[attachment].render_target_view);
            if(desc.depth_attachment) fb.depth_stencil_view = objects[*desc.depth_attachment].depth_stencil_view;
            return handle;
        }
        void destroy_framebuffer(framebuffer framebuffer) override { objects.destroy(framebuffer); }

        window create_window(render_pass pass, const int2 & dimensions, std::string_view title) override
        {
            // Validate render pass description
            auto & pass_desc = objects[pass].desc;
            if(pass_desc.color_attachments.size() != 1) throw std::logic_error("window render pass must have exactly one color attachment");

            // Create the window
            auto [handle, win] = objects.create<window>();
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_OPENGL_API, GLFW_NO_API);
            win.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);
            if(!win.w) throw std::runtime_error("glfwCreateWindow(...) failed");

            // Create swapchain and render target view for this window
            DXGI_SWAP_CHAIN_DESC scd {};
            scd.BufferCount = 3;
            scd.BufferDesc.Format = get_dx_format(pass_desc.color_attachments[0].format);
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            scd.OutputWindow = glfwGetWin32Window(win.w);
            scd.SampleDesc.Count = 1;
            scd.Windowed = TRUE;
            check("IDXGIFactory::CreateSwapChain", factory->CreateSwapChain(dev, &scd, &win.swap_chain));
            ID3D11Resource * image;
            check("IDXGISwapChain::GetBuffer", win.swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&image));
            check("ID3D11Device::CreateRenderTargetView", dev->CreateRenderTargetView(image, nullptr, &win.swap_chain_view));

            // Create a framebuffer for this window
            auto [fb_handle, fb] = objects.create<framebuffer>();
            fb.dims = dimensions;
            fb.render_target_views = {win.swap_chain_view};
            win.fb = fb_handle;

            // If render pass specifies a depth attachment, create a depth buffer specifically for this window
            if(pass_desc.depth_attachment)
            {
                win.depth_image = create_image({rhi::image_shape::_2d, {dimensions,1}, 1, pass_desc.depth_attachment->format, rhi::depth_attachment_bit}, {});
                fb.depth_stencil_view = objects[win.depth_image].depth_stencil_view;
            }

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
        void write_descriptor(descriptor_set set, int binding, sampler sampler, image image) override { desc_emulator.write_descriptor(set, binding, sampler, image); }

        shader create_shader(const shader_module & module) override
        {
            auto [handle, s] = objects.create<shader>();
            s = module;
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
            ctx->ClearState();
            ctx->RSSetState(rasterizer_state);
            cmd_emulator.execute(cmd, overload(
                [this](const begin_render_pass_command & c)
                {
                    auto & pass = objects[c.pass];
                    auto & fb = objects[c.framebuffer];

                    // Clear render targets if specified by render pass
                    for(size_t i=0; i<pass.desc.color_attachments.size(); ++i)
                    {
                        if(std::holds_alternative<clear>(pass.desc.color_attachments[i].load_op))
                        {
                            const FLOAT rgba[] {0,0,0,1};
                            ctx->ClearRenderTargetView(fb.render_target_views[i], rgba);
                        }
                    }
                    if(pass.desc.depth_attachment)
                    {
                        if(std::holds_alternative<clear>(pass.desc.depth_attachment->load_op))
                        {
                            ctx->ClearDepthStencilView(fb.depth_stencil_view, D3D11_CLEAR_DEPTH, 1.0f, 0);
                        }
                    }

                    // Set the render targets and viewport
                    ctx->OMSetRenderTargets(exactly(fb.render_target_views.size()), fb.render_target_views.data(), fb.depth_stencil_view);
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
                    }, [this](size_t index, sampler sampler, image image) 
                    {
                        ctx->VSSetSamplers(exactly(index), 1, &objects[sampler].sampler_state);
                        ctx->VSSetShaderResources(exactly(index), 1, &objects[image].shader_resource_view);
                        ctx->PSSetSamplers(exactly(index), 1, &objects[sampler].sampler_state);
                        ctx->PSSetShaderResources(exactly(index), 1, &objects[image].shader_resource_view);
                    });
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
        void begin_render_pass(command_buffer cmd, render_pass pass, framebuffer framebuffer) override 
        { 
            if(objects[pass].desc.color_attachments.size() > objects[framebuffer].render_target_views.size()) throw std::logic_error("color attachment count mismatch");
            if(objects[pass].desc.depth_attachment && !objects[framebuffer].depth_stencil_view) throw std::logic_error("depth attachment count mismatch");
            cmd_emulator.begin_render_pass(cmd, pass, framebuffer); 
        }
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

    autoregister_backend<d3d_device> autoregister_d3d_backend {"Direct3D 11.1"};
}
