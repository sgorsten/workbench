#include "rhi-internal.h"

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "vulkan-1.lib")

namespace vk
{
    const char * to_string(VkResult result);
    struct vulkan_error : public std::error_category
    {
        const char * name() const noexcept override { return "VkResult"; }
        std::string message(int value) const override { return std::to_string(static_cast<VkResult>(value)); }
        static const std::error_category & instance() { static vulkan_error inst; return inst; }
    };
    void check(VkResult result);

    struct physical_device_selection
    {
        VkPhysicalDevice physical_device;
        uint32_t queue_family;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        uint32_t swap_image_count;
        VkSurfaceTransformFlagBitsKHR surface_transform;
    };

    struct buffer
    {
        VkDeviceMemory memory_object;
        VkBuffer buffer_object;
        char * mapped = 0;
    };

    struct input_layout
    {
        std::vector<rhi::vertex_binding_desc> bindings;
    };

    struct shader
    {
        VkShaderModule module;
        shader_stage stage;
    };

    struct pipeline
    {
        rhi::pipeline_desc desc;
        VkPipeline pipeline_object;
    };

    struct framebuffer
    {
        int2 dims;
        std::vector<VkFramebuffer> framebuffers; // If this framebuffer targets a swapchain, the framebuffers for each swapchain image
        uint32_t current_index; // If this framebuffer targets a swapchain, the index of the current backbuffer
    };

    struct window
    {
        GLFWwindow * glfw_window {};
        VkSurfaceKHR surface {};
        VkSwapchainKHR swapchain {};
        std::vector<VkImage> swapchain_images;
        std::vector<VkImageView> swapchain_image_views;
        VkSemaphore image_available {}, render_finished {};
        uint2 dims;
        //handle<gfx::image> depth_image;
        VkRenderPass render_pass;
        framebuffer swapchain_framebuffer;
    };

    template<class T> struct vulkan_traits;
    template<> struct vulkan_traits<rhi::buffer> { using type = buffer; };
    template<> struct vulkan_traits<rhi::descriptor_pool> { using type = VkDescriptorPool; };
    template<> struct vulkan_traits<rhi::descriptor_set_layout> { using type = VkDescriptorSetLayout; };      
    template<> struct vulkan_traits<rhi::descriptor_set> { using type = VkDescriptorSet; };
    template<> struct vulkan_traits<rhi::pipeline_layout> { using type = VkPipelineLayout; };
    template<> struct vulkan_traits<rhi::input_layout> { using type = input_layout; };
    template<> struct vulkan_traits<rhi::shader> { using type = shader; }; 
    template<> struct vulkan_traits<rhi::pipeline> { using type = pipeline; };
    template<> struct vulkan_traits<rhi::window> { using type = window; };

    struct device : rhi::device
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
        heterogeneous_object_set<vulkan_traits, 
            rhi::buffer, rhi::descriptor_pool, rhi::descriptor_set_layout, rhi::descriptor_set,
            rhi::pipeline_layout, rhi::input_layout, rhi::shader, rhi::pipeline, rhi::window> objects;

        device(std::function<void(const char *)> debug_callback);
        ~device();

