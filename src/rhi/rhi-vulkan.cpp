#include "rhi-internal.h"
#include <sstream>

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "vulkan-1.lib")

namespace rhi
{
    VkFormat get_vk_format(image_format format)
    {
        switch(format)
        {
        #define X(FORMAT, SIZE, TYPE, VK, GL, DX) case FORMAT: return VK;
        #include "rhi-format.inl"
        #undef X
        default: fail_fast();
        }
    }

    struct physical_device_selection
    {
        VkPhysicalDevice physical_device;
        uint32_t queue_family;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        uint32_t swap_image_count;
        VkSurfaceTransformFlagBitsKHR surface_transform;
    };

    struct vk_buffer
    {
        VkDeviceMemory memory_object;
        VkBuffer buffer_object;        
        char * mapped = 0;
    };

    struct vk_image
    {
        VkDeviceMemory device_memory;
        VkImage image_object;
        VkImageView image_view;        
    };

    struct vk_shader
    {
        VkShaderModule module;
        shader_stage stage;
    };

    struct vk_pipeline
    {
        pipeline_desc desc;
        VkPipeline pipeline_object;
    };

    struct vk_framebuffer
    {
        int2 dims;
        std::vector<VkFramebuffer> framebuffers; // If this framebuffer targets a swapchain, the framebuffers for each swapchain image
        uint32_t current_index; // If this framebuffer targets a swapchain, the index of the current backbuffer
    };

    struct vk_window
    {
        GLFWwindow * glfw_window {};
        VkSurfaceKHR surface {};
        VkSwapchainKHR swapchain {};
        std::vector<VkImage> swapchain_images;
        std::vector<VkImageView> swapchain_image_views;
        VkSemaphore image_available {}, render_finished {};
        uint2 dims;
        image depth_image;
        framebuffer swapchain_framebuffer;
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
        VkPhysicalDeviceMemoryProperties mem_props {};

        // Resources for staging memory
        VkBuffer staging_buffer {};
        VkDeviceMemory staging_memory {};
        void * mapped_staging_memory {};
        VkCommandPool staging_pool {};

        // Objects
        template<class T> struct traits;
        template<> struct traits<buffer> { using type = vk_buffer; };
        template<> struct traits<image> { using type = vk_image; };
        template<> struct traits<sampler> { using type = VkSampler; };
        template<> struct traits<render_pass> { using type = VkRenderPass; };
        template<> struct traits<framebuffer> { using type = vk_framebuffer; };
        template<> struct traits<descriptor_pool> { using type = VkDescriptorPool; };
        template<> struct traits<descriptor_set_layout> { using type = VkDescriptorSetLayout; };
        template<> struct traits<descriptor_set> { using type = VkDescriptorSet; };
        template<> struct traits<pipeline_layout> { using type = VkPipelineLayout; };
        template<> struct traits<shader> { using type = vk_shader; }; 
        template<> struct traits<pipeline> { using type = vk_pipeline; };
        template<> struct traits<window> { using type = vk_window; };
        template<> struct traits<command_buffer> { using type = VkCommandBuffer; };
        heterogeneous_object_set<traits, buffer, image, sampler, render_pass, framebuffer, descriptor_pool, descriptor_set_layout, descriptor_set, 
            pipeline_layout, shader, pipeline, window, command_buffer> objects;

        vk_device(std::function<void(const char *)> debug_callback);
        ~vk_device();

        // Core helper functions
        VkDeviceMemory allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props);
        VkCommandBuffer begin_transient();
        void end_transient(VkCommandBuffer command_buffer);

        // info
        device_info get_info() const override { return {"Vulkan 1.0", {coord_axis::right, coord_axis::down, coord_axis::forward}, linalg::zero_to_one}; }

        // buffers
        std::tuple<buffer, char *> create_buffer(const buffer_desc & desc, const void * initial_data) override;
        void destroy_buffer(buffer buffer) override;

        image create_image(const image_desc & desc, std::vector<const void *> initial_data) override;
        void destroy_image(image image) override;

        sampler create_sampler(const sampler_desc & desc) override;
        void destroy_sampler(sampler sampler) override;

        void destroy_framebuffer(framebuffer framebuffer);

        render_pass create_render_pass(const render_pass_desc & desc) override;
        void destroy_render_pass(render_pass pass) override;

        // descriptors
        descriptor_pool create_descriptor_pool() override;
        void destroy_descriptor_pool(descriptor_pool pool) override;

