#include "rhi-internal.h"
#include <sstream>
#include <map>

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "vulkan-1.lib")
#include <queue>
#include <set>

auto get_tuple(const VkAttachmentDescription & desc) { return std::tie(desc.flags, desc.format, desc.samples, desc.loadOp, desc.storeOp, desc.stencilLoadOp, desc.stencilStoreOp, desc.initialLayout, desc.finalLayout); }
bool operator < (const VkAttachmentDescription & a, const VkAttachmentDescription & b) { return get_tuple(a) < get_tuple(b); }
bool operator < (const VkAttachmentReference & a, const VkAttachmentReference & b) { return std::tie(a.attachment, a.layout) < std::tie(b.attachment, b.layout); }

namespace rhi
{
    // Initialize tables
    auto convert_vk(shader_stage stage) { switch(stage) { default: fail_fast();
        #define RHI_SHADER_STAGE(CASE, VK, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(address_mode mode) { switch(mode) { default: fail_fast();
        #define RHI_ADDRESS_MODE(CASE, VK, DX, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(primitive_topology topology) { switch(topology) { default: fail_fast();
        #define RHI_PRIMITIVE_TOPOLOGY(CASE, VK, DX, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(cull_mode mode) { switch(mode) { default: fail_fast();
        #define RHI_CULL_MODE(CASE, VK, DX, GL_ENABLED, GL_CULL_FACE) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(compare_op op) { switch(op) { default: fail_fast();
        #define RHI_COMPARE_OP(CASE, VK, DX, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(blend_op op) { switch(op) { default: fail_fast();
        #define RHI_BLEND_OP(CASE, VK, DX, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(blend_factor factor) { switch(factor) { default: fail_fast();
        #define RHI_BLEND_FACTOR(CASE, VK, DX, GL) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(attribute_format format) { switch(format) { default: fail_fast();
        #define RHI_ATTRIBUTE_FORMAT(CASE, VK, DX, GL_SIZE, GL_TYPE, GL_NORMALIZED) case CASE: return VK;
        #include "rhi-tables.inl"
    }}
    auto convert_vk(image_format format) { switch(format) { default: fail_fast();
        #define RHI_IMAGE_FORMAT(CASE, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case CASE: return VK;
        #include "rhi-tables.inl"
    }}

    struct vk_render_pass_desc
    {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> color_refs;
        std::optional<VkAttachmentReference> depth_ref;
    };
    bool operator < (const vk_render_pass_desc & a, const vk_render_pass_desc & b) { return std::tie(a.attachments, a.color_refs, a.depth_ref) < std::tie(b.attachments, b.color_refs, b.depth_ref); }

    struct physical_device_selection
    {
        VkPhysicalDevice physical_device;
        uint32_t queue_family;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        uint32_t swap_image_count;
        VkSurfaceTransformFlagBitsKHR surface_transform;
    };

    struct vk_device : device
    {
        // Core Vulkan objects
        std::function<void(const char *)> debug_callback;
        VkInstance instance {};
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT {};
        VkDebugReportCallbackEXT callback {};
        physical_device_selection selection {};
        VkDevice dev {};
        VkQueue queue {};
        VkPhysicalDeviceProperties device_props {};
        VkPhysicalDeviceMemoryProperties mem_props {};

        // Resources for staging memory
        size_t staging_buffer_size {32*1024*1024};
        VkBuffer staging_buffer {};
        VkDeviceMemory staging_memory {};
        void * mapped_staging_memory {};
        VkCommandPool staging_pool {};

        // Scheduling
        constexpr static int fence_ring_size = 256, fence_ring_mask = 0xFF;
        VkFence ring_fences[fence_ring_size];
        uint64_t submitted_index, completed_index;
        struct scheduled_action { uint64_t after_completion_index; std::function<void(vk_device &)> execute; };
        std::queue<scheduled_action> scheduled_actions;

        // Deduplicated render passes
        std::map<vk_render_pass_desc, VkRenderPass> render_passes;

        vk_device(std::function<void(const char *)> debug_callback);
        ~vk_device();

        // Core helper functions
        void destroy_immediate(VkRenderPass pass) { vkDestroyRenderPass(dev, pass, nullptr); }
        void destroy_immediate(VkDeviceMemory memory) { vkFreeMemory(dev, memory, nullptr); }
        void destroy_immediate(VkBuffer buffer) { vkDestroyBuffer(dev, buffer, nullptr); }
        void destroy_immediate(VkSampler sampler) { vkDestroySampler(dev, sampler, nullptr); }
        void destroy_immediate(VkImage image) { vkDestroyImage(dev, image, nullptr); }
        void destroy_immediate(VkImageView image_view) { vkDestroyImageView(dev, image_view, nullptr); }
        void destroy_immediate(VkFramebuffer framebuffer) { vkDestroyFramebuffer(dev, framebuffer, nullptr); }
        void destroy_immediate(VkDescriptorSetLayout layout) { vkDestroyDescriptorSetLayout(dev, layout, nullptr); }
        void destroy_immediate(VkPipelineLayout layout) { vkDestroyPipelineLayout(dev, layout, nullptr); }
        void destroy_immediate(VkShaderModule module) { vkDestroyShaderModule(dev, module, nullptr); }
        void destroy_immediate(VkPipeline pipeline) { vkDestroyPipeline(dev, pipeline, nullptr); }
        void destroy_immediate(VkDescriptorPool pool) { vkDestroyDescriptorPool(dev, pool, nullptr); }
        void destroy_immediate(VkSemaphore semaphore) { vkDestroySemaphore(dev, semaphore, nullptr); }
        void destroy_immediate(VkFence fence) { vkDestroyFence(dev, fence, nullptr); }
        void destroy_immediate(VkSurfaceKHR surface) { vkDestroySurfaceKHR(instance, surface, nullptr); }
        void destroy_immediate(VkSwapchainKHR swapchain) { vkDestroySwapchainKHR(dev, swapchain, nullptr); }
        void destroy_immediate(GLFWwindow * window) { glfwDestroyWindow(window); }
        template<class T> void destroy(T object) { scheduled_actions.push({submitted_index, [object](vk_device & dev) { dev.destroy_immediate(object); }}); }

        VkDeviceMemory allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props);
        VkRenderPass get_render_pass(const vk_render_pass_desc & desc);
        uint64_t submit(const VkSubmitInfo & submit_info);

        // info
        device_info get_info() const override { return {linalg::zero_to_one, false}; }

