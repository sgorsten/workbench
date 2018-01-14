#include "io.h"

glfw::context::context()
{
    if(glfwInit() == GLFW_FALSE) throw std::runtime_error("glfwInit() failed");
}

glfw::context::~context()
{
    glfwTerminate();
}

static glfw::window & get(GLFWwindow * window) { return *reinterpret_cast<glfw::window *>(glfwGetWindowUserPointer(window)); }

glfw::window::window(GLFWwindow * window) : w{window}
{
    glfwSetWindowUserPointer(w, this);
    glfwSetWindowPosCallback(w, [](GLFWwindow * window, int xpos, int ypos) { get(window).on_window_pos({xpos, ypos}); });
    glfwSetWindowSizeCallback(w, [](GLFWwindow * window, int width, int height) { get(window).on_window_size({width, height}); });
    glfwSetWindowCloseCallback(w, [](GLFWwindow * window) { get(window).on_window_close(); });
    glfwSetWindowRefreshCallback(w, [](GLFWwindow * window) { get(window).on_window_refresh(); });
    glfwSetWindowFocusCallback(w, [](GLFWwindow * window, int focused) { get(window).on_window_focus(!!focused); });
    glfwSetWindowIconifyCallback(w, [](GLFWwindow * window, int iconified) { get(window).on_window_iconify(!!iconified); });
    glfwSetFramebufferSizeCallback(w, [](GLFWwindow * window, int width, int height) { get(window).on_framebuffer_size({width, height}); });
    glfwSetMouseButtonCallback(w, [](GLFWwindow * window, int button, int action, int mods) { get(window).on_mouse_button(button, action, mods); });
    glfwSetCursorPosCallback(w, [](GLFWwindow * window, double xpos, double ypos) { get(window).on_cursor_pos({xpos, ypos}); });
    glfwSetCursorEnterCallback(w, [](GLFWwindow * window, int entered) { get(window).on_cursor_enter(!!entered); });
    glfwSetScrollCallback(w, [](GLFWwindow * window, double xoffset, double yoffset) { get(window).on_scroll({xoffset, yoffset}); });
    glfwSetKeyCallback(w, [](GLFWwindow * window, int key, int scancode, int action, int mods) { get(window).on_key(key, scancode, action, mods); });
    glfwSetCharModsCallback(w, [](GLFWwindow * window, unsigned int codepoint, int mods) { get(window).on_char(codepoint, mods); });
}

glfw::window::~window() 
{ 
    glfwSetWindowPosCallback(w, nullptr);
    glfwSetWindowSizeCallback(w, nullptr);
    glfwSetWindowCloseCallback(w, nullptr);
    glfwSetWindowRefreshCallback(w, nullptr);
    glfwSetWindowFocusCallback(w, nullptr);
    glfwSetWindowIconifyCallback(w, nullptr);
    glfwSetFramebufferSizeCallback(w, nullptr);
    glfwSetMouseButtonCallback(w, nullptr);
    glfwSetCursorPosCallback(w, nullptr);
    glfwSetCursorEnterCallback(w, nullptr);
    glfwSetScrollCallback(w, nullptr);
    glfwSetKeyCallback(w, nullptr);
    glfwSetCharModsCallback(w, nullptr);
    glfwSetWindowUserPointer(w, nullptr);
}

