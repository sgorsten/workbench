#include "graphics.h"
#include "rhi/rhi-internal.h"

GLFWcursor * gfx::get_standard_cursor(cursor_type type)
{
    static GLFWcursor * cursors[4]
    {
        glfwCreateStandardCursor(GLFW_ARROW_CURSOR),
        glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR),
        glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR),
        glfwCreateStandardCursor(GLFW_IBEAM_CURSOR)
    };
    return cursors[static_cast<int>(type)];
}

gfx::context::context() { if(glfwInit() == GLFW_FALSE) throw std::runtime_error("glfwInit() failed"); }
gfx::context::~context() { glfwTerminate(); }
const std::vector<rhi::client_info> & gfx::context::get_clients() { return rhi::global_backend_list(); }
rhi::ptr<rhi::device> gfx::context::create_device(array_view<rhi::client_api> api_preference, rhi::debug_callback debug_callback)
{
    auto & clients = get_clients();
    if(clients.empty()) throw std::runtime_error("No client APIs available for RHI");
    for(auto pref : api_preference) for(auto & client : clients) if(client.api == pref) return client.create_device(debug_callback);
    return clients.front().create_device(debug_callback);
}
void gfx::context::poll_events() { glfwPollEvents(); }

////////////
// window //
////////////

static gfx::window & get(GLFWwindow * window) { return *reinterpret_cast<gfx::window *>(glfwGetWindowUserPointer(window)); }

gfx::window::window(rhi::device & dev, const int2 & dimensions, const std::string & title) : rhi_window{dev.create_window(dimensions, title)}, glfw_window{rhi_window->get_glfw_window()}
{
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetWindowPosCallback(glfw_window, [](GLFWwindow * window, int xpos, int ypos) { get(window).on_window_pos({xpos, ypos}); });
    glfwSetWindowSizeCallback(glfw_window, [](GLFWwindow * window, int width, int height) { get(window).on_window_size({width, height}); });
    glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow * window) { get(window).on_window_close(); });
    glfwSetWindowRefreshCallback(glfw_window, [](GLFWwindow * window) { get(window).on_window_refresh(); });
    glfwSetWindowFocusCallback(glfw_window, [](GLFWwindow * window, int focused) { get(window).on_window_focus(!!focused); });
    glfwSetWindowIconifyCallback(glfw_window, [](GLFWwindow * window, int iconified) { get(window).on_window_iconify(!!iconified); });
    glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow * window, int width, int height) { get(window).on_framebuffer_size({width, height}); });
    glfwSetMouseButtonCallback(glfw_window, [](GLFWwindow * window, int button, int action, int mods) { get(window).on_mouse_button(button, action, mods); });
    glfwSetCursorPosCallback(glfw_window, [](GLFWwindow * window, double xpos, double ypos) { get(window).on_cursor_pos({xpos, ypos}); });
    glfwSetCursorEnterCallback(glfw_window, [](GLFWwindow * window, int entered) { get(window).on_cursor_enter(!!entered); });
    glfwSetScrollCallback(glfw_window, [](GLFWwindow * window, double xoffset, double yoffset) { get(window).on_scroll({xoffset, yoffset}); });
    glfwSetKeyCallback(glfw_window, [](GLFWwindow * window, int key, int scancode, int action, int mods) { get(window).on_key(key, scancode, action, mods); });
    glfwSetCharModsCallback(glfw_window, [](GLFWwindow * window, unsigned int codepoint, int mods) { get(window).on_char(codepoint, mods); });
}

gfx::window::~window() 
{
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetWindowPosCallback(glfw_window, nullptr);
    glfwSetWindowSizeCallback(glfw_window, nullptr);
    glfwSetWindowCloseCallback(glfw_window, nullptr);
    glfwSetWindowRefreshCallback(glfw_window, nullptr);
    glfwSetWindowFocusCallback(glfw_window, nullptr);
    glfwSetWindowIconifyCallback(glfw_window, nullptr);
    glfwSetFramebufferSizeCallback(glfw_window, nullptr);
    glfwSetMouseButtonCallback(glfw_window, nullptr);
    glfwSetCursorPosCallback(glfw_window, nullptr);
    glfwSetCursorEnterCallback(glfw_window, nullptr);
    glfwSetScrollCallback(glfw_window, nullptr);
    glfwSetKeyCallback(glfw_window, nullptr);
    glfwSetCharModsCallback(glfw_window, nullptr);
    glfwSetWindowUserPointer(glfw_window, nullptr);
}