        // resources
        ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) override;
        ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) override;
        ptr<sampler> create_sampler(const sampler_desc & desc) override;
        ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) override;
        ptr<window> create_window(const int2 & dimensions, std::string_view title) override;

        ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override;
        ptr<pipeline_layout> create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets) override;
        ptr<shader> create_shader(const shader_desc & desc) override;
        ptr<pipeline> create_pipeline(const pipeline_desc & desc) override;        

        ptr<descriptor_pool> create_descriptor_pool() override;
        ptr<command_buffer> create_command_buffer() override;

        uint64_t submit(command_buffer & cmd) override;
        uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) override;
        uint64_t get_last_submission_id() override { return submitted_index; }
        void wait_until_complete(uint64_t submit_id) override;
    };

    autoregister_backend<vk_device> autoregister_vk_backend {"Vulkan 1.0"};
    
    struct vk_buffer : buffer
    {
        ptr<vk_device> device;
        VkDeviceMemory memory_object;
        VkBuffer buffer_object;        
        char * mapped = 0;

        vk_buffer(vk_device * device, const buffer_desc & desc, const void * initial_data);
        ~vk_buffer();

        size_t get_offset_alignment() override { return exactly(device->device_props.limits.minUniformBufferOffsetAlignment); }
        char * get_mapped_memory() override { return mapped; }
    };

    struct vk_image : image
    {
        ptr<vk_device> device;
        image_desc desc;
        VkDeviceMemory device_memory;
        VkImage image_object;
        VkImageView image_view;
        VkImageCreateInfo image_info;
        VkImageViewCreateInfo view_info;

        vk_image(vk_device * device, const image_desc & desc, std::vector<const void *> initial_data);
        ~vk_image();

        VkImageView create_view(int mip, int layer) const;
    };

    struct vk_sampler : sampler
    {
        ptr<vk_device> device;
        VkSampler sampler;

        vk_sampler(vk_device * device, const sampler_desc & desc);
        ~vk_sampler();
    };

    struct vk_framebuffer : framebuffer
    {
        ptr<vk_device> device;
        std::vector<VkFormat> color_formats;
        std::optional<VkFormat> depth_format;
        int2 dims;

        GLFWwindow * glfw_window {};
        VkSurfaceKHR surface {};
        VkSwapchainKHR swapchain {};

        std::vector<VkImageView> views; // Views belonging to this framebuffer
        std::vector<VkFramebuffer> framebuffers; // If this framebuffer targets a swapchain, the framebuffers for each swapchain image
        uint32_t current_index {0}; // If this framebuffer targets a swapchain, the index of the current backbuffer

        vk_framebuffer(vk_device * device, const framebuffer_desc & desc);
        vk_framebuffer(vk_device * device, vk_image & depth_image, const std::string & title);
        ~vk_framebuffer();

        coord_system get_ndc_coords() const override { return {coord_axis::right, coord_axis::down, coord_axis::forward}; }

        vk_render_pass_desc get_render_pass_desc(const render_pass_desc & desc) const;
    };

    struct vk_window : window
    {
        ptr<vk_device> device;
        ptr<image> depth_image;
        ptr<vk_framebuffer> swapchain_framebuffer;
        VkSemaphore image_available {}, render_finished {};        

        vk_window(vk_device * device, const int2 & dimensions, std::string title);
        ~vk_window();

        GLFWwindow * get_glfw_window() { return swapchain_framebuffer->glfw_window; }
        framebuffer & get_swapchain_framebuffer() { return *swapchain_framebuffer; }
    };

    struct vk_descriptor_set_layout : descriptor_set_layout
    {
        ptr<vk_device> device;
        VkDescriptorSetLayout layout;

        vk_descriptor_set_layout(vk_device * device, const std::vector<descriptor_binding> & bindings);
        ~vk_descriptor_set_layout();
    };

    struct vk_pipeline_layout : pipeline_layout
    {
        ptr<vk_device> device;
        VkPipelineLayout layout;

        vk_pipeline_layout(vk_device * device, const std::vector<descriptor_set_layout *> & sets);
        ~vk_pipeline_layout();
    };

    struct vk_shader : shader
    {
        ptr<vk_device> device;
        VkShaderModule module;
        VkShaderStageFlagBits stage;

        vk_shader(vk_device * device, const shader_desc & desc);
        ~vk_shader();
    };

    struct vk_pipeline : pipeline
    {
        ptr<vk_device> device;
        pipeline_desc desc;
        std::unordered_map<VkRenderPass, VkPipeline> pipeline_objects;

        vk_pipeline(vk_device * device, const pipeline_desc & desc);
        ~vk_pipeline();

        VkPipeline get_pipeline(VkRenderPass render_pass);
    };

    struct vk_descriptor_set : descriptor_set 
    {
        VkDevice device;
        VkDescriptorSet set;

        void write(int binding, buffer_range range) override;
        void write(int binding, sampler & sampler, image & image) override;
    };
    struct vk_descriptor_pool : descriptor_pool 
    {
        ptr<vk_device> device;
        VkDescriptorPool pool;
        std::vector<counted<vk_descriptor_set>> sets;
        size_t used_sets;

        vk_descriptor_pool(vk_device * device);
        ~vk_descriptor_pool();

        void reset() override;
        ptr<descriptor_set> alloc(descriptor_set_layout & layout) override;
    };

    struct vk_command_buffer : command_buffer
    {
        ptr<vk_device> device;
        std::set<ptr<object>> referenced_objects;
        VkCommandBuffer cmd;
        VkRenderPass current_pass;

        void record_reference(object & object);
        virtual void generate_mipmaps(image & image) override;
        virtual void begin_render_pass(const render_pass_desc & desc, framebuffer & framebuffer) override;
        virtual void set_scissor_rect(int x0, int y0, int x1, int y1) override;
        virtual void bind_pipeline(pipeline & pipe) override;
        virtual void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set & set) override;
        virtual void bind_vertex_buffer(int index, buffer_range range) override;
        virtual void bind_index_buffer(buffer_range range) override;
        virtual void draw(int first_vertex, int vertex_count) override;
        virtual void draw_indexed(int first_index, int index_count) override;
        virtual void end_render_pass() override;
    };

    ptr<buffer> vk_device::create_buffer(const buffer_desc & desc, const void * initial_data) { return new delete_when_unreferenced<vk_buffer>{this, desc, initial_data}; }
    ptr<image> vk_device::create_image(const image_desc & desc, std::vector<const void *> initial_data) { return new delete_when_unreferenced<vk_image>{this, desc, initial_data}; }
    ptr<sampler> vk_device::create_sampler(const sampler_desc & desc) { return new delete_when_unreferenced<vk_sampler>{this, desc}; }
    ptr<framebuffer> vk_device::create_framebuffer(const framebuffer_desc & desc) { return new delete_when_unreferenced<vk_framebuffer>{this, desc}; }
    ptr<window> vk_device::create_window(const int2 & dimensions, std::string_view title) { return new delete_when_unreferenced<vk_window>{this, dimensions, std::string{title}}; }

    ptr<descriptor_set_layout> vk_device::create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) { return new delete_when_unreferenced<vk_descriptor_set_layout>{this, bindings}; }
    ptr<pipeline_layout> vk_device::create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets) { return new delete_when_unreferenced<vk_pipeline_layout>{this, sets}; }
    ptr<shader> vk_device::create_shader(const shader_desc & desc) { return new delete_when_unreferenced<vk_shader>{this, desc}; }
    ptr<pipeline> vk_device::create_pipeline(const pipeline_desc & desc) { return new delete_when_unreferenced<vk_pipeline>{this, desc}; }

    ptr<descriptor_pool> vk_device::create_descriptor_pool() { return new delete_when_unreferenced<vk_descriptor_pool>{this}; }
}