        descriptor_set_layout create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) override;
        void destroy_descriptor_set_layout(descriptor_set_layout layout) override;

        void reset_descriptor_pool(descriptor_pool pool) override;
        descriptor_set alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout layout) override;
        void write_descriptor(descriptor_set set, int binding, buffer_range range) override;
        void write_descriptor(descriptor_set set, int binding, sampler sampler, image image) override;

        // pipelines
        pipeline_layout create_pipeline_layout(const std::vector<descriptor_set_layout> & sets) override;
        void destroy_pipeline_layout(pipeline_layout layout) override;
        
        shader create_shader(const shader_module & module) override;
        void destroy_shader(shader shader) override;

        pipeline create_pipeline(const pipeline_desc & desc) override;
        void destroy_pipeline(pipeline pipeline) override;

        // windows
        window create_window(render_pass pass, const int2 & dimensions, std::string_view title) override;
        GLFWwindow * get_glfw_window(window window) override { return objects[window].glfw_window; }
        framebuffer get_swapchain_framebuffer(window window) override { return objects[window].swapchain_framebuffer; }
        void destroy_window(window window) override;

        // rendering
        command_buffer start_command_buffer() override;
        void begin_render_pass(command_buffer cmd, render_pass pass, framebuffer framebuffer) override;
        void bind_pipeline(command_buffer cmd, pipeline pipe) override;
        void bind_descriptor_set(command_buffer cmd, pipeline_layout layout, int set_index, descriptor_set set) override;
        void bind_vertex_buffer(command_buffer cmd, int index, buffer_range range) override;
        void bind_index_buffer(command_buffer cmd, buffer_range range) override;
        void draw(command_buffer cmd, int first_vertex, int vertex_count) override;
        void draw_indexed(command_buffer cmd, int first_index, int index_count) override;
        void end_render_pass(command_buffer cmd) override;

        void present(command_buffer submit, window window) override;
        void wait_idle() override;
    };
}

std::shared_ptr<rhi::device> create_vulkan_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<rhi::vk_device>(debug_callback);
}

