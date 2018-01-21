#include "rhi-internal.h"
#include <sstream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <d3d11_4.h>
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

    struct d3d_device : device
    {
        std::function<void(const char *)> debug_callback;    

        ID3D11Device5 * dev;
        ID3D11DeviceContext4 * ctx;
        IDXGIFactory * factory;
        ID3D11Fence * fence;
        uint64_t submitted_index = 0;

        d3d_device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
        {
            const D3D_FEATURE_LEVEL feature_levels[] {D3D_FEATURE_LEVEL_11_1};
            ID3D11Device * dev11;
            ID3D11DeviceContext * ctx11;
            IDXGIDevice * dxgi_dev;
            check("D3D11CreateDevice", D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, feature_levels, 1, D3D11_SDK_VERSION, &dev11, nullptr, &ctx11));
            check("IUnknown::QueryInterface", dev11->QueryInterface(&dev));
            check("IUnknown::QueryInterface", ctx11->QueryInterface(&ctx));
            check("IUnknown::QueryInterface", dev->QueryInterface(&dxgi_dev));

            IDXGIAdapter * adapter = nullptr;
            check("IDXGIDevice::GetAdapter", dxgi_dev->GetAdapter(&adapter));
            check("IDXGIAdapter::GetParent", adapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory));

            check("ID3D11Device5::CreateFence", dev->CreateFence(0, D3D11_FENCE_FLAG_NONE, __uuidof(ID3D11Fence), (void **)&fence));
        }

        device_info get_info() const override { return {linalg::zero_to_one, true}; }

        ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) override;
        ptr<sampler> create_sampler(const sampler_desc & desc) override;
        ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) override;
        ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) override;
        ptr<window> create_window(const int2 & dimensions, std::string_view title) override;        
        
        ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override { return descriptor_emulator::create_descriptor_set_layout(bindings); }
        ptr<pipeline_layout> create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets) override { return descriptor_emulator::create_pipeline_layout(sets); }
        ptr<shader> create_shader(const shader_module & module) override;
        ptr<pipeline> create_pipeline(const pipeline_desc & desc) override;

        ptr<descriptor_pool> create_descriptor_pool() override { return descriptor_emulator::create_descriptor_pool(); }

        ptr<command_buffer> start_command_buffer() override { return command_emulator::start_command_buffer(); }
        uint64_t submit(command_buffer & cmd) override;
        uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) override;
        void wait_until_complete(uint64_t submit_id) override;
    };

    autoregister_backend<d3d_device> autoregister_d3d_backend {"Direct3D 11.1"};

    struct d3d_buffer : buffer
    {
        ID3D11Buffer * buffer_object = 0;
        char * mapped = 0;

        d3d_buffer(ID3D11Device & device, ID3D11DeviceContext & ctx, const buffer_desc & desc, const void * initial_data)
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
            check("ID3D11Device::CreateBuffer", device.CreateBuffer(&buffer_desc, initial_data ? &data : 0, &buffer_object));

            if(desc.dynamic)
            {
                D3D11_MAPPED_SUBRESOURCE sub;
                check("ID3D11DeviceContext::Map", ctx.Map(buffer_object, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub));
                mapped = reinterpret_cast<char *>(sub.pData);
            }
        }

        char * get_mapped_memory() override { return mapped; }
    };

    struct d3d_sampler : sampler
    {
        ID3D11SamplerState * sampler_state = 0;

        d3d_sampler(ID3D11Device & device, const sampler_desc & desc)
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
            check("ID3D11Device::CreateSamplerState", device.CreateSamplerState(&samp_desc, &sampler_state));
        }
    };
    
    struct d3d_image : image
    {
        rhi::image_format format;
        ID3D11Resource * resource = 0;
        ID3D11ShaderResourceView * shader_resource_view;

        d3d_image(ID3D11Device & device, const image_desc & desc, std::vector<const void *> initial_data)
        {
            UINT array_size = 1, bind_flags = 0, misc_flags = 0;
            if(desc.flags & rhi::image_flag::sampled_image_bit) bind_flags |= D3D11_BIND_SHADER_RESOURCE;
            if(desc.flags & rhi::image_flag::color_attachment_bit) bind_flags |= D3D11_BIND_RENDER_TARGET;
            if(desc.flags & rhi::image_flag::depth_attachment_bit) bind_flags |= D3D11_BIND_DEPTH_STENCIL;
            if(desc.mip_levels > 1)
            {
                bind_flags |= (D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET);
                misc_flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
            }
            if(desc.shape == rhi::image_shape::cube)
            {
                array_size *= 6;
                misc_flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
            }
            std::vector<D3D11_SUBRESOURCE_DATA> data;
            const size_t row_pitch = get_pixel_size(desc.format)*desc.dimensions.x, slice_pitch = row_pitch*desc.dimensions.y;
            for(auto d : initial_data) data.push_back({d, exactly(row_pitch), exactly(slice_pitch)});

            format = desc.format;
            if(desc.shape == rhi::image_shape::_1d)
            {
                const D3D11_TEXTURE1D_DESC tex_desc {exactly(desc.dimensions.x), exactly(desc.mip_levels), array_size, get_dx_format(desc.format), D3D11_USAGE_DEFAULT, bind_flags, 0, misc_flags};
                ID3D11Texture1D * tex;
                check("ID3D11Device::CreateTexture1D", device.CreateTexture1D(&tex_desc, data.empty() ? nullptr : data.data(), &tex));
                resource = tex;
            }
            else if(desc.shape == rhi::image_shape::_3d)
            {
                const D3D11_TEXTURE3D_DESC tex_desc {exactly(desc.dimensions.x), exactly(desc.dimensions.y), exactly(desc.dimensions.z), exactly(desc.mip_levels), get_dx_format(desc.format), D3D11_USAGE_DEFAULT, bind_flags, 0, misc_flags};
                ID3D11Texture3D * tex;
                check("ID3D11Device::CreateTexture2D", device.CreateTexture3D(&tex_desc, data.empty() ? nullptr : data.data(), &tex));
                resource = tex;
            }
            else // 2D or cube textures
            {
                const D3D11_TEXTURE2D_DESC tex_desc {exactly(desc.dimensions.x), exactly(desc.dimensions.y), exactly(desc.mip_levels), array_size, get_dx_format(desc.format), {1,0}, D3D11_USAGE_DEFAULT, bind_flags, 0, misc_flags};
                ID3D11Texture2D * tex;
                check("ID3D11Device::CreateTexture2D", device.CreateTexture2D(&tex_desc, data.empty() ? nullptr : data.data(), &tex));
                resource = tex;
            } 
            if(desc.flags & rhi::image_flag::sampled_image_bit) check("ID3D11Device::CreateShaderResourceView", device.CreateShaderResourceView(resource, nullptr, &shader_resource_view));
        }
    };

    ID3D11RenderTargetView * create_render_target_view(ID3D11Device & device, ID3D11Resource * resource, rhi::image_format format, int mip, int layer)
    {
        D3D11_RENDER_TARGET_VIEW_DESC view_desc {get_dx_format(format), D3D11_RTV_DIMENSION_TEXTURE2DARRAY};
        view_desc.Texture2DArray.MipSlice = exactly(mip);
        view_desc.Texture2DArray.FirstArraySlice = exactly(layer);
        view_desc.Texture2DArray.ArraySize = 1;
        ID3D11RenderTargetView * view;
        check("ID3D11Device::CreateRenderTargetView", device.CreateRenderTargetView(resource, &view_desc, &view));
        return view;
    }

    ID3D11DepthStencilView * create_depth_stencil_view(ID3D11Device & device, ID3D11Resource * resource, rhi::image_format format, int mip, int layer)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC view_desc {get_dx_format(format), D3D11_DSV_DIMENSION_TEXTURE2DARRAY};
        view_desc.Texture2DArray.MipSlice = exactly(mip);
        view_desc.Texture2DArray.FirstArraySlice = exactly(layer);
        view_desc.Texture2DArray.ArraySize = 1;
        ID3D11DepthStencilView * view;
        check("ID3D11Device::CreateDepthStencilView", device.CreateDepthStencilView(resource, &view_desc, &view));
        return view;
    }

    struct d3d_framebuffer : framebuffer
    {
        int2 dims;
        std::vector<ID3D11RenderTargetView *> render_target_views; // non-owning
        ID3D11DepthStencilView * depth_stencil_view = 0; // non-owning

        d3d_framebuffer() {}
        d3d_framebuffer(ID3D11Device & device, const framebuffer_desc & desc)
        {
            dims = desc.dimensions;
            for(auto attachment : desc.color_attachments) render_target_views.push_back(create_render_target_view(device, static_cast<d3d_image &>(*attachment.image).resource, static_cast<d3d_image &>(*attachment.image).format, attachment.mip, attachment.layer));
            if(desc.depth_attachment) depth_stencil_view = create_depth_stencil_view(device, static_cast<d3d_image &>(*desc.depth_attachment->image).resource, static_cast<d3d_image &>(*desc.depth_attachment->image).format, desc.depth_attachment->mip, desc.depth_attachment->layer);
        }

        coord_system get_ndc_coords() const override { return {coord_axis::right, coord_axis::up, coord_axis::forward}; }
    };

    struct d3d_window : window
    {
        GLFWwindow * w = 0;
        IDXGISwapChain * swap_chain = 0;
        ID3D11RenderTargetView * swap_chain_view = 0;
        ptr<d3d_image> depth_image;
        ptr<d3d_framebuffer> fb;

        d3d_window(d3d_device & device, const int2 & dimensions, std::string title)
        {
            // Create the window
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_OPENGL_API, GLFW_NO_API);
            w = glfwCreateWindow(dimensions.x, dimensions.y, title.c_str(), nullptr, nullptr);
            if(!w) throw std::runtime_error("glfwCreateWindow(...) failed");

            // Create swapchain and render target view for this window
            DXGI_SWAP_CHAIN_DESC scd {};
            scd.BufferCount = 3;
            scd.BufferDesc.Format = get_dx_format(image_format::rgba_srgb8);
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            scd.OutputWindow = glfwGetWin32Window(w);
            scd.SampleDesc.Count = 1;
            scd.Windowed = TRUE;
            check("IDXGIFactory::CreateSwapChain", device.factory->CreateSwapChain(device.dev, &scd, &swap_chain));
            ID3D11Resource * image;
            check("IDXGISwapChain::GetBuffer", swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&image));
            check("ID3D11Device::CreateRenderTargetView", device.dev->CreateRenderTargetView(image, nullptr, &swap_chain_view));

            // Create a framebuffer for this window
            fb = new delete_when_unreferenced<d3d_framebuffer>{};
            fb->dims = dimensions;
            fb->render_target_views = {swap_chain_view};

            // If render pass specifies a depth attachment, create a depth buffer specifically for this window
            depth_image = &static_cast<d3d_image &>(*device.create_image({rhi::image_shape::_2d, {dimensions,1}, 1, image_format::depth_float32, rhi::depth_attachment_bit}, {}));
            fb->depth_stencil_view = create_depth_stencil_view(*device.dev, depth_image->resource, image_format::depth_float32, 0, 0);
        }

        GLFWwindow * get_glfw_window() override { return w; }
        framebuffer & get_swapchain_framebuffer() override { return *fb; }
    };

    struct d3d_shader : shader
    {
        shader_module module;
        
        d3d_shader(const shader_module & module) : module{module} {}
    };
    
    struct d3d_pipeline : pipeline
    {
        rhi::pipeline_desc desc;
        ID3D11InputLayout * layout = 0;
        ID3D11VertexShader * vs = 0;
        ID3D11PixelShader * ps = 0;
        ID3D11RasterizerState * rasterizer_state = 0;
        ID3D11DepthStencilState * depth_stencil_state = 0;

        d3d_pipeline(ID3D11Device & device, const pipeline_desc & desc) : desc{desc}
        {
            for(auto & s : desc.stages)
            {
                auto & shader = static_cast<d3d_shader &>(*s);

                // Compile SPIR-V to HLSL
                spirv_cross::CompilerHLSL compiler(shader.module.spirv);
	            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
	            for(auto & resource : resources.uniform_buffers)
	            {
                    compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(descriptor_emulator::get_flat_buffer_binding(*desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
		            compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }
	            for(auto & resource : resources.sampled_images)
	            {
		            compiler.set_decoration(resource.id, spv::DecorationBinding, exactly(descriptor_emulator::get_flat_image_binding(*desc.layout, compiler.get_decoration(resource.id, spv::DecorationDescriptorSet), compiler.get_decoration(resource.id, spv::DecorationBinding))));
                    compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
	            }
                spirv_cross::CompilerHLSL::Options options;
                options.shader_model = 50;
                compiler.set_options(options);
                const auto hlsl = compiler.compile();
                //debug_callback(hlsl.c_str());

                // Compile HLSL and create shader stages and input layout
                ID3DBlob * shader_blob, * error_blob;
                switch(shader.module.stage)
                {
                    
                default: throw std::logic_error("invalid shader_stage");
                case shader_stage::fragment:
                    check("D3DCompile", D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &shader_blob, &error_blob));
                    check("ID3D11Device::CreatePixelShader", device.CreatePixelShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &ps));
                    break;
                case shader_stage::vertex: 
                    check("D3DCompile", D3DCompile(hlsl.c_str(), hlsl.size(), "spirv-cross.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &shader_blob, &error_blob));
                    check("ID3D11Device::CreateVertexShader", device.CreateVertexShader(shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), nullptr, &vs));           
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
                    check("ID3D11Device::CreateInputLayout", device.CreateInputLayout(input_descs.data(), exactly(input_descs.size()), shader_blob->GetBufferPointer(), shader_blob->GetBufferSize(), &layout));
                    break;
                }
            }

            D3D11_RASTERIZER_DESC rasterizer_desc {};
            rasterizer_desc.FillMode = D3D11_FILL_SOLID;
            switch(desc.cull_mode)
            {
            case rhi::cull_mode::none: rasterizer_desc.CullMode = D3D11_CULL_NONE; break;
            case rhi::cull_mode::back: rasterizer_desc.CullMode = D3D11_CULL_BACK; break;
            case rhi::cull_mode::front: rasterizer_desc.CullMode = D3D11_CULL_FRONT; break;
            }
            rasterizer_desc.FrontCounterClockwise = desc.front_face == rhi::front_face::counter_clockwise;
            rasterizer_desc.DepthClipEnable = TRUE;
            check("ID3D11Device::CreateRasterizerState", device.CreateRasterizerState(&rasterizer_desc, &rasterizer_state));

            D3D11_DEPTH_STENCIL_DESC depth_stencil_desc {};
            if(desc.depth_test)
            {
                depth_stencil_desc.DepthEnable = TRUE;
                depth_stencil_desc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(static_cast<int>(*desc.depth_test)+1);
            }
            depth_stencil_desc.DepthWriteMask = desc.depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            check("ID3D11Device::CreateDepthStencilState", device.CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state));
        }
    };

    ptr<buffer> d3d_device::create_buffer(const buffer_desc & desc, const void * initial_data) { return new delete_when_unreferenced<d3d_buffer>{*dev, *ctx, desc, initial_data}; }
    ptr<sampler> d3d_device::create_sampler(const sampler_desc & desc) { return new delete_when_unreferenced<d3d_sampler>{*dev, desc}; }
    ptr<image> d3d_device::create_image(const image_desc & desc, std::vector<const void *> initial_data) { return new delete_when_unreferenced<d3d_image>{*dev, desc, initial_data}; }
    ptr<framebuffer> d3d_device::create_framebuffer(const framebuffer_desc & desc) { return new delete_when_unreferenced<d3d_framebuffer>{*dev, desc}; }
    ptr<window> d3d_device::create_window(const int2 & dimensions, std::string_view title) { return new delete_when_unreferenced<d3d_window>{*this, dimensions, std::string{title}}; }
    ptr<shader> d3d_device::create_shader(const shader_module & module) { return new delete_when_unreferenced<d3d_shader>{module}; }
    ptr<pipeline> d3d_device::create_pipeline(const pipeline_desc & desc) { return new delete_when_unreferenced<d3d_pipeline>{*dev, desc}; }
}