namespace rhi
{
    struct vulkan_error : public std::error_category
    {
        const char * name() const noexcept override { return "VkResult"; }
        std::string message(int value) const override { return std::to_string(value); }
        static const std::error_category & instance() { static vulkan_error inst; return inst; }
    };
    void check(const char * func, VkResult result)
    {
        if(result != VK_SUCCESS)
        {
            throw std::system_error(std::error_code(exactly(static_cast<std::underlying_type_t<VkResult>>(result)), vulkan_error::instance()), to_string(func, "(...) failed"));
        }
    }

    static bool has_extension(const std::vector<VkExtensionProperties> & extensions, std::string_view name)
    {
        return std::find_if(begin(extensions), end(extensions), [name](const VkExtensionProperties & p) { return p.extensionName == name; }) != end(extensions);
    }

    static physical_device_selection select_physical_device(VkInstance instance, const std::vector<const char *> & required_extensions)
    {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, 0);
        GLFWwindow * example_window = glfwCreateWindow(256, 256, "", nullptr, nullptr);
        VkSurfaceKHR example_surface = 0;
        check("glfwCreateWindowSurface", glfwCreateWindowSurface(instance, example_window, nullptr, &example_surface));

        uint32_t device_count = 0;
        check("vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
        std::vector<VkPhysicalDevice> physical_devices(device_count);
        check("vkEnumeratePhysicalDevices", vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()));
        for(auto & d : physical_devices)
        {
            // Skip physical devices which do not support our desired extensions
            uint32_t extension_count = 0;
            check("vkEnumerateDeviceExtensionProperties", vkEnumerateDeviceExtensionProperties(d, 0, &extension_count, nullptr));
            std::vector<VkExtensionProperties> extensions(extension_count);
            check("vkEnumerateDeviceExtensionProperties", vkEnumerateDeviceExtensionProperties(d, 0, &extension_count, extensions.data()));
            for(auto req : required_extensions) if(!has_extension(extensions, req)) continue;

            // Skip physical devices who do not support at least one format and present mode for our example surface
            VkSurfaceCapabilitiesKHR surface_caps;
            check("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR(d, example_surface, &surface_caps));
            uint32_t surface_format_count = 0, present_mode_count = 0;
            check("vkGetPhysicalDeviceSurfaceFormatsKHR", vkGetPhysicalDeviceSurfaceFormatsKHR(d, example_surface, &surface_format_count, nullptr));
            std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
            check("vkGetPhysicalDeviceSurfaceFormatsKHR", vkGetPhysicalDeviceSurfaceFormatsKHR(d, example_surface, &surface_format_count, surface_formats.data()));
            check("vkGetPhysicalDeviceSurfacePresentModesKHR", vkGetPhysicalDeviceSurfacePresentModesKHR(d, example_surface, &present_mode_count, nullptr));
            std::vector<VkPresentModeKHR> surface_present_modes(present_mode_count);
            check("vkGetPhysicalDeviceSurfacePresentModesKHR", vkGetPhysicalDeviceSurfacePresentModesKHR(d, example_surface, &present_mode_count, surface_present_modes.data()));
            if(surface_formats.empty() || surface_present_modes.empty()) continue;
        
            // Select a format
            VkSurfaceFormatKHR surface_format = surface_formats[0];
            for(auto f : surface_formats) if(f.format==VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) surface_format = f;
            if(surface_format.format == VK_FORMAT_UNDEFINED) surface_format = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

            // Select a presentation mode
            VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
            for(auto mode : surface_present_modes) if(mode == VK_PRESENT_MODE_MAILBOX_KHR) present_mode = mode;

            // Look for a queue family that supports both graphics and presentation to our example surface
            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(d, &queue_family_count, nullptr);
            std::vector<VkQueueFamilyProperties> queue_family_props(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(d, &queue_family_count, queue_family_props.data());
            for(uint32_t i=0; i<queue_family_props.size(); ++i)
            {
                VkBool32 present = VK_FALSE;
                check("vkGetPhysicalDeviceSurfaceSupportKHR", vkGetPhysicalDeviceSurfaceSupportKHR(d, i, example_surface, &present));
                if((queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) 
                {
                    vkDestroySurfaceKHR(instance, example_surface, nullptr);
                    glfwDestroyWindow(example_window);
                    return {d, i, surface_format, present_mode, std::min(surface_caps.minImageCount+1, surface_caps.maxImageCount), surface_caps.currentTransform};
                }
            }
        }
        throw std::runtime_error("no suitable Vulkan device present");
    }
}

////////////////
// vk_device //
////////////////

using namespace rhi;

vk_device::vk_device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
{ 
    uint32_t extension_count = 0;
    auto ext = glfwGetRequiredInstanceExtensions(&extension_count);
    std::vector<const char *> extensions{ext, ext+extension_count};

    const VkApplicationInfo app_info {VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "simple-scene", VK_MAKE_VERSION(1,0,0), "No Engine", VK_MAKE_VERSION(0,0,0), VK_API_VERSION_1_0};
    const char * layers[] {"VK_LAYER_LUNARG_standard_validation"};
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    const VkInstanceCreateInfo instance_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, {}, &app_info, 1, layers, exactly(extensions.size()), extensions.data()};
    check("vkCreateInstance", vkCreateInstance(&instance_info, nullptr, &instance));
    auto vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
    vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

    const VkDebugReportCallbackCreateInfoEXT callback_info {VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT, nullptr, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, 
        [](VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location, int32_t message_code, const char * layer_prefix, const char * message, void * user_data) -> VkBool32
        {
            auto & ctx = *reinterpret_cast<vk_device *>(user_data);
            if(ctx.debug_callback) ctx.debug_callback(message);
            return VK_FALSE;
        }, this};
    check("vkCreateDebugReportCallbackEXT", vkCreateDebugReportCallbackEXT(instance, &callback_info, nullptr, &callback));

    std::vector<const char *> device_extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    selection = select_physical_device(instance, device_extensions);
    const float queue_priorities[] {1.0f};
    const VkDeviceQueueCreateInfo queue_infos[] {{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, {}, selection.queue_family, 1, queue_priorities}};
    const VkDeviceCreateInfo device_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, {}, 1, queue_infos, 1, layers, exactly(device_extensions.size()), device_extensions.data()};
    check("vkCreateDevice", vkCreateDevice(selection.physical_device, &device_info, nullptr, &dev));
    vkGetDeviceQueue(dev, selection.queue_family, 0, &queue);
    vkGetPhysicalDeviceProperties(selection.physical_device, &device_props);
    vkGetPhysicalDeviceMemoryProperties(selection.physical_device, &mem_props);