        // Core helper functions
        VkDeviceMemory allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props);
        VkCommandBuffer begin_transient();
        void end_transient(VkCommandBuffer command_buffer);

        // info
        rhi::device_info get_info() const override { return {"Vulkan 1.0", {coord_axis::right, coord_axis::down, coord_axis::forward}, linalg::zero_to_one}; }

        // buffers
        std::tuple<rhi::buffer, char *> create_buffer(const rhi::buffer_desc & desc, const void * initial_data) override;
        void destroy_buffer(rhi::buffer buffer) override;

        // descriptors
        rhi::descriptor_pool create_descriptor_pool() override;
        void destroy_descriptor_pool(rhi::descriptor_pool pool) override;

        rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) override;
        void destroy_descriptor_set_layout(rhi::descriptor_set_layout layout) override;

        void reset_descriptor_pool(rhi::descriptor_pool pool) override;
        rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) override;
        void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) override;

        // pipelines
        rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) override;
        void destroy_pipeline_layout(rhi::pipeline_layout layout) override;

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override;
        void destroy_input_layout(rhi::input_layout layout) override;

        rhi::shader create_shader(const shader_module & module) override;
        void destroy_shader(rhi::shader shader) override;

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override;
        void destroy_pipeline(rhi::pipeline pipeline) override;

        // windows
        std::tuple<rhi::window, GLFWwindow *> create_window(const int2 & dimensions, std::string_view title) override;
        void destroy_window(rhi::window window) override;

        // rendering
        VkCommandBuffer cmd;
        void begin_render_pass(rhi::window window) override;
        void bind_pipeline(rhi::pipeline pipe) override;
        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) override;
        void bind_vertex_buffer(int index, rhi::buffer_range range) override;
        void bind_index_buffer(rhi::buffer_range range) override;
        void draw(int first_vertex, int vertex_count) override;
        void draw_indexed(int first_index, int index_count) override;
        void end_render_pass() override;
        void present(rhi::window window) override;
        void wait_idle() override;
    };
}

std::shared_ptr<rhi::device> create_vulkan_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<vk::device>(debug_callback);
}

////////////////////
// error handling //
////////////////////

const char * vk::to_string(VkResult result)
{
    switch(result)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR: return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
    default: fail_fast();
    }
}

void vk::check(VkResult result)
{
    if(result != VK_SUCCESS)
    {
        throw std::system_error(std::error_code(result, vulkan_error::instance()), "VkResult");
    }
}

///////////////////////////////
// physical device selection //
///////////////////////////////

namespace vk
{
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
        check(glfwCreateWindowSurface(instance, example_window, nullptr, &example_surface));

        uint32_t device_count = 0;
        check(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
        std::vector<VkPhysicalDevice> physical_devices(device_count);
        check(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data()));
        for(auto & d : physical_devices)
        {
            // Skip physical devices which do not support our desired extensions
            uint32_t extension_count = 0;
            check(vkEnumerateDeviceExtensionProperties(d, 0, &extension_count, nullptr));
            std::vector<VkExtensionProperties> extensions(extension_count);
            check(vkEnumerateDeviceExtensionProperties(d, 0, &extension_count, extensions.data()));
            for(auto req : required_extensions) if(!has_extension(extensions, req)) continue;

            // Skip physical devices who do not support at least one format and present mode for our example surface
            VkSurfaceCapabilitiesKHR surface_caps;
            check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(d, example_surface, &surface_caps));
            uint32_t surface_format_count = 0, present_mode_count = 0;
            check(vkGetPhysicalDeviceSurfaceFormatsKHR(d, example_surface, &surface_format_count, nullptr));
            std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
            check(vkGetPhysicalDeviceSurfaceFormatsKHR(d, example_surface, &surface_format_count, surface_formats.data()));
            check(vkGetPhysicalDeviceSurfacePresentModesKHR(d, example_surface, &present_mode_count, nullptr));
            std::vector<VkPresentModeKHR> surface_present_modes(present_mode_count);
            check(vkGetPhysicalDeviceSurfacePresentModesKHR(d, example_surface, &present_mode_count, surface_present_modes.data()));
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
                check(vkGetPhysicalDeviceSurfaceSupportKHR(d, i, example_surface, &present));
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
// vk::device //
////////////////

vk::device::device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
{ 
    uint32_t extension_count = 0;
    auto ext = glfwGetRequiredInstanceExtensions(&extension_count);
    std::vector<const char *> extensions{ext, ext+extension_count};

    const VkApplicationInfo app_info {VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "simple-scene", VK_MAKE_VERSION(1,0,0), "No Engine", VK_MAKE_VERSION(0,0,0), VK_API_VERSION_1_0};
    const char * layers[] {"VK_LAYER_LUNARG_standard_validation"};
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    const VkInstanceCreateInfo instance_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, {}, &app_info, 1, layers, exactly(extensions.size()), extensions.data()};
    check(vkCreateInstance(&instance_info, nullptr, &instance));
    auto vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
    vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