namespace rhi
{
    struct vulkan_error : public std::error_category
    {
        const char * name() const noexcept override { return "VkResult"; }
        std::string message(int value) const override { return std::to_string(value); }
        static const std::error_category & instance() { static vulkan_error inst; return inst; }
    };
    static void check(const char * func, VkResult result)
    {
        if(result != VK_SUCCESS)
        {
            std::ostringstream ss; ss << func << "(...) failed";
            throw std::system_error(std::error_code(exactly(static_cast<std::underlying_type_t<VkResult>>(result)), vulkan_error::instance()), ss.str());
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
    vkGetPhysicalDeviceMemoryProperties(selection.physical_device, &mem_props);

    // Set up staging buffer
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = 16*1024*1024;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check("vkCreateBuffer", vkCreateBuffer(dev, &buffer_info, nullptr, &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &mem_reqs);
    staging_memory = allocate(mem_reqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    check("vkBindBufferMemory", vkBindBufferMemory(dev, staging_buffer, staging_memory, 0));
    check("vkMapMemory", vkMapMemory(dev, staging_memory, 0, buffer_info.size, 0, &mapped_staging_memory));
        
    VkCommandPoolCreateInfo command_pool_info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = selection.queue_family; // TODO: Could use an explicit transfer queue
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check("vkCreateCommandPool", vkCreateCommandPool(dev, &command_pool_info, nullptr, &staging_pool));
}

vk_device::~vk_device()
{
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

VkCommandBuffer vk_device::begin_transient() 
{
    VkCommandBufferAllocateInfo alloc_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = staging_pool;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer command_buffer;
    check("vkAllocateCommandBuffers", vkAllocateCommandBuffers(dev, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check("vkBeginCommandBuffer", vkBeginCommandBuffer(command_buffer, &begin_info));

    return command_buffer;
}

void vk_device::end_transient(VkCommandBuffer command_buffer) 
{
    check("vkEndCommandBuffer", vkEndCommandBuffer(command_buffer));
    VkSubmitInfo submitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;
    check("vkQueueSubmit", vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    check("vkQueueWaitIdle", vkQueueWaitIdle(queue)); // TODO: Do something with fences instead
    vkFreeCommandBuffers(dev, staging_pool, 1, &command_buffer);
}

////////////////////////
// vk_device buffers //
////////////////////////

std::tuple<buffer, char *> vk_device::create_buffer(const buffer_desc & desc, const void * initial_data)
{
    auto [handle, b] = objects.create<buffer>();

    // Create buffer with appropriate usage flags
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = desc.size;
    if(desc.usage == buffer_usage::vertex) buffer_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(desc.usage == buffer_usage::index) buffer_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(desc.usage == buffer_usage::uniform) buffer_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if(desc.usage == buffer_usage::storage) buffer_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(initial_data) buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check("vkCreateBuffer", vkCreateBuffer(dev, &buffer_info, nullptr, &b.buffer_object));

    // Obtain and bind memory (TODO: Pool allocations)
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, b.buffer_object, &mem_reqs);
    VkMemoryPropertyFlags memory_property_flags = 0;
    if(desc.dynamic) memory_property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    b.memory_object = allocate(mem_reqs, memory_property_flags);
    check("vkBindBufferMemory", vkBindBufferMemory(dev, b.buffer_object, b.memory_object, 0));

    // Initialize memory if requested to do so
    if(initial_data)
    {
        memcpy(mapped_staging_memory, initial_data, desc.size);
        auto cmd = begin_transient();
        const VkBufferCopy copy {0, 0, desc.size};
        vkCmdCopyBuffer(cmd, staging_buffer, b.buffer_object, 1, &copy);
        end_transient(cmd);
    }

    // Map memory if requested to do so
    if(desc.dynamic)
    {
        check("vkMapMemory", vkMapMemory(dev, b.memory_object, 0, desc.size, 0, reinterpret_cast<void**>(&b.mapped)));
    }

    return {handle, b.mapped};
}

void vk_device::destroy_buffer(buffer buffer)
{
    vkDestroyBuffer(dev, objects[buffer].buffer_object, nullptr);
    if(objects[buffer].mapped) vkUnmapMemory(dev, objects[buffer].memory_object);
    vkFreeMemory(dev, objects[buffer].memory_object, nullptr);
    objects.destroy(buffer); 
}

static size_t compute_image_size(int pixels, VkFormat format)
{
    if(format <= VK_FORMAT_UNDEFINED) throw std::logic_error("unknown format");
    if(format <= VK_FORMAT_R4G4_UNORM_PACK8) return pixels*1;
    if(format <= VK_FORMAT_A1R5G5B5_UNORM_PACK16) return pixels*2;
    if(format <= VK_FORMAT_R8_SRGB) return pixels*1;
    if(format <= VK_FORMAT_R8G8_SRGB) return pixels*2;
    if(format <= VK_FORMAT_B8G8R8_SRGB) return pixels*3;
    if(format <= VK_FORMAT_A2B10G10R10_SINT_PACK32) return pixels*4;
    if(format <= VK_FORMAT_R16_SFLOAT) return pixels*2;
    if(format <= VK_FORMAT_R16G16_SFLOAT) return pixels*4;
    if(format <= VK_FORMAT_R16G16B16_SFLOAT) return pixels*6;
    if(format <= VK_FORMAT_R16G16B16A16_SFLOAT) return pixels*8;
    if(format <= VK_FORMAT_R32_SFLOAT) return pixels*4;
    if(format <= VK_FORMAT_R32G32_SFLOAT) return pixels*8;
    if(format <= VK_FORMAT_R32G32B32_SFLOAT) return pixels*12;
    if(format <= VK_FORMAT_R32G32B32A32_SFLOAT) return pixels*16;
    if(format <= VK_FORMAT_E5B9G9R9_UFLOAT_PACK32) return pixels*4;
    throw std::logic_error("unknown format");
}

static size_t compute_image_size(int2 dims, VkFormat format) { return compute_image_size(product(dims), format); }
static size_t compute_image_size(int3 dims, VkFormat format) { return compute_image_size(product(dims), format); }

static void transition_layout(VkCommandBuffer command_buffer, VkImage image, uint32_t mip_level, uint32_t array_layer, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkImageMemoryBarrier barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = mip_level;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = array_layer;
    barrier.subresourceRange.layerCount = 1;
    switch(old_layout)
    {
    case VK_IMAGE_LAYOUT_UNDEFINED: break; // No need to wait for anything, contents can be discarded
    case VK_IMAGE_LAYOUT_PREINITIALIZED: barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; break; // Wait for host writes to complete before changing layout    
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; break; // Wait for transfer reads to complete before changing layout
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; break; // Wait for transfer writes to complete before changing layout
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; break; // Wait for color attachment writes to complete before changing layout
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; break; // Wait for shader reads to complete before changing layout
    default: throw std::logic_error("unsupported layout transition");
    }
    switch(new_layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; break; // Transfer reads should wait for layout change to complete
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; break; // Transfer writes should wait for layout change to complete
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; break; // Writes to color attachments should wait for layout change to complete
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; break; // Shader reads should wait for layout change to complete
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; break; // Memory reads should wait for layout change to complete
    default: throw std::logic_error("unsupported layout transition");
    }
    vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

image vk_device::create_image(const image_desc & desc, std::vector<const void *> initial_data)
{
    auto [handle, im] = objects.create<image>();
    VkImageCreateInfo image_info {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
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
    default: throw std::logic_error("bad gfx::image_shape");
    }

    image_info.format = get_vk_format(desc.format);
    image_info.extent = {static_cast<uint32_t>(desc.dimensions.x), static_cast<uint32_t>(desc.dimensions.y), static_cast<uint32_t>(desc.dimensions.z)};
    image_info.mipLevels = desc.mip_levels;        
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    if(initial_data.size()) image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if(image_info.mipLevels > 1) image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if(desc.flags & image_flag::sampled_image_bit) image_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if(desc.flags & image_flag::color_attachment_bit) image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if(desc.flags & image_flag::depth_attachment_bit) image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    check("vkCreateImage", vkCreateImage(dev, &image_info, nullptr, &im.image_object));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(dev, im.image_object, &mem_reqs);
    im.device_memory = allocate(mem_reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check("vkBindImageMemory", vkBindImageMemory(dev, im.image_object, im.device_memory, 0));
    
    if(initial_data.size())
    {
        if(initial_data.size() != image_info.arrayLayers) throw std::logic_error("not enough initial_data pointers");
        for(size_t layer=0; layer<initial_data.size(); ++layer) 
        {
            // Write initial data for this layer into the staging area
            memcpy(mapped_staging_memory, initial_data[layer], compute_image_size(desc.dimensions, image_info.format));

            VkImageSubresourceLayers layers {};
            layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            layers.baseArrayLayer = exactly(layer);
            layers.layerCount = 1;

            // Copy image contents from staging buffer into mip level zero
            auto cmd = begin_transient();
            transition_layout(cmd, im.image_object, 0, exactly(layer), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            VkBufferImageCopy copy_region {};
            copy_region.imageSubresource = layers;
            copy_region.imageExtent = image_info.extent;
            vkCmdCopyBufferToImage(cmd, staging_buffer, im.image_object, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            // Generate mip levels using blits
            VkOffset3D dims {desc.dimensions.x, desc.dimensions.y, desc.dimensions.z};
            for(uint32_t i=1; i<image_info.mipLevels; ++i)
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

                transition_layout(cmd, im.image_object, i-1, exactly(layer), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                transition_layout(cmd, im.image_object, i, exactly(layer), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                vkCmdBlitImage(cmd, im.image_object, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, im.image_object, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
                transition_layout(cmd, im.image_object, i-1, exactly(layer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            transition_layout(cmd, im.image_object, image_info.mipLevels-1, exactly(layer), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            end_transient(cmd);
        }
    }

    VkImageViewCreateInfo image_view_info {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    image_view_info.image = im.image_object;
    image_view_info.viewType = view_type;
    image_view_info.format = image_info.format;
    if(desc.flags & image_flag::depth_attachment_bit) image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = image_info.mipLevels;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = image_info.arrayLayers;
    check("vkCreateImageView", vkCreateImageView(dev, &image_view_info, nullptr, &im.image_view));
    return handle;
}

void vk_device::destroy_image(image image)
{
    vkDestroyImageView(dev, objects[image].image_view, nullptr);
    vkDestroyImage(dev, objects[image].image_object, nullptr);
    vkFreeMemory(dev, objects[image].device_memory, nullptr);
    objects.destroy(image); 
}

sampler vk_device::create_sampler(const sampler_desc & desc)
{
    auto convert_mode = [](rhi::address_mode mode)
    {
        switch(mode)
        {
        case rhi::address_mode::repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case rhi::address_mode::mirrored_repeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case rhi::address_mode::clamp_to_edge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case rhi::address_mode::mirror_clamp_to_edge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        case rhi::address_mode::clamp_to_border: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: fail_fast();
        }
    };
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
    sampler_info.addressModeU = convert_mode(desc.wrap_s);
    sampler_info.addressModeV = convert_mode(desc.wrap_t);
    sampler_info.addressModeW = convert_mode(desc.wrap_r);
    sampler_info.magFilter = convert_filter(desc.mag_filter);
    sampler_info.minFilter = convert_filter(desc.min_filter);
    if(desc.mip_filter)
    {
        sampler_info.maxLod = 1000;
        sampler_info.mipmapMode = convert_mipmap_mode(*desc.mip_filter);
    }
    auto [handle, samp] = objects.create<sampler>();
    check("vkCreateSampler", vkCreateSampler(dev, &sampler_info, nullptr, &samp));
    return handle;
}
void vk_device::destroy_sampler(sampler sampler)
{
    vkDestroySampler(dev, objects[sampler], nullptr);
    objects.destroy(sampler);
}

void vk_device::destroy_framebuffer(framebuffer framebuffer)
{
    for(auto fb : objects[framebuffer].framebuffers) vkDestroyFramebuffer(dev, fb, nullptr);
    objects.destroy(framebuffer);
}

static VkAttachmentDescription make_attachment_description(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load_op, VkImageLayout initial_layout=VK_IMAGE_LAYOUT_UNDEFINED, VkAttachmentStoreOp store_op=VK_ATTACHMENT_STORE_OP_DONT_CARE, VkImageLayout final_layout=VK_IMAGE_LAYOUT_UNDEFINED)
{
    return {0, format, samples, load_op, store_op, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, initial_layout, final_layout};
}

render_pass vk_device::create_render_pass(const render_pass_desc & desc) 
{
    const VkAttachmentDescription attachments[]
    {
        make_attachment_description(selection.surface_format.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
        make_attachment_description(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED)
    };
    const VkAttachmentReference color_refs[] {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depth_ref {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass_desc {};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = exactly(countof(color_refs));
    subpass_desc.pColorAttachments = color_refs;
    subpass_desc.pDepthStencilAttachment = &depth_ref;
    
    VkRenderPassCreateInfo render_pass_info {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = exactly(countof(attachments));
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_desc;

    auto [handle, pass] = objects.create<render_pass>();
    check("vkCreateRenderPass", vkCreateRenderPass(dev, &render_pass_info, nullptr, &pass));
    return handle;
}
void vk_device::destroy_render_pass(render_pass pass)
{ 
    vkDestroyRenderPass(dev, objects[pass], nullptr);
    objects.destroy(pass); 
}

////////////////////////////
// vk_device descriptors //
////////////////////////////

descriptor_pool vk_device::create_descriptor_pool() 
{ 
    auto [handle, pool] = objects.create<descriptor_pool>();
    const VkDescriptorPoolSize pool_sizes[] {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1024}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024}};
    VkDescriptorPoolCreateInfo descriptor_pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptor_pool_info.poolSizeCount = 2;
    descriptor_pool_info.pPoolSizes = pool_sizes;
    descriptor_pool_info.maxSets = 1024;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    check("vkCreateDescriptorPool", vkCreateDescriptorPool(dev, &descriptor_pool_info, nullptr, &pool));
    return handle;
}
void vk_device::destroy_descriptor_pool(descriptor_pool pool)
{
    vkDestroyDescriptorPool(dev, objects[pool], nullptr);
    objects.destroy(pool); 
}

descriptor_set_layout vk_device::create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) 
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

    auto [handle, layout] = objects.create<descriptor_set_layout>();
    check("vkCreateDescriptorSetLayout", vkCreateDescriptorSetLayout(dev, &create_info, nullptr, &layout));
    return handle;
}
void vk_device::destroy_descriptor_set_layout(descriptor_set_layout layout) 
{ 
    vkDestroyDescriptorSetLayout(dev, objects[layout], nullptr);
    objects.destroy(layout); 
}

void vk_device::reset_descriptor_pool(descriptor_pool pool) 
{
    vkResetDescriptorPool(dev, objects[pool], 0);
}
descriptor_set vk_device::alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout layout) 
{ 
    auto & p = objects[pool];

    VkDescriptorSetAllocateInfo alloc_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = p;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &objects[layout];

    auto [handle, set] = objects.create<descriptor_set>();
    check("vkAllocateDescriptorSets", vkAllocateDescriptorSets(dev, &alloc_info, &set));
    //p.descriptor_sets.push_back(descriptor_set);
    return handle;
}
void vk_device::write_descriptor(descriptor_set set, int binding, buffer_range range) 
{
    VkDescriptorBufferInfo buffer_info { objects[range.buffer].buffer_object, range.offset, range.size };
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, objects[set], exactly(binding), 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &buffer_info, nullptr};
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}
void vk_device::write_descriptor(descriptor_set set, int binding, sampler sampler, image image) 
{
    VkDescriptorImageInfo image_info {objects[sampler], objects[image].image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, objects[set], exactly(binding), 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &image_info, nullptr, nullptr};
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}

//////////////////////////
// vk_device pipelines //
//////////////////////////

pipeline_layout vk_device::create_pipeline_layout(const std::vector<descriptor_set_layout> & sets) 
{ 
    std::vector<VkDescriptorSetLayout> set_layouts;
    for(auto s : sets) set_layouts.push_back(objects[s]);

    VkPipelineLayoutCreateInfo create_info {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    create_info.setLayoutCount = exactly(set_layouts.size());
    create_info.pSetLayouts = set_layouts.data();

    auto [handle, layout] = objects.create<pipeline_layout>();
    check("vkCreatePipelineLayout", vkCreatePipelineLayout(dev, &create_info, nullptr, &layout));
    return handle;
}
void vk_device::destroy_pipeline_layout(pipeline_layout layout)
{
    vkDestroyPipelineLayout(dev, objects[layout], nullptr);
    objects.destroy(layout); 
}

shader vk_device::create_shader(const shader_module & module)
{
    VkShaderModuleCreateInfo create_info {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = module.spirv.size() * sizeof(uint32_t);
    create_info.pCode = module.spirv.data();
    auto [handle, s] = objects.create<shader>();
    check("vkCreateShaderModule", vkCreateShaderModule(dev, &create_info, nullptr, &s.module));
    s.stage = module.stage;
    return handle;
}
void vk_device::destroy_shader(shader shader)
{
    vkDestroyShaderModule(dev, objects[shader].module, nullptr);
    objects.destroy(shader); 
}

pipeline vk_device::create_pipeline(const pipeline_desc & desc)
{
    VkPipelineInputAssemblyStateCreateInfo inputAssembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    switch(desc.topology)
    {
    case primitive_topology::points: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
    case primitive_topology::lines: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
    case primitive_topology::triangles: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    }
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    const VkViewport viewport {};
    const VkRect2D scissor {};
    VkPipelineViewportStateCreateInfo viewportState {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; /// Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    VkPipelineColorBlendStateCreateInfo colorBlending {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    if(true) //render_pass.has_color_attachments())
    {
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
    }
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    const VkDynamicState dynamic_states[] {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil_state.depthWriteEnable = VK_TRUE;
    if(desc.depth_test)
    {
        depth_stencil_state.depthTestEnable = VK_TRUE;
        depth_stencil_state.depthCompareOp = static_cast<VkCompareOp>(*desc.depth_test);
    }

    // TODO: Fix this
    VkPipelineShaderStageCreateInfo stages[2] 
    {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, objects[desc.stages[0]].module, "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, objects[desc.stages[1]].module, "main"},
    };
            
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    for(auto & b : desc.input)
    {
        bindings.push_back({(uint32_t)b.index, (uint32_t)b.stride, VK_VERTEX_INPUT_RATE_VERTEX});
        for(auto & a : b.attributes)
        {
            switch(a.type)
            {
            case attribute_format::float1: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32_SFLOAT, (uint32_t)a.offset}); break;
            case attribute_format::float2: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32_SFLOAT, (uint32_t)a.offset}); break;
            case attribute_format::float3: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)a.offset}); break;
            case attribute_format::float4: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)a.offset}); break;
            }
        }
    }
    VkPipelineVertexInputStateCreateInfo vertex_input_state {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, exactly(bindings.size()), bindings.data(), exactly(attributes.size()), attributes.data()};

    VkGraphicsPipelineCreateInfo pipelineInfo {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2; //stages.size;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertex_input_state;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depth_stencil_state;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = objects[desc.layout];
    pipelineInfo.renderPass = objects[desc.pass];
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    auto [handle, pipe] = objects.create<pipeline>();
    pipe.desc = desc;
    check("vkCreateGraphicsPipelines", vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe.pipeline_object));
    return handle;
}
void vk_device::destroy_pipeline(pipeline pipeline)
{
    vkDestroyPipeline(dev, objects[pipeline].pipeline_object, nullptr);
    objects.destroy(pipeline); 
}

////////////////////////
// vk_device windows //
////////////////////////

window vk_device::create_window(render_pass pass, const int2 & dimensions, std::string_view title) 
{
    const std::string buffer {begin(title), end(title)};
    auto [handle, win] = objects.create<window>();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    win.glfw_window = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);

    check("glfwCreateWindowSurface", glfwCreateWindowSurface(instance, win.glfw_window, nullptr, &win.surface));

    VkBool32 present = VK_FALSE;
    check("vkGetPhysicalDeviceSurfaceSupportKHR", vkGetPhysicalDeviceSurfaceSupportKHR(selection.physical_device, selection.queue_family, win.surface, &present));
    if(!present) throw std::runtime_error("vkGetPhysicalDeviceSurfaceSupportKHR(...) inconsistent");

    // Determine swap extent
    VkExtent2D swap_extent {static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y)};
    VkSurfaceCapabilitiesKHR surface_caps;
    check("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection.physical_device, win.surface, &surface_caps));
    swap_extent.width = std::min(std::max(swap_extent.width, surface_caps.minImageExtent.width), surface_caps.maxImageExtent.width);
    swap_extent.height = std::min(std::max(swap_extent.height, surface_caps.minImageExtent.height), surface_caps.maxImageExtent.height);

    VkSwapchainCreateInfoKHR swapchain_info {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchain_info.surface = win.surface;
    swapchain_info.minImageCount = selection.swap_image_count;
    swapchain_info.imageFormat = selection.surface_format.format;
    swapchain_info.imageColorSpace = selection.surface_format.colorSpace;
    swapchain_info.imageExtent = swap_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = selection.surface_transform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = selection.present_mode;
    swapchain_info.clipped = VK_TRUE;

    check("vkCreateSwapchainKHR", vkCreateSwapchainKHR(dev, &swapchain_info, nullptr, &win.swapchain));    

    uint32_t swapchain_image_count;    
    check("vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR(dev, win.swapchain, &swapchain_image_count, nullptr));
    win.swapchain_images.resize(swapchain_image_count);
    check("vkGetSwapchainImagesKHR", vkGetSwapchainImagesKHR(dev, win.swapchain, &swapchain_image_count, win.swapchain_images.data()));

    win.swapchain_image_views.resize(swapchain_image_count);
    for(uint32_t i=0; i<swapchain_image_count; ++i)
    {
        VkImageViewCreateInfo view_info {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_info.image = win.swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = selection.surface_format.format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        check("vkCreateImageView", vkCreateImageView(dev, &view_info, nullptr, &win.swapchain_image_views[i]));
    }

    VkSemaphoreCreateInfo semaphore_info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check("vkCreateSemaphore", vkCreateSemaphore(dev, &semaphore_info, nullptr, &win.image_available));
    check("vkCreateSemaphore", vkCreateSemaphore(dev, &semaphore_info, nullptr, &win.render_finished));
    
    // Create swapchain framebuffer
    auto [fb_handle, fb] = objects.create<framebuffer>();
    fb.dims = dimensions;
    fb.framebuffers.resize(win.swapchain_image_views.size());
    win.depth_image = create_image({rhi::image_shape::_2d, {dimensions,1}, 1, rhi::image_format::depth_float32, rhi::depth_attachment_bit}, {});
    for(size_t i=0; i<win.swapchain_image_views.size(); ++i)
    {
        std::vector<VkImageView> attachments {win.swapchain_image_views[i], objects[win.depth_image].image_view};
        VkFramebufferCreateInfo framebuffer_info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_info.renderPass = objects[pass];
        framebuffer_info.attachmentCount = exactly(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = dimensions.x;
        framebuffer_info.height = dimensions.y;
        framebuffer_info.layers = 1;
        check("vkCreateFramebuffer", vkCreateFramebuffer(dev, &framebuffer_info, nullptr, &fb.framebuffers[i]));
    }   
    win.swapchain_framebuffer = fb_handle;

    // Acquire the first image
    check("vkAcquireNextImageKHR", vkAcquireNextImageKHR(dev, win.swapchain, std::numeric_limits<uint64_t>::max(), win.image_available, VK_NULL_HANDLE, &fb.current_index));
    return handle;
}

void vk_device::destroy_window(window window)
{ 
    auto & win = objects[window];
    destroy_framebuffer(win.swapchain_framebuffer);
    destroy_image(win.depth_image);
    vkDestroySemaphore(dev, win.render_finished, nullptr);
    vkDestroySemaphore(dev, win.image_available, nullptr);
    for(auto view : win.swapchain_image_views) vkDestroyImageView(dev, view, nullptr);
    vkDestroySwapchainKHR(dev, win.swapchain, nullptr);
    vkDestroySurfaceKHR(instance, win.surface, nullptr);
    glfwDestroyWindow(win.glfw_window);
    objects.destroy(window);
}

//////////////////////////
// vk_device rendering //
//////////////////////////

command_buffer vk_device::start_command_buffer()
{
    auto [handle, cmd] = objects.create<command_buffer>();
    cmd = begin_transient();
    return handle;
}

void vk_device::begin_render_pass(command_buffer cmd, render_pass pass, framebuffer framebuffer)
{
    auto & fb = objects[framebuffer];

    VkClearValue clear_values[] {{0, 0, 0, 1}, {1.0f, 0}};
    VkRenderPassBeginInfo pass_begin_info {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_begin_info.renderPass = objects[pass];
    pass_begin_info.framebuffer = fb.framebuffers[fb.current_index];
    pass_begin_info.renderArea = {{0,0},{exactly(fb.dims.x),exactly(fb.dims.y)}};
    pass_begin_info.clearValueCount = exactly(countof(clear_values));
    pass_begin_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(objects[cmd], &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewports[] {{0, 0, exactly(fb.dims.x), exactly(fb.dims.y), 0, 1}};
    vkCmdSetViewport(objects[cmd], 0, exactly(countof(viewports)), viewports);
    vkCmdSetScissor(objects[cmd], 0, 1, &pass_begin_info.renderArea);    
}

void vk_device::bind_pipeline(command_buffer cmd, pipeline pipe)
{
    vkCmdBindPipeline(objects[cmd], VK_PIPELINE_BIND_POINT_GRAPHICS, objects[pipe].pipeline_object);
}

void vk_device::bind_descriptor_set(command_buffer cmd, pipeline_layout layout, int set_index, descriptor_set set)
{
    vkCmdBindDescriptorSets(objects[cmd], VK_PIPELINE_BIND_POINT_GRAPHICS, objects[layout], set_index, 1, &objects[set], 0, nullptr);
}

void vk_device::bind_vertex_buffer(command_buffer cmd, int index, buffer_range range)
{
    VkDeviceSize offset = range.offset;
    vkCmdBindVertexBuffers(objects[cmd], index, 1, &objects[range.buffer].buffer_object, &offset);
}

void vk_device::bind_index_buffer(command_buffer cmd, buffer_range range)
{
    vkCmdBindIndexBuffer(objects[cmd], objects[range.buffer].buffer_object, range.offset, VK_INDEX_TYPE_UINT32);
}

void vk_device::draw(command_buffer cmd, int first_vertex, int vertex_count)
{
    vkCmdDraw(objects[cmd], vertex_count, 1, first_vertex, 0);
}

void vk_device::draw_indexed(command_buffer cmd, int first_index, int index_count)
{
    vkCmdDrawIndexed(objects[cmd], index_count, 1, first_index, 0, 0);
}

void vk_device::end_render_pass(command_buffer cmd)
{
    vkCmdEndRenderPass(objects[cmd]);
}

void vk_device::present(command_buffer submit, window window)
{
    check("vkEndCommandBuffer", vkEndCommandBuffer(objects[submit])); 

    auto & win = objects[window];

    VkPipelineStageFlags wait_stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &win.image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &objects[submit];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &win.render_finished;
    check("vkQueueSubmit", vkQueueSubmit(queue, 1, &submit_info, nullptr)); //objects[fence]));

    VkPresentInfoKHR present_info {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &win.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &win.swapchain;
    present_info.pImageIndices = &objects[win.swapchain_framebuffer].current_index;
    check("vkQueuePresentKHR", vkQueuePresentKHR(queue, &present_info));
            
    check("vkQueueWaitIdle", vkQueueWaitIdle(queue)); // TODO: Do something with fences instead
    vkFreeCommandBuffers(dev, staging_pool, 1, &objects[submit]);

    auto & fb = objects[win.swapchain_framebuffer];
    check("vkAcquireNextImageKHR", vkAcquireNextImageKHR(dev, win.swapchain, std::numeric_limits<uint64_t>::max(), win.image_available, VK_NULL_HANDLE, &fb.current_index));
}

void vk_device::wait_idle() 
{
    check("vkdeviceWaitIdle", vkDeviceWaitIdle(dev));
}
