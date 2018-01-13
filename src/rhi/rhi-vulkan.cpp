#include "rhi-internal.h"

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "vulkan-1.lib")

namespace vk
{
    const char * to_string(VkResult result)
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
        default: return ""; // fail_fast();
        }
    }

    struct vulkan_error : public std::error_category
    {
        const char * name() const noexcept override { return "VkResult"; }
        std::string message(int value) const override { return std::to_string(static_cast<VkResult>(value)); }
        static const std::error_category & instance() { static vulkan_error inst; return inst; }
    };

    void check(VkResult result)
    {
        if(result != VK_SUCCESS)
        {
            throw std::system_error(std::error_code(result, vulkan_error::instance()), "VkResult");
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

    bool has_extension(const std::vector<VkExtensionProperties> & extensions, std::string_view name)
    {
        return std::find_if(begin(extensions), end(extensions), [name](const VkExtensionProperties & p) { return p.extensionName == name; }) != end(extensions);
    }

    physical_device_selection select_physical_device(VkInstance instance, const std::vector<const char *> & required_extensions)
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

    struct input_layout
    {
        std::vector<rhi::vertex_binding_desc> bindings;
    };

    struct pipeline
    {
        rhi::pipeline_desc desc;
        VkPipeline pipeline_object;
    };

    struct buffer
    {
        VkDeviceMemory memory_object;
        VkBuffer buffer_object;
        char * mapped = 0;
    };

    struct window
    {
        GLFWwindow * w;
    };

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
        object_set<rhi::descriptor_set_layout, VkDescriptorSetLayout> descriptor_set_layouts;
        object_set<rhi::pipeline_layout, VkPipelineLayout> pipeline_layouts;
        object_set<rhi::descriptor_pool, VkDescriptorPool> descriptor_pools;
        object_set<rhi::descriptor_set, VkDescriptorSet> descriptor_sets;
        object_set<rhi::input_layout, input_layout> input_layouts;
        object_set<rhi::shader, shader_module> shaders;
        object_set<rhi::pipeline, pipeline> pipelines;
        object_set<rhi::buffer, buffer> buffers;
        object_set<rhi::window, window> windows;

        // Core helper functions
        VkDeviceMemory allocate(const VkMemoryRequirements & reqs, VkMemoryPropertyFlags props)
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

        VkCommandBuffer begin_transient() 
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

        void end_transient(VkCommandBuffer command_buffer) 
        {
            check(vkEndCommandBuffer(command_buffer));
            VkSubmitInfo submitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &command_buffer;
            check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
            check(vkQueueWaitIdle(queue)); // TODO: Do something with fences instead
            vkFreeCommandBuffers(dev, staging_pool, 1, &command_buffer);
        }

        device(std::function<void(const char *)> debug_callback) : debug_callback{debug_callback}
        { 
            uint32_t extension_count = 0;
            auto ext = glfwGetRequiredInstanceExtensions(&extension_count);
            std::vector<const char *> extensions{ext, ext+extension_count};

            const VkApplicationInfo app_info {VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "simple-scene", VK_MAKE_VERSION(1,0,0), "No Engine", VK_MAKE_VERSION(0,0,0), VK_API_VERSION_1_0};
            const char * layers[] {"VK_LAYER_LUNARG_standard_validation"};
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            const VkInstanceCreateInfo instance_info {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, {}, &app_info, 1, layers, extensions.size(), extensions.data()};
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
            const VkDeviceCreateInfo device_info {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, {}, 1, queue_infos, 1, layers, device_extensions.size(), device_extensions.data()};
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

        ~device()
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
        
        rhi::device_info get_info() const override { return {"Vulkan 1.0", {coord_axis::right, coord_axis::down, coord_axis::forward}, linalg::zero_to_one}; }

        std::tuple<rhi::window, GLFWwindow *> create_window(const int2 & dimensions, std::string_view title) override 
        {
            const std::string buffer {begin(title), end(title)};
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_OPENGL_API, GLFW_NO_API);
            auto [handle, window] = windows.create();
            window.w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, nullptr);
            if(!window.w) throw std::runtime_error("glfwCreateWindow(...) failed");
            return {handle, window.w};
        }

        rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings) override { return {}; }
        rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets) override { return {}; }
        rhi::descriptor_pool create_descriptor_pool() { return {}; }
        void reset_descriptor_pool(rhi::descriptor_pool pool) {}
        rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout) { return {}; }
        void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range) {}

        rhi::input_layout create_input_layout(const std::vector<rhi::vertex_binding_desc> & bindings) override
        {
            auto [handle, layout] = input_layouts.create();
            layout.bindings = bindings;
            return handle;
        }

        rhi::shader create_shader(const shader_module & module) override
        {
            auto [handle, shader] = shaders.create();
            shader = module;
            return handle;
        }

        rhi::pipeline create_pipeline(const rhi::pipeline_desc & desc) override
        {
            auto [handle, pipeline] = pipelines.create();
            pipeline.desc = desc;
            return handle;
        }

        std::tuple<rhi::buffer, char *> create_buffer(const rhi::buffer_desc & desc, const void * initial_data) override
        {
            auto [handle, b] = buffers.create();

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

        void begin_render_pass(rhi::window window) override
        {

        }

        void bind_pipeline(rhi::pipeline pipe) override
        {

        }

        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) override
        {

        }

        void bind_vertex_buffer(int index, rhi::buffer_range range) override
        {
    
        }

        void bind_index_buffer(rhi::buffer_range range) override
        {

        }

        void draw(int first_vertex, int vertex_count) override
        {

        }

        void draw_indexed(int first_index, int index_count) override
        {

        }

        void end_render_pass() override
        {

        }

        void present(rhi::window window) override
        {
            glfwSwapBuffers(windows[window].w);
        }
    };
}

std::shared_ptr<rhi::device> create_vulkan_device(std::function<void(const char *)> debug_callback)
{
    return std::make_shared<vk::device>(debug_callback);
}