    const VkDebugReportCallbackCreateInfoEXT callback_info {VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT, nullptr, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, 
        [](VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location, int32_t message_code, const char * layer_prefix, const char * message, void * user_data) -> VkBool32
        {
            auto & ctx = *reinterpret_cast<device *>(user_data);
            if(ctx.debug_callback) ctx.debug_callback(message);
            return VK_FALSE;
        }, this};
    check(vkCreateDebugReportCallbackEXT(instance, &callback_info, nullptr, &callback));

    std::vector<const char *> device_extensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    selection = select_physical_device(instance, device_extensions);
    const float queue_priorities[] {1.0f};
    const VkDeviceQueueCreateInfo queue_infos[] {{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, {}, selection.queue_family, 1, queue_priorities}};
    const VkDeviceCreateInfo device_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, {}, 1, queue_infos, 1, layers, exactly(device_extensions.size()), device_extensions.data()};
    check(vkCreateDevice(selection.physical_device, &device_info, nullptr, &dev));
    vkGetDeviceQueue(dev, selection.queue_family, 0, &queue);
    vkGetPhysicalDeviceMemoryProperties(selection.physical_device, &mem_props);

    // Set up staging buffer
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = 16*1024*1024;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(dev, &buffer_info, nullptr, &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, staging_buffer, &mem_reqs);
    staging_memory = allocate(mem_reqs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkBindBufferMemory(dev, staging_buffer, staging_memory, 0);

    check(vkMapMemory(dev, staging_memory, 0, buffer_info.size, 0, &mapped_staging_memory));
        
    VkCommandPoolCreateInfo command_pool_info {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.queueFamilyIndex = selection.queue_family; // TODO: Could use an explicit transfer queue
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    check(vkCreateCommandPool(dev, &command_pool_info, nullptr, &staging_pool));
}

vk::device::~device()
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

VkDeviceMemory vk::device::allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props)
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
            check(vkAllocateMemory(dev, &alloc_info, nullptr, &memory));
            return memory;
        }
    }
    throw std::runtime_error("no suitable memory type");
}

VkCommandBuffer vk::device::begin_transient() 
{
    VkCommandBufferAllocateInfo alloc_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = staging_pool;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer command_buffer;
    check(vkAllocateCommandBuffers(dev, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command_buffer, &begin_info));

    return command_buffer;
}

void vk::device::end_transient(VkCommandBuffer command_buffer) 
{
    check(vkEndCommandBuffer(command_buffer));
    VkSubmitInfo submitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;
    check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    check(vkQueueWaitIdle(queue)); // TODO: Do something with fences instead
    vkFreeCommandBuffers(dev, staging_pool, 1, &command_buffer);
}

////////////////////////
// vk::device buffers //
////////////////////////

std::tuple<rhi::buffer, char *> vk::device::create_buffer(const rhi::buffer_desc & desc, const void * initial_data)
{
    auto [handle, b] = objects.create<rhi::buffer>();

    // Create buffer with appropriate usage flags
    VkBufferCreateInfo buffer_info {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = desc.size;
    if(desc.usage == rhi::buffer_usage::vertex) buffer_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if(desc.usage == rhi::buffer_usage::index) buffer_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if(desc.usage == rhi::buffer_usage::uniform) buffer_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if(desc.usage == rhi::buffer_usage::storage) buffer_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if(initial_data) buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(dev, &buffer_info, nullptr, &b.buffer_object));

    // Obtain and bind memory (TODO: Pool allocations)
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(dev, b.buffer_object, &mem_reqs);
    VkMemoryPropertyFlags memory_property_flags = 0;
    if(desc.dynamic) memory_property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    b.memory_object = allocate(mem_reqs, memory_property_flags);
    vkBindBufferMemory(dev, b.buffer_object, b.memory_object, 0);

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
        check(vkMapMemory(dev, b.memory_object, 0, desc.size, 0, reinterpret_cast<void**>(&b.mapped)));
    }

    return {handle, b.mapped};
}