    // Set up staging buffer
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = staging_buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check("vkCreateBuffer", vkCreateBuffer(dev, &buffer_info, nullptr, &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &mem_reqs);
    staging_memory = allocate(mem_reqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check("vkBindBufferMemory", vkBindBufferMemory(dev, staging_buffer, staging_memory, 0));
    check("vkMapMemory", vkMapMemory(dev, staging_memory, 0, buffer_info.size, 0, &mapped_staging_memory));
        
    VkCommandPoolCreateInfo command_pool_info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.queueFamilyIndex = selection.queue_family; // TODO: Could use an explicit transfer queue
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check("vkCreateCommandPool", vkCreateCommandPool(dev, &command_pool_info, nullptr, &staging_pool));

    // Initialize fence ring
    const VkFenceCreateInfo fence_info {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    for(auto & fence : ring_fences) check("vkCreateFence", vkCreateFence(dev, &fence_info, nullptr, &fence));
    submitted_index = completed_index = 0;
}

vk_device::~vk_device()
{
    // Flush our queue
    wait_until_complete(submitted_index);
    for(auto & fence : ring_fences) destroy_immediate(fence);
    for(auto & pair : render_passes) destroy_immediate(pair.second);

    // NOTE: We expect the higher level software layer to ensure that all API objects have been destroyed by this point
    vkDestroyCommandPool(dev, staging_pool, nullptr);
    vkDestroyBuffer(dev, staging_buffer, nullptr);
    vkUnmapMemory(dev, staging_memory);
    vkFreeMemory(dev, staging_memory, nullptr);

    vkDestroyDevice(dev, nullptr);
    vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VkDeviceMemory vk_device::allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props)
{
    for(uint32_t i=0; i<mem_props.memoryTypeCount; ++i)
    {
        if(reqs.memoryTypeBits & (1 << i) && (mem_props.memoryTypes[i].propertyFlags & props) == props)
        {
            VkMemoryAllocateInfo alloc_info {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = reqs.size;
            alloc_info.memoryTypeIndex = i;

            VkDeviceMemory memory;
            check("vkAllocateMemory", vkAllocateMemory(dev, &alloc_info, nullptr, &memory));
            return memory;
        }
    }
    throw std::runtime_error("no suitable memory type");
}

VkRenderPass vk_device::get_render_pass(const vk_render_pass_desc & desc)
{
    auto & pass = render_passes[desc];
    if(!pass)
    {
        VkSubpassDescription subpass_desc {};
        subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_desc.colorAttachmentCount = exactly(countof(desc.color_refs));
        subpass_desc.pColorAttachments = desc.color_refs.data();
        subpass_desc.pDepthStencilAttachment = desc.depth_ref ? &*desc.depth_ref : nullptr;

        VkRenderPassCreateInfo render_pass_info {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        render_pass_info.attachmentCount = exactly(countof(desc.attachments));
        render_pass_info.pAttachments = desc.attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass_desc;

        check("vkCreateRenderPass", vkCreateRenderPass(dev, &render_pass_info, nullptr, &pass));
    }
    return pass;
}

/////////////////////////
// vk_device resources //
/////////////////////////

vk_buffer::vk_buffer(vk_device * device, const buffer_desc & desc, const void * initial_data) : device{device}
{
    // Create buffer with appropriate usage flags
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = desc.size;
    if(desc.flags & rhi::vertex_buffer_bit) buffer_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(desc.flags & rhi::index_buffer_bit) buffer_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(desc.flags & rhi::uniform_buffer_bit) buffer_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if(desc.flags & rhi::storage_buffer_bit) buffer_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(initial_data) buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check("vkCreateBuffer", vkCreateBuffer(device->dev, &buffer_info, nullptr, &buffer_object));

    // Obtain and bind memory (TODO: Pool allocations)
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device->dev, buffer_object, &mem_reqs);
    VkMemoryPropertyFlags memory_property_flags = 0;
    if(desc.flags & rhi::mapped_memory_bit) memory_property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    memory_object = device->allocate(mem_reqs, memory_property_flags);
    check("vkBindBufferMemory", vkBindBufferMemory(device->dev, buffer_object, memory_object, 0));

    // Initialize memory if requested to do so
    if(initial_data)
    {
        memcpy(device->mapped_staging_memory, initial_data, desc.size);
        auto cmd = device->create_command_buffer();
        const VkBufferCopy copy {0, 0, desc.size};
        vkCmdCopyBuffer(static_cast<vk_command_buffer &>(*cmd).cmd, device->staging_buffer, buffer_object, 1, &copy);
        device->submit(*cmd);
    }

    // Map memory if requested to do so
    if(desc.flags & rhi::mapped_memory_bit)
    {
        check("vkMapMemory", vkMapMemory(device->dev, memory_object, 0, desc.size, 0, reinterpret_cast<void**>(&mapped)));
    }        
}

vk_buffer::~vk_buffer()
{
    if(mapped) vkUnmapMemory(device->dev, memory_object);
    device->destroy(buffer_object);
    device->destroy(memory_object);
}

static void transition_image(VkCommandBuffer command_buffer, VkImage image, uint32_t mip_level, uint32_t array_layer, 
    VkPipelineStageFlags src_stage_mask, VkAccessFlags src_access_mask, VkImageLayout old_layout,
    VkPipelineStageFlags dst_stage_mask, VkAccessFlags dst_access_mask, VkImageLayout new_layout)
{
    const VkImageMemoryBarrier barriers[] {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, 0, 
        src_access_mask, dst_access_mask, old_layout, new_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, 
        image, {VK_IMAGE_ASPECT_COLOR_BIT, mip_level, 1, array_layer, 1}};
    vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, exactly(countof(barriers)), barriers);
}

vk_image::vk_image(vk_device * device, const image_desc & desc, std::vector<const void *> initial_data) : device{device}, desc{desc}
{
    image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.arrayLayers = 1;
    VkImageViewType view_type;
    switch(desc.shape)
    {
    case image_shape::_1d: image_info.imageType = VK_IMAGE_TYPE_1D; view_type = VK_IMAGE_VIEW_TYPE_1D; break;
    case image_shape::_2d: image_info.imageType = VK_IMAGE_TYPE_2D; view_type = VK_IMAGE_VIEW_TYPE_2D;break;
    case image_shape::_3d: image_info.imageType = VK_IMAGE_TYPE_3D; view_type = VK_IMAGE_VIEW_TYPE_3D;break;
    case image_shape::cube: 
        image_info.imageType = VK_IMAGE_TYPE_2D; 
        view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        image_info.arrayLayers *= 6;
        image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    default: fail_fast();
    }

    image_info.format = convert_vk(desc.format);
    image_info.extent = {static_cast<uint32_t>(desc.dimensions.x), static_cast<uint32_t>(desc.dimensions.y), static_cast<uint32_t>(desc.dimensions.z)};
    image_info.mipLevels = desc.mip_levels;        
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(image_info.mipLevels > 1) image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(desc.flags & image_flag::sampled_image_bit) image_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if(desc.flags & image_flag::color_attachment_bit) image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(desc.flags & image_flag::depth_attachment_bit) image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    check("vkCreateImage", vkCreateImage(device->dev, &image_info, nullptr, &image_object));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device->dev, image_object, &mem_reqs);
    device_memory = device->allocate(mem_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check("vkBindImageMemory", vkBindImageMemory(device->dev, image_object, device_memory, 0));
    
    if(initial_data.size())
    {
        if(initial_data.size() != image_info.arrayLayers) throw std::logic_error("not enough initial_data pointers");
        for(size_t layer=0; layer<initial_data.size(); ++layer) 
        {
            // Write initial data for this layer into the staging area
            const size_t image_size = get_pixel_size(desc.format) * product(desc.dimensions);
            if(image_size > device->staging_buffer_size) throw std::runtime_error("staging buffer exhausted");
            memcpy(device->mapped_staging_memory, initial_data[layer], image_size);

            VkImageSubresourceLayers layers {};
            layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            layers.baseArrayLayer = exactly(layer);
            layers.layerCount = 1;

            // Copy image contents from staging buffer into mip level zero
            auto cmd = device->create_command_buffer();

            // Must transition to transfer_dst_optimal before any transfers occur
            transition_image(static_cast<vk_command_buffer &>(*cmd).cmd, image_object, 0, exactly(layer), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            // Copy to the image
            VkBufferImageCopy copy_region {};
            copy_region.imageSubresource = layers;
            copy_region.imageExtent = image_info.extent;
            vkCmdCopyBufferToImage(static_cast<vk_command_buffer &>(*cmd).cmd, device->staging_buffer, image_object, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            // After transfer finishes, transition to shader_read_only_optimal, and complete that before any shaders execute
            transition_image(static_cast<vk_command_buffer &>(*cmd).cmd, image_object, 0, exactly(layer), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            device->submit(*cmd);
        }
    }

    // Create a view of the overall object
    view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = image_object;
    view_info.viewType = view_type;
    view_info.format = image_info.format;
    if(desc.flags & image_flag::depth_attachment_bit) view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = image_info.mipLevels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = image_info.arrayLayers;
    check("vkCreateImageView", vkCreateImageView(device->dev, &view_info, nullptr, &image_view));
}

vk_image::~vk_image()
{
    device->destroy(image_view);
    device->destroy(image_object);
    device->destroy(device_memory);
}

VkImageView vk_image::create_view(int mip, int layer) const
{
    auto info = view_info;
    switch(info.viewType)
    {
    case VK_IMAGE_VIEW_TYPE_CUBE: info.viewType = VK_IMAGE_VIEW_TYPE_2D; break;
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY: info.viewType = VK_IMAGE_VIEW_TYPE_1D; break;
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY: info.viewType = VK_IMAGE_VIEW_TYPE_2D; break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: info.viewType = VK_IMAGE_VIEW_TYPE_2D; break;
    }
    info.subresourceRange.baseMipLevel = exactly(mip);
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = exactly(layer);
    info.subresourceRange.layerCount = 1;
    VkImageView view;
    check("vkCreateImageView", vkCreateImageView(device->dev, &info, nullptr, &view));
    return view;
}

vk_sampler::vk_sampler(vk_device * device, const sampler_desc & desc) : device{device}
{
    auto convert_filter = [](rhi::filter filter)
    {
        switch(filter)
        {
        case rhi::filter::nearest: return VK_FILTER_NEAREST;
        case rhi::filter::linear: return VK_FILTER_LINEAR;
        default: fail_fast();
        }
    };
    auto convert_mipmap_mode = [](rhi::filter filter)
    {
        switch(filter)
        {
        case rhi::filter::nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case rhi::filter::linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default: fail_fast();
        }
    };

    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.addressModeU = convert_vk(desc.wrap_s);
    sampler_info.addressModeV = convert_vk(desc.wrap_t);
    sampler_info.addressModeW = convert_vk(desc.wrap_r);
    sampler_info.magFilter = convert_filter(desc.mag_filter);
    sampler_info.minFilter = convert_filter(desc.min_filter);
    sampler_info.maxAnisotropy = 1.0;
    if(desc.mip_filter)
    {
        sampler_info.maxLod = 1000;
        sampler_info.mipmapMode = convert_mipmap_mode(*desc.mip_filter);
    }
    check("vkCreateSampler", vkCreateSampler(device->dev, &sampler_info, nullptr, &sampler));
}
vk_sampler::~vk_sampler()
{
    device->destroy(sampler);
}

vk_framebuffer::vk_framebuffer(vk_device * device, const framebuffer_desc & desc) : device{device}
{
    dims = desc.dimensions;
    framebuffers.push_back(VK_NULL_HANDLE);
    rhi::render_pass_desc pass_desc;
    for(auto & color_attachment : desc.color_attachments) 
    {
        color_formats.push_back(convert_vk(static_cast<vk_image &>(*color_attachment.image).desc.format));
        views.push_back(static_cast<vk_image &>(*color_attachment.image).create_view(color_attachment.mip, color_attachment.layer));
        pass_desc.color_attachments.push_back({dont_care{}, dont_care{}});
        
    }
    if(desc.depth_attachment) 
    {
        depth_format = convert_vk(static_cast<vk_image &>(*desc.depth_attachment->image).desc.format);
        views.push_back(static_cast<vk_image &>(*desc.depth_attachment->image).create_view(desc.depth_attachment->mip, desc.depth_attachment->layer));
        pass_desc.depth_attachment = {dont_care{}, dont_care{}};
    }
    
    VkFramebufferCreateInfo framebuffer_info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_info.renderPass = device->get_render_pass(get_render_pass_desc(pass_desc));
    framebuffer_info.attachmentCount = exactly(views.size());
    framebuffer_info.pAttachments = views.data();
    framebuffer_info.width = exactly(desc.dimensions.x);
    framebuffer_info.height = exactly(desc.dimensions.y);
    framebuffer_info.layers = 1;
    check("vkCreateFramebuffer", vkCreateFramebuffer(device->dev, &framebuffer_info, nullptr, &framebuffers[0]));
}

vk_framebuffer::vk_framebuffer(vk_device * device, vk_image & depth_image, const std::string & title) : device{device} 
{
    const int2 dimensions = depth_image.desc.dimensions.xy();
    const auto format = rhi::image_format::rgba_srgb8;    

    const std::string buffer {begin(title), end(title)};
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfw_window = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);

    check("glfwCreateWindowSurface", glfwCreateWindowSurface(device->instance, glfw_window, nullptr, &surface));

    VkBool32 present = VK_FALSE;
    check("vkGetPhysicalDeviceSurfaceSupportKHR", vkGetPhysicalDeviceSurfaceSupportKHR(device->selection.physical_device, device->selection.queue_family, surface, &present));
    if(!present) throw std::runtime_error("vkGetPhysicalDeviceSurfaceSupportKHR(...) inconsistent");

    // Determine swap extent
    VkExtent2D swap_extent {static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y)};
    VkSurfaceCapabilitiesKHR surface_caps;
    check("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->selection.physical_device, surface, &surface_caps));
    swap_extent.width = std::min(std::max(swap_extent.width, surface_caps.minImageExtent.width), surface_caps.maxImageExtent.width);
    swap_extent.height = std::min(std::max(swap_extent.height, surface_caps.minImageExtent.height), surface_caps.maxImageExtent.height);

    VkSwapchainCreateInfoKHR swapchain_info {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = device->selection.swap_image_count;
    swapchain_info.imageFormat = convert_vk(format);
    swapchain_info.imageColorSpace = VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; //selection.surface_format.colorSpace;
    swapchain_info.imageExtent = swap_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = device->selection.surface_transform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = device->selection.present_mode;
    swapchain_info.clipped = VK_TRUE;

    check("vkCreateSwapchainKHR", vkCreateSwapchainKHR(device->dev, &swapchain_info, nullptr, &swapchain));    

    uint32_t swapchain_image_count;    
    check("vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR(device->dev, swapchain, &swapchain_image_count, nullptr));
    std::vector<VkImage> swapchain_images(swapchain_image_count);
    check("vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR(device->dev, swapchain, &swapchain_image_count, swapchain_images.data()));

    views.resize(swapchain_image_count);
    for(uint32_t i=0; i<swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo view_info {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = convert_vk(format);
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        check("vkCreateImageView", vkCreateImageView(device->dev, &view_info, nullptr, &views[i]));
    }
    
    // Create swapchain framebuffer
    dims = dimensions;
    framebuffers.resize(views.size());
    color_formats = {convert_vk(format)};
    depth_format = convert_vk(rhi::image_format::depth_float32);
    auto render_pass = device->get_render_pass(get_render_pass_desc({{{dont_care{}, dont_care{}}}, depth_attachment_desc{dont_care{}, dont_care{}}}));
    for(size_t i=0; i<views.size(); ++i)
    {
        std::vector<VkImageView> attachments {views[i], depth_image.image_view};
        VkFramebufferCreateInfo framebuffer_info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = exactly(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = dimensions.x;
        framebuffer_info.height = dimensions.y;
        framebuffer_info.layers = 1;
        check("vkCreateFramebuffer", vkCreateFramebuffer(device->dev, &framebuffer_info, nullptr, &framebuffers[i]));
    }
}

vk_framebuffer::~vk_framebuffer()
{
    for(auto fb : framebuffers) device->destroy(fb);
    for(auto view : views) device->destroy(view);
    device->destroy(swapchain);
    device->destroy(surface);
    device->destroy(glfw_window);
}

template<class T> static VkAttachmentDescription make_attachment_description(VkFormat format, const T & desc, VkImageLayout attachment_optimal_layout)
{
    VkAttachmentDescription attachment {0, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, attachment_optimal_layout};
    auto convert_layout = [=](rhi::layout layout)
    {
        switch(layout)
        {
        case rhi::layout::attachment_optimal: return attachment_optimal_layout;
        case rhi::layout::shader_read_only_optimal: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case rhi::layout::present_source: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: fail_fast();
        }
    };
    auto visitor = overload(
        [](dont_care) {},
        [&](clear_color) { attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; },
        [&](clear_depth) { attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; },
        [&](load load) { attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; attachment.initialLayout = convert_layout(load.initial_layout); },
        [&](store store) { attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; attachment.finalLayout = convert_layout(store.final_layout); }
    );
    std::visit(visitor, desc.load_op);
    std::visit(visitor, desc.store_op);
    return attachment;
}

vk_render_pass_desc vk_framebuffer::get_render_pass_desc(const render_pass_desc & desc) const
{
    vk_render_pass_desc r;
    if(desc.color_attachments.size() != color_formats.size()) throw std::logic_error("render pass color attachments mismatch");
    if(desc.depth_attachment.has_value() != depth_format.has_value()) throw std::logic_error("render pass color attachments mismatch");
    for(size_t i=0; i<desc.color_attachments.size(); ++i)
    {
        r.color_refs.push_back({exactly(r.attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        r.attachments.push_back(make_attachment_description(color_formats[i], desc.color_attachments[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }
    if(desc.depth_attachment)
    {
        r.depth_ref = {exactly(r.attachments.size()), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        r.attachments.push_back(make_attachment_description(*depth_format, *desc.depth_attachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
    }
    return r;
}

vk_window::vk_window(vk_device * device, const int2 & dimensions, std::string title) : device{device}
{
    depth_image = device->create_image({rhi::image_shape::_2d, {dimensions,1}, 1, rhi::image_format::depth_float32, rhi::depth_attachment_bit}, {});
    swapchain_framebuffer = new delete_when_unreferenced<vk_framebuffer>{device, static_cast<vk_image &>(*depth_image), title};

    VkSemaphoreCreateInfo semaphore_info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check("vkCreateSemaphore", vkCreateSemaphore(device->dev, &semaphore_info, nullptr, &image_available));
    check("vkCreateSemaphore", vkCreateSemaphore(device->dev, &semaphore_info, nullptr, &render_finished));    

    // Acquire the first image
    check("vkAcquireNextImageKHR", vkAcquireNextImageKHR(device->dev, swapchain_framebuffer->swapchain, std::numeric_limits<uint64_t>::max(), image_available, VK_NULL_HANDLE, &swapchain_framebuffer->current_index));
}
vk_window::~vk_window()
{ 
    device->destroy(render_finished);
    device->destroy(image_available);
}

vk_descriptor_set_layout::vk_descriptor_set_layout(vk_device * device, const std::vector<descriptor_binding> & bindings) : device{device}
{ 
    std::vector<VkDescriptorSetLayoutBinding> set_bindings(bindings.size());
    for(size_t i=0; i<bindings.size(); ++i)
    {
        set_bindings[i].binding = bindings[i].index;
        switch(bindings[i].type)
        {
        case descriptor_type::combined_image_sampler: set_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
        case descriptor_type::uniform_buffer: set_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
        }
        set_bindings[i].descriptorCount = bindings[i].count;
        set_bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    }
        
    VkDescriptorSetLayoutCreateInfo create_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    create_info.bindingCount = exactly(set_bindings.size());
    create_info.pBindings = set_bindings.data();
    check("vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout(device->dev, &create_info, nullptr, &layout));
}
vk_descriptor_set_layout::~vk_descriptor_set_layout()
{ 
    device->destroy(layout);
}

vk_pipeline_layout::vk_pipeline_layout(vk_device * device, const std::vector<descriptor_set_layout *> & sets) : device{device}
{ 
    std::vector<VkDescriptorSetLayout> set_layouts;
    for(auto s : sets) set_layouts.push_back(static_cast<vk_descriptor_set_layout *>(s)->layout);

    VkPipelineLayoutCreateInfo create_info {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    create_info.setLayoutCount = exactly(set_layouts.size());
    create_info.pSetLayouts = set_layouts.data();
    check("vkCreatePipelineLayout", vkCreatePipelineLayout(device->dev, &create_info, nullptr, &layout));
}
vk_pipeline_layout::~vk_pipeline_layout()
{
    device->destroy(layout);
}

vk_shader::vk_shader(vk_device * device, const shader_desc & desc) : device{device}
{
    VkShaderModuleCreateInfo create_info {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = desc.spirv.size() * sizeof(uint32_t);
    create_info.pCode = desc.spirv.data();
    check("vkCreateShaderModule", vkCreateShaderModule(device->dev, &create_info, nullptr, &this->module));
    stage = convert_vk(desc.stage);
}
vk_shader::~vk_shader()
{
    device->destroy(module);
}

vk_pipeline::vk_pipeline(vk_device * device, const pipeline_desc & desc) : device{device}, desc{desc} {}

VkPipeline vk_pipeline::get_pipeline(VkRenderPass render_pass)
{
    auto & pipe = pipeline_objects[render_pass];
    if(pipe) return pipe;

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
    for(auto & s : desc.stages)
    {
        auto & shader = static_cast<vk_shader &>(*s);
        shader_stages.push_back({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, shader.stage, shader.module, "main"});
    };
            
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    for(auto & b : desc.input)
    {
        bindings.push_back({(uint32_t)b.index, (uint32_t)b.stride, VK_VERTEX_INPUT_RATE_VERTEX});
        for(auto & a : b.attributes) attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, convert_vk(a.type), (uint32_t)a.offset});
    }
    const VkPipelineVertexInputStateCreateInfo vertex_input_state {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, exactly(bindings.size()), bindings.data(), exactly(attributes.size()), attributes.data()};

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, convert_vk(desc.topology)};

    const VkPipelineViewportStateCreateInfo viewport_state {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};

    VkPipelineRasterizationStateCreateInfo rasterization_state {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state.cullMode = convert_vk(desc.cull_mode);
    rasterization_state.frontFace = (desc.front_face == rhi::front_face::clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE);
    rasterization_state.lineWidth = 1.0f;

    const VkPipelineMultisampleStateCreateInfo multisample_state {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT};

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil_state.depthWriteEnable = desc.depth_write ? VK_TRUE : VK_FALSE;
    if(desc.depth_test)
    {
        depth_stencil_state.depthTestEnable = VK_TRUE;
        depth_stencil_state.depthCompareOp = static_cast<VkCompareOp>(*desc.depth_test);
    }

    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    for(auto & b : desc.blend) blend_attachments.push_back({b.enable, convert_vk(b.color.source_factor), convert_vk(b.color.dest_factor), convert_vk(b.color.op), convert_vk(b.alpha.source_factor), convert_vk(b.alpha.dest_factor), convert_vk(b.alpha.op), 0xF});
    const VkPipelineColorBlendStateCreateInfo color_blend_state {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, exactly(countof(blend_attachments)), blend_attachments.data(), {0,0,0,0}};

    const VkDynamicState dynamic_states[] {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic_state {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, exactly(countof(dynamic_states)), dynamic_states};

    const VkGraphicsPipelineCreateInfo pipelineInfo {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0,
        exactly(countof(shader_stages)), shader_stages.data(),
        &vertex_input_state,
        &input_assembly_state,
        nullptr,
        &viewport_state,
        &rasterization_state,
        &multisample_state,
        &depth_stencil_state,
        &color_blend_state,
        &dynamic_state,
        static_cast<vk_pipeline_layout &>(*desc.layout).layout,
        render_pass, 0,
        VK_NULL_HANDLE, -1
    };
    check("vkCreateGraphicsPipelines", vkCreateGraphicsPipelines(device->dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe));
    return pipe;
}
vk_pipeline::~vk_pipeline()
{
    for(auto & pass_to_pipe : pipeline_objects) device->destroy(pass_to_pipe.second);
}

void vk_descriptor_set::write(int binding, buffer_range range) 
{
    VkDescriptorBufferInfo buffer_info { static_cast<vk_buffer &>(range.buffer).buffer_object, range.offset, range.size };
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, exactly(binding), 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &buffer_info, nullptr};
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}
void vk_descriptor_set::write(int binding, sampler & sampler, image & image) 
{
    VkDescriptorImageInfo image_info {static_cast<vk_sampler &>(sampler).sampler, static_cast<vk_image &>(image).image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, exactly(binding), 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_info, nullptr, nullptr};
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

vk_descriptor_pool::vk_descriptor_pool(vk_device * device) : device{device}, sets{1024}, used_sets{0}
{ 
    const VkDescriptorPoolSize pool_sizes[] {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1024}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024}};
    VkDescriptorPoolCreateInfo descriptor_pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptor_pool_info.poolSizeCount = 2;
    descriptor_pool_info.pPoolSizes = pool_sizes;
    descriptor_pool_info.maxSets = 1024;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    check("vkCreateDescriptorPool", vkCreateDescriptorPool(device->dev, &descriptor_pool_info, nullptr, &pool));
    for(auto & set : sets) set.device = device->dev;
}
vk_descriptor_pool::~vk_descriptor_pool()
{
    device->destroy(pool);
}

void vk_descriptor_pool::reset()
{
    for(size_t i=0; i<used_sets; ++i)
    {
        if(sets[i].ref_count != 0) throw std::logic_error("rhi::descriptor_pool::reset called with descriptor sets outstanding");
        vkFreeDescriptorSets(device->dev, pool, 1, &sets[i].set);
    }
    vkResetDescriptorPool(device->dev, pool, 0);
    used_sets = 0;
}
ptr<descriptor_set> vk_descriptor_pool::alloc(descriptor_set_layout & layout) 
{ 
    if(used_sets == sets.size()) throw std::logic_error("out of descriptor sets");
    VkDescriptorSetAllocateInfo alloc_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &static_cast<vk_descriptor_set_layout &>(layout).layout;
    check("vkAllocateDescriptorSets", vkAllocateDescriptorSets(device->dev, &alloc_info, &sets[used_sets].set));
    return &sets[used_sets++];
}

//////////////////////////
// vk_device rendering //
//////////////////////////

ptr<command_buffer> vk_device::create_command_buffer()
{
    ptr<vk_command_buffer> cmd {new delete_when_unreferenced<vk_command_buffer>{}};
    cmd->device = this;
    
    VkCommandBufferAllocateInfo alloc_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = staging_pool;
    alloc_info.commandBufferCount = 1;
    check("vkAllocateCommandBuffers", vkAllocateCommandBuffers(dev, &alloc_info, &cmd->cmd));

    VkCommandBufferBeginInfo begin_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check("vkBeginCommandBuffer", vkBeginCommandBuffer(cmd->cmd, &begin_info));
    cmd->current_pass = nullptr;
    return cmd;
}

void vk_command_buffer::record_reference(object & object)
{
    auto it = referenced_objects.find(&object);
    if(it == referenced_objects.end()) referenced_objects.insert(&object);
}

void vk_command_buffer::generate_mipmaps(image & image)
{
    record_reference(image);
    auto & im = static_cast<vk_image &>(image);
    for(uint32_t layer=0; layer<im.image_info.arrayLayers; ++layer)
    {
        VkImageSubresourceLayers layers {};
        layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        layers.baseArrayLayer = exactly(layer);
        layers.layerCount = 1;
        VkOffset3D dims {exactly(im.image_info.extent.width), exactly(im.image_info.extent.height), exactly(im.image_info.extent.depth)};
        
        transition_image(cmd, im.image_object, 0, exactly(layer), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        for(uint32_t i=1; i<im.image_info.mipLevels; ++i)
        {
            VkImageBlit blit {};
            blit.srcSubresource = layers;
            blit.srcSubresource.mipLevel = i-1;
            blit.srcOffsets[1] = dims;

            dims.x = std::max(dims.x/2,1);
            dims.y = std::max(dims.y/2,1);
            dims.z = std::max(dims.z/2,1);
            blit.dstSubresource = layers;
            blit.dstSubresource.mipLevel = i;
            blit.dstOffsets[1] = dims;

            transition_image(cmd, im.image_object, i, exactly(layer), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            vkCmdBlitImage(cmd, im.image_object, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, im.image_object, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            transition_image(cmd, im.image_object, i, exactly(layer), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }
        for(uint32_t i=1; i<im.image_info.mipLevels; ++i)
        {
            transition_image(cmd, im.image_object, i, exactly(layer), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
}

void vk_command_buffer::begin_render_pass(const render_pass_desc & pass_desc, framebuffer & framebuffer)
{
    record_reference(framebuffer);
    auto & fb = static_cast<vk_framebuffer &>(framebuffer);
    std::vector<VkClearValue> clear_values;
    for(size_t i=0; i<pass_desc.color_attachments.size(); ++i)
    {
        if(auto op = std::get_if<clear_color>(&pass_desc.color_attachments[i].load_op))
        {
            clear_values.resize(i+1);
            clear_values[i].color = {op->r, op->g, op->b, op->a};
        }
    }
    if(pass_desc.depth_attachment)
    {
        if(auto op = std::get_if<clear_depth>(&pass_desc.depth_attachment->load_op))
        {
            clear_values.resize(pass_desc.color_attachments.size()+1);
            clear_values.back().depthStencil = {op->depth, op->stencil};
        }
    }

    VkRenderPassBeginInfo pass_begin_info {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_begin_info.renderPass = device->get_render_pass(fb.get_render_pass_desc(pass_desc));
    pass_begin_info.framebuffer = fb.framebuffers[fb.current_index];
    pass_begin_info.renderArea = {{0,0},{exactly(fb.dims.x),exactly(fb.dims.y)}};
    pass_begin_info.clearValueCount = exactly(countof(clear_values));
    pass_begin_info.pClearValues = clear_values.data();
    
    vkCmdBeginRenderPass(cmd, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewports[] {{0, 0, exactly(fb.dims.x), exactly(fb.dims.y), 0, 1}};
    vkCmdSetViewport(cmd, 0, exactly(countof(viewports)), viewports);
    vkCmdSetScissor(cmd, 0, 1, &pass_begin_info.renderArea);    
    current_pass = pass_begin_info.renderPass;
}

void vk_command_buffer::set_scissor_rect(int x0, int y0, int x1, int y1)
{
    VkRect2D scissor {{x0, y0}, {x1-x0, y1-y0}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void vk_command_buffer::bind_pipeline(pipeline & pipe)
{
    record_reference(pipe);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<vk_pipeline &>(pipe).get_pipeline(current_pass));
}

void vk_command_buffer::bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set & set)
{
    record_reference(layout);
    record_reference(set);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<vk_pipeline_layout &>(layout).layout, set_index, 1, &static_cast<vk_descriptor_set &>(set).set, 0, nullptr);
}

void vk_command_buffer::bind_vertex_buffer(int index, buffer_range range)
{
    record_reference(range.buffer);
    VkDeviceSize offset = range.offset;
    vkCmdBindVertexBuffers(cmd, index, 1, &static_cast<vk_buffer &>(range.buffer).buffer_object, &offset);
}

void vk_command_buffer::bind_index_buffer(buffer_range range)
{
    record_reference(range.buffer);
    vkCmdBindIndexBuffer(cmd, static_cast<vk_buffer &>(range.buffer).buffer_object, range.offset, VK_INDEX_TYPE_UINT32);
}

void vk_command_buffer::draw(int first_vertex, int vertex_count)
{
    vkCmdDraw(cmd, vertex_count, 1, first_vertex, 0);
}

void vk_command_buffer::draw_indexed(int first_index, int index_count)
{
    vkCmdDrawIndexed(cmd, index_count, 1, first_index, 0, 0);
}

void vk_command_buffer::end_render_pass()
{
    vkCmdEndRenderPass(cmd);
    current_pass = 0;
}

uint64_t vk_device::submit(const VkSubmitInfo & submit_info)
{
    if(completed_index + fence_ring_size == submitted_index) wait_until_complete(submitted_index - fence_ring_mask);
    check("vkQueueSubmit", vkQueueSubmit(queue, 1, &submit_info, ring_fences[submitted_index & fence_ring_mask]));
    ++submitted_index;
    for(uint32_t i=0; i<submit_info.commandBufferCount; ++i) scheduled_actions.push({submitted_index, [cmd=submit_info.pCommandBuffers[i]](vk_device & dev) { vkFreeCommandBuffers(dev.dev, dev.staging_pool, 1, &cmd); }});
    return submitted_index;
}

void vk_device::wait_until_complete(uint64_t submission_index)
{
    while(completed_index < submission_index)
    {
        check("vkWaitForFences", vkWaitForFences(dev, 1, &ring_fences[completed_index & fence_ring_mask], VK_TRUE, std::numeric_limits<uint64_t>::max()));
        check("vkResetFences", vkResetFences(dev, 1, &ring_fences[completed_index & fence_ring_mask]));
        ++completed_index;
    }

    while(!scheduled_actions.empty() && completed_index >= scheduled_actions.front().after_completion_index)
    {
        scheduled_actions.front().execute(*this);
        scheduled_actions.pop();
    }
}

uint64_t vk_device::submit(command_buffer & cmd)
{
    check("vkEndCommandBuffer", vkEndCommandBuffer(static_cast<vk_command_buffer &>(cmd).cmd));
    VkSubmitInfo submit_info {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &static_cast<vk_command_buffer &>(cmd).cmd;
    submit(submit_info);
    return submitted_index;
}

uint64_t vk_device::acquire_and_submit_and_present(command_buffer & cmd, window & window)
{
    check("vkEndCommandBuffer", vkEndCommandBuffer(static_cast<vk_command_buffer &>(cmd).cmd)); 

    auto & win = static_cast<vk_window &>(window);

    VkPipelineStageFlags wait_stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &win.image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &static_cast<vk_command_buffer &>(cmd).cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &win.render_finished;
    submit(submit_info);

    VkPresentInfoKHR present_info {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &win.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &win.swapchain_framebuffer->swapchain;
    present_info.pImageIndices = &win.swapchain_framebuffer->current_index;
    check("vkQueuePresentKHR", vkQueuePresentKHR(queue, &present_info));

    check("vkAcquireNextImageKHR", vkAcquireNextImageKHR(dev, win.swapchain_framebuffer->swapchain, std::numeric_limits<uint64_t>::max(), win.image_available, VK_NULL_HANDLE, &win.swapchain_framebuffer->current_index));
    return submitted_index;
}
