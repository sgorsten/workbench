#include "io.h"

glfw::context::context()
{
    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, 0);
    hidden_window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    glfwDefaultWindowHints();        
    glfwMakeContextCurrent(hidden_window);
    glewInit();
}

glfw::context::~context()
{
    glfwDestroyWindow(hidden_window);
    glfwTerminate();
}

static glfw::window & get(GLFWwindow * window) { return *reinterpret_cast<glfw::window *>(glfwGetWindowUserPointer(window)); }

glfw::window::window(context & context, int2 dimensions, std::string_view title)
{
    const std::string buffer {begin(title), end(title)};
    w = glfwCreateWindow(dimensions.x, dimensions.y, buffer.c_str(), nullptr, context.hidden_window);
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
    glfwSetWindowUserPointer(w, this);
}

glfw::window::~window() 
{ 
    glfwDestroyWindow(w); 
}