using namespace rhi;

uint64_t d3d_device::submit(command_buffer & cmd)
{
    d3d_pipeline * current_pipeline = 0;
    ctx->ClearState();
    command_emulator::execute(cmd, overload(
        [this](const generate_mipmaps_command & c)
        {
            ctx->GenerateMips(static_cast<d3d_image &>(*c.im).shader_resource_view);
        },
        [this,&current_pipeline](const begin_render_pass_command & c)
        {
            auto & pass = c.pass;
            auto & fb = static_cast<d3d_framebuffer &>(*c.framebuffer);

            // Clear render targets if specified by render pass
            for(size_t i=0; i<pass.color_attachments.size(); ++i)
            {
                if(auto op = std::get_if<clear_color>(&pass.color_attachments[i].load_op))
                {
                    ctx->ClearRenderTargetView(fb.render_target_views[i], &op->r);
                }
            }
            if(pass.depth_attachment)
            {
                if(auto op = std::get_if<clear_depth>(&pass.depth_attachment->load_op))
                {
                    ctx->ClearDepthStencilView(fb.depth_stencil_view, D3D11_CLEAR_DEPTH|D3D11_CLEAR_DEPTH, op->depth, op->stencil);
                }
            }

            // Set the render targets and viewport
            ctx->OMSetRenderTargets(exactly(fb.render_target_views.size()), fb.render_target_views.data(), fb.depth_stencil_view);
            D3D11_VIEWPORT vp {0, 0, exactly(fb.dims.x), exactly(fb.dims.y), 0, 1};
            ctx->RSSetViewports(1, &vp);
        },
        [this,&current_pipeline](const bind_pipeline_command & c)
        {
            current_pipeline = &static_cast<d3d_pipeline &>(*c.pipe);
            ctx->IASetInputLayout(current_pipeline->layout);
            switch(current_pipeline->desc.topology)
            {
            case primitive_topology::points: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); break;
            case primitive_topology::lines: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); break;
            case primitive_topology::triangles: ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); break;
            }
            ctx->VSSetShader(current_pipeline->vs, nullptr, 0);
            ctx->RSSetState(current_pipeline->rasterizer_state);
            ctx->PSSetShader(current_pipeline->ps, nullptr, 0);      
            ctx->OMSetDepthStencilState(current_pipeline->depth_stencil_state, 0);
        },
        [this](const bind_descriptor_set_command & c)
        {
            descriptor_emulator::bind_descriptor_set(*c.layout, c.set_index, *c.set, [this](size_t index, buffer_range range) 
            { 
                const UINT first_constant = exactly(range.offset/16), num_constants = exactly((range.size+255)/256*16);
                ctx->VSSetConstantBuffers1(exactly(index), 1, &static_cast<d3d_buffer &>(*range.buffer).buffer_object, &first_constant, &num_constants);
                ctx->PSSetConstantBuffers1(exactly(index), 1, &static_cast<d3d_buffer &>(*range.buffer).buffer_object, &first_constant, &num_constants);
            }, [this](size_t index, sampler & sampler, image & image) 
            {
                ctx->VSSetSamplers(exactly(index), 1, &static_cast<d3d_sampler &>(sampler).sampler_state);
                ctx->VSSetShaderResources(exactly(index), 1, &static_cast<d3d_image &>(image).shader_resource_view);
                ctx->PSSetSamplers(exactly(index), 1, &static_cast<d3d_sampler &>(sampler).sampler_state);
                ctx->PSSetShaderResources(exactly(index), 1, &static_cast<d3d_image &>(image).shader_resource_view);
            });
        },
        [this,&current_pipeline](const bind_vertex_buffer_command & c)
        {
            for(auto & buf : current_pipeline->desc.input)
            {
                if(buf.index == c.index)
                {
                    const UINT stride = exactly(buf.stride), offset = exactly(c.range.offset);
                    ctx->IASetVertexBuffers(c.index, 1, &static_cast<d3d_buffer &>(*c.range.buffer).buffer_object, &stride, &offset);
                }
            }      
        },
        [this](const bind_index_buffer_command & c) 
        { 
            ctx->IASetIndexBuffer(static_cast<d3d_buffer &>(*c.range.buffer).buffer_object, DXGI_FORMAT_R32_UINT, exactly(c.range.offset)); 
        },
        [this](const draw_command & c)
        { 
            ctx->Draw(c.vertex_count, c.first_vertex); 
        },
        [this](const draw_indexed_command & c)
        { 
            ctx->DrawIndexed(c.index_count, c.first_index, 0); 
        },
        [this](const end_render_pass_command &) {}
    ));
    if(fence) check("ID3D11DeviceContext4::Signal", ctx->Signal(fence, ++submitted_index));
    return submitted_index;
}

uint64_t d3d_device::acquire_and_submit_and_present(command_buffer & cmd, window & window)
{
    submit(cmd);
    static_cast<d3d_window &>(window).swap_chain->Present(1, 0);
    return submitted_index;
}

void d3d_device::wait_until_complete(uint64_t submit_id)
{ 
    check("ID3D11DeviceContext4::Wait", ctx->Wait(fence, submitted_index)); 
}