void vk::device::destroy_buffer(rhi::buffer buffer)
{
    vkDestroyBuffer(dev, objects[buffer].buffer_object, nullptr);
    if(objects[buffer].mapped) vkUnmapMemory(dev, objects[buffer].memory_object);
    vkFreeMemory(dev, objects[buffer].memory_object, nullptr);
    objects.destroy(buffer); 
}

////////////////////////////
// vk::device descriptors //
////////////////////////////

rhi::descriptor_pool vk::device::create_descriptor_pool() 
{ 
    auto [handle, pool] = objects.create<rhi::descriptor_pool>();
    const VkDescriptorPoolSize pool_sizes[] {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1024}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1024}};
    VkDescriptorPoolCreateInfo descriptor_pool_info {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptor_pool_info.poolSizeCount = 2;
    descriptor_pool_info.pPoolSizes = pool_sizes;
    descriptor_pool_info.maxSets = 1024;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    check(vkCreateDescriptorPool(dev, &descriptor_pool_info, nullptr, &pool));
    return handle;
}
void vk::device::destroy_descriptor_pool(rhi::descriptor_pool pool)
{
    vkDestroyDescriptorPool(dev, objects[pool], nullptr);
    objects.destroy(pool); 
}

rhi::descriptor_set_layout vk::device::create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) 
{ 
    std::vector<VkDescriptorSetLayoutBinding> set_bindings(bindings.size());
    for(size_t i=0; i<bindings.size(); ++i)
    {
        set_bindings[i].binding = bindings[i].index;
        switch(bindings[i].type)
        {
        case rhi::descriptor_type::combined_image_sampler: set_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
        case rhi::descriptor_type::uniform_buffer: set_bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
        }
        set_bindings[i].descriptorCount = bindings[i].count;
        set_bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    }
        
    VkDescriptorSetLayoutCreateInfo create_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    create_info.bindingCount = exactly(set_bindings.size());
    create_info.pBindings = set_bindings.data();

    auto [handle, layout] = objects.create<rhi::descriptor_set_layout>();
    check(vkCreateDescriptorSetLayout(dev, &create_info, nullptr, &layout));
    return handle;
}
void vk::device::destroy_descriptor_set_layout(rhi::descriptor_set_layout layout) 
{ 
    vkDestroyDescriptorSetLayout(dev, objects[layout], nullptr);
    objects.destroy(layout); 
}

void vk::device::reset_descriptor_pool(rhi::descriptor_pool pool) 
{
    vkResetDescriptorPool(dev, objects[pool], 0);
}
rhi::descriptor_set vk::device::alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) 
{ 
    auto & p = objects[pool];

    VkDescriptorSetAllocateInfo alloc_info {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = p;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &objects[layout];

    auto [handle, set] = objects.create<rhi::descriptor_set>();
    check(vkAllocateDescriptorSets(dev, &alloc_info, &set));
    //p.descriptor_sets.push_back(descriptor_set);
    return handle;
}
void vk::device::write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) 
{
    VkDescriptorBufferInfo buffer_info { objects[range.buffer].buffer_object, range.offset, range.size };
    VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, objects[set], exactly(binding), 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &buffer_info, nullptr};
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}

//////////////////////////
// vk::device pipelines //
//////////////////////////

