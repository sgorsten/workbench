#include "graphics.h"
#include "rhi/rhi-internal.h"

gfx::context::context() { if(glfwInit() == GLFW_FALSE) throw std::runtime_error("glfwInit() failed"); }
gfx::context::~context() { glfwTerminate(); }
const std::vector<rhi::backend_info> & gfx::context::get_backends() { return rhi::global_backend_list(); }
void gfx::context::poll_events() { glfwPollEvents(); }

static gfx::window & get(GLFWwindow * window) { return *reinterpret_cast<gfx::window *>(glfwGetWindowUserPointer(window)); }

gfx::window::window(std::shared_ptr<rhi::device> dev, rhi::render_pass pass, const int2 & dimensions, const std::string & title) : dev{dev}, rhi_window{dev->create_window(pass, dimensions, title)}, glfw_window{dev->get_glfw_window(rhi_window)}
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
    dev->destroy_window(rhi_window);
}
