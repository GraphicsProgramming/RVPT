//
// Created by legend on 5/20/20.
//

#include "window.h"

#include <iostream>

window::window(window::settings settings) : active_settings(settings)
{
    // Initializing glfw and a window
    auto glfw_ret = glfwInit();
    if (!glfw_ret) std::cerr << "Failed to initialize glfw" << '\n';

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ptr = glfwCreateWindow(settings.width, settings.height,
                                  settings.title, NULL, NULL);
    if (window_ptr == nullptr)
        std::cerr << "Failed to create a glfw window" << '\n';
}

window::~window()
{
    glfwDestroyWindow(window_ptr);
    glfwTerminate();
}

window::settings window::get_settings() { return active_settings; }

GLFWwindow* window::get_window_pointer() { return window_ptr; }

bool window::should_close() { return glfwWindowShouldClose(window_ptr); }