rhi::pipeline_layout vk::device::create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) 
{ 
    std::vector<VkDescriptorSetLayout> set_layouts;
    for(auto s : sets) set_layouts.push_back(objects[s]);

    VkPipelineLayoutCreateInfo create_info {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    create_info.setLayoutCount = exactly(set_layouts.size());
    create_info.pSetLayouts = set_layouts.data();

    auto [handle, layout] = objects.create<rhi::pipeline_layout>();
    check(vkCreatePipelineLayout(dev, &create_info, nullptr, &layout));
    return handle;
}
void vk::device::destroy_pipeline_layout(rhi::pipeline_layout layout)
{
    vkDestroyPipelineLayout(dev, objects[layout], nullptr);
    objects.destroy(layout); 
}

rhi::input_layout vk::device::create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings)
{
    auto [handle, layout] = objects.create<rhi::input_layout>();
    layout.bindings = bindings;
    return handle;
}
void vk::device::destroy_input_layout(rhi::input_layout layout)
{
    objects.destroy(layout); 
}

rhi::shader vk::device::create_shader(const shader_module & module)
{
    VkShaderModuleCreateInfo create_info {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = module.spirv.size() * sizeof(uint32_t);
    create_info.pCode = module.spirv.data();
    auto [handle, shader] = objects.create<rhi::shader>();
    check(vkCreateShaderModule(dev, &create_info, nullptr, &shader.module));
    shader.stage = module.stage;
    return handle;
}
void vk::device::destroy_shader(rhi::shader shader)
{
    vkDestroyShaderModule(dev, objects[shader].module, nullptr);
    objects.destroy(shader); 
}

rhi::pipeline vk::device::create_pipeline(const rhi::pipeline_desc & desc)
{
    auto [handle, pipeline] = objects.create<rhi::pipeline>();
    pipeline.desc = desc;
    //return handle;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    switch(desc.topology)
    {
    case rhi::primitive_topology::points: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
    case rhi::primitive_topology::lines: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
    case rhi::primitive_topology::triangles: inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
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
    depth_stencil_state.depthWriteEnable = VK_FALSE;
    depth_stencil_state.depthTestEnable = VK_FALSE;
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;

    // TODO: Fix this
    VkPipelineShaderStageCreateInfo stages[2] 
    {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, objects[desc.stages[0]].module, "main"},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, objects[desc.stages[1]].module, "main"},
    };
            
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    for(auto & b : objects[desc.input].bindings)
    {
        bindings.push_back({(uint32_t)b.index, (uint32_t)b.stride, VK_VERTEX_INPUT_RATE_VERTEX});
        for(auto & a : b.attributes)
        {
            switch(a.type)
            {
            case rhi::attribute_format::float1: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32_SFLOAT, (uint32_t)a.offset}); break;
            case rhi::attribute_format::float2: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32_SFLOAT, (uint32_t)a.offset}); break;
            case rhi::attribute_format::float3: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)a.offset}); break;
            case rhi::attribute_format::float4: attributes.push_back({(uint32_t)a.index, (uint32_t)b.index, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)a.offset}); break;
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
    pipelineInfo.renderPass = objects[rhi::window{1}].render_pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    check(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline_object));
    return handle;
}
void vk::device::destroy_pipeline(rhi::pipeline pipeline)
{
    vkDestroyPipeline(dev, objects[pipeline].pipeline_object, nullptr);
    objects.destroy(pipeline); 
}

////////////////////////
// vk::device windows //
////////////////////////

VkAttachmentDescription make_attachment_description(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load_op, VkImageLayout initial_layout=VK_IMAGE_LAYOUT_UNDEFINED, VkAttachmentStoreOp store_op=VK_ATTACHMENT_STORE_OP_DONT_CARE, VkImageLayout final_layout=VK_IMAGE_LAYOUT_UNDEFINED)
{
    return {0, format, samples, load_op, store_op, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, initial_layout, final_layout};
}

std::tuple<rhi::window, GLFWwindow *> vk::device::create_window(const int2 & dimensions, std::string_view title) 
{
    const std::string buffer {begin(title), end(title)};
    auto [handle, win] = objects.create<rhi::window>();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    win.glfw_window = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);

    check(glfwCreateWindowSurface(instance, win.glfw_window, nullptr, &win.surface));

    VkBool32 present = VK_FALSE;
    check(vkGetPhysicalDeviceSurfaceSupportKHR(selection.physical_device, selection.queue_family, win.surface, &present));
    if(!present) throw std::runtime_error("vkGetPhysicalDeviceSurfaceSupportKHR(...) inconsistent");

    // Determine swap extent
    VkExtent2D swap_extent {static_cast<uint32_t>(dimensions.x), static_cast<uint32_t>(dimensions.y)};
    VkSurfaceCapabilitiesKHR surface_caps;
    check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selection.physical_device, win.surface, &surface_caps));
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

    check(vkCreateSwapchainKHR(dev, &swapchain_info, nullptr, &win.swapchain));    

    uint32_t swapchain_image_count;    
    check(vkGetSwapchainImagesKHR(dev, win.swapchain, &swapchain_image_count, nullptr));
    win.swapchain_images.resize(swapchain_image_count);
    check(vkGetSwapchainImagesKHR(dev, win.swapchain, &swapchain_image_count, win.swapchain_images.data()));

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
        check(vkCreateImageView(dev, &view_info, nullptr, &win.swapchain_image_views[i]));
    }

    VkSemaphoreCreateInfo semaphore_info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check(vkCreateSemaphore(dev, &semaphore_info, nullptr, &win.image_available));
    check(vkCreateSemaphore(dev, &semaphore_info, nullptr, &win.render_finished));

    ///////////// render pass //////////

    // Create render pass
    std::vector<VkAttachmentDescription> color_attachments
    {
        make_attachment_description(selection.surface_format.format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    };
    //VkAttachmentDescription depth_attachment = make_attachment_description(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED);

    std::vector<VkAttachmentDescription> attachments(begin(color_attachments), end(color_attachments));
    std::vector<VkAttachmentReference> attachment_refs;
    for(uint32_t i=0; i<color_attachments.size(); ++i) attachment_refs.push_back({i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});

    VkSubpassDescription subpass_desc {};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if(true) //depth_attachment)
    {
        //attachments.push_back(depth_attachment);
        //attachment_refs.push_back({narrow(color_attachments.size()), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
        //subpass_desc.pDepthStencilAttachment = &attachment_refs[color_attachments.size()];
    }
    subpass_desc.colorAttachmentCount = exactly(color_attachments.size());
    subpass_desc.pColorAttachments = attachment_refs.data();
    
    VkRenderPassCreateInfo render_pass_info {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = exactly(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_desc;

    //auto [handle, render_pass] = objects.create<gfx::render_pass>();
    check(vkCreateRenderPass(dev, &render_pass_info, nullptr, &win.render_pass));

    //////////

    // Create swapchain framebuffer
    auto & fb = win.swapchain_framebuffer;
    //auto [fb_handle, fb] = objects.create<gfx::framebuffer>();
    fb.dims = dimensions;
    fb.framebuffers.resize(win.swapchain_image_views.size());
    //win.depth_image = make_render_target(uint2(dimensions), VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
    for(size_t i=0; i<win.swapchain_image_views.size(); ++i)
    {
        std::vector<VkImageView> attachments {win.swapchain_image_views[i]}; //, objects[win.depth_image].image_view};
        VkFramebufferCreateInfo framebuffer_info {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_info.renderPass = win.render_pass; //objects[render_pass];
        framebuffer_info.attachmentCount = exactly(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = dimensions.x;
        framebuffer_info.height = dimensions.y;
        framebuffer_info.layers = 1;
        check(vkCreateFramebuffer(dev, &framebuffer_info, nullptr, &fb.framebuffers[i]));
    }   
    //win.swapchain_framebuffer = fb_handle;

    return {handle, win.glfw_window};
}

void vk::device::destroy_window(rhi::window window)
{ 
    auto & win = objects[window];
    for(auto fb : win.swapchain_framebuffer.framebuffers) vkDestroyFramebuffer(dev, fb, nullptr);
    vkDestroyRenderPass(dev, win.render_pass, nullptr);
    vkDestroySemaphore(dev, win.render_finished, nullptr);
    vkDestroySemaphore(dev, win.image_available, nullptr);
    for(auto view : win.swapchain_image_views) vkDestroyImageView(dev, view, nullptr);
    vkDestroySwapchainKHR(dev, win.swapchain, nullptr);
    vkDestroySurfaceKHR(instance, win.surface, nullptr);
    glfwDestroyWindow(win.glfw_window);
    objects.destroy(window);
}

//////////////////////////
// vk::device rendering //
//////////////////////////

void vk::device::begin_render_pass(rhi::window window)
{
    auto & win = objects[window];
    auto & fb = win.swapchain_framebuffer;
    check(vkAcquireNextImageKHR(dev, win.swapchain, std::numeric_limits<uint64_t>::max(), win.image_available, VK_NULL_HANDLE, &fb.current_index));

    VkClearValue clear_values[] {{0, 0, 0, 1}}; //, {1.0f, 0}};
    VkRenderPassBeginInfo pass_begin_info {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass_begin_info.renderPass = objects[window].render_pass;//objects[render_pass];
    pass_begin_info.framebuffer = fb.framebuffers[fb.current_index];
    pass_begin_info.renderArea = {{0,0},{(uint32_t)fb.dims.x,(uint32_t)fb.dims.y}};
    pass_begin_info.clearValueCount = 1; //narrow(countof(clear_values));
    pass_begin_info.pClearValues = clear_values;

    cmd = begin_transient();
    vkCmdBeginRenderPass(cmd, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewports[] {{static_cast<float>(pass_begin_info.renderArea.offset.x), static_cast<float>(pass_begin_info.renderArea.offset.y), 
        static_cast<float>(pass_begin_info.renderArea.extent.width), static_cast<float>(pass_begin_info.renderArea.extent.height), 0.0f, 1.0f}};
    vkCmdSetViewport(cmd, 0, 1, viewports);
    vkCmdSetScissor(cmd, 0, 1, &pass_begin_info.renderArea);    
}

void vk::device::bind_pipeline(rhi::pipeline pipe)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objects[pipe].pipeline_object);
}

void vk::device::bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objects[layout], set_index, 1, &objects[set], 0, nullptr);
}

void vk::device::bind_vertex_buffer(int index, rhi::buffer_range range)
{
    VkDeviceSize offset = range.offset;
    vkCmdBindVertexBuffers(cmd, index, 1, &objects[range.buffer].buffer_object, &offset);
}

void vk::device::bind_index_buffer(rhi::buffer_range range)
{
    vkCmdBindIndexBuffer(cmd, objects[range.buffer].buffer_object, range.offset, VK_INDEX_TYPE_UINT32);
}

void vk::device::draw(int first_vertex, int vertex_count)
{
    vkCmdDraw(cmd, vertex_count, 1, first_vertex, 0);
}

void vk::device::draw_indexed(int first_index, int index_count)
{
    vkCmdDrawIndexed(cmd, index_count, 1, first_index, 0, 0);
}

void vk::device::end_render_pass()
{
    vkCmdEndRenderPass(cmd);
}

void vk::device::present(rhi::window window)
{
    check(vkEndCommandBuffer(cmd)); 

    auto & win = objects[window];

    VkPipelineStageFlags wait_stages[] {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &win.image_available;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &win.render_finished;
    check(vkQueueSubmit(queue, 1, &submit_info, nullptr)); //objects[fence]));

    VkPresentInfoKHR present_info {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &win.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &win.swapchain;
    present_info.pImageIndices = &win.swapchain_framebuffer.current_index;
    check(vkQueuePresentKHR(queue, &present_info));
            
    check(vkQueueWaitIdle(queue)); // TODO: Do something with fences instead
    vkFreeCommandBuffers(dev, staging_pool, 1, &cmd);
    cmd = 0;
}

void vk::device::wait_idle() 
{
    vkDeviceWaitIdle(dev);
}
