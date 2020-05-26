//
// Created by legend on 5/20/20.
//

#include "window.h"

#include <iostream>

Window::Window(Window::Settings settings) : active_settings(settings)
{
    // Initializing glfw and a window
    auto glfw_ret = glfwInit();
    if (!glfw_ret) std::cerr << "Failed to initialize glfw" << '\n';

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ptr = glfwCreateWindow(settings.width, settings.height, settings.title, nullptr, nullptr);
    if (window_ptr == nullptr) std::cerr << "Failed to create a glfw window" << '\n';

    glfwSetWindowUserPointer(window_ptr, this);
    glfwSetKeyCallback(window_ptr, key_callback);
    glfwSetMouseButtonCallback(window_ptr, mouse_click_callback);
    glfwSetCursorPosCallback(window_ptr, mouse_move_callback);
    glfwSetScrollCallback(window_ptr, scroll_callback);
}

Window::~Window()
{
    glfwDestroyWindow(window_ptr);
    glfwTerminate();
}

void Window::poll_events() { glfwPollEvents(); }

void Window::add_key_callback(std::function<void(int, Action)> callback)
{
    key_callbacks.push_back(callback);
}

void Window::add_mouse_click_callback(std::function<void(Mouse, Action)> callback)
{
    mouse_click_callbacks.push_back(callback);
}

void Window::add_mouse_move_callback(std::function<void(float, float)> callback)
{
    mouse_move_callbacks.push_back(callback);
}

void Window::add_scroll_callback(std::function<void(float x, float y)> callback)
{
    scroll_callbacks.push_back(callback);
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Action callback_action;
    switch (action)
    {
        case GLFW_RELEASE:
            callback_action = Action::RELEASE;
            break;
        case GLFW_PRESS:
            callback_action = Action::PRESS;
            break;
        case GLFW_REPEAT:
            callback_action = Action::REPEAT;
            break;
        default:
            callback_action = Action::UNKNOWN;
    }

    auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    for (auto& callback : window_ptr->key_callbacks) callback(key, callback_action);
}

void Window::mouse_click_callback(GLFWwindow *window, int button, int action, int mods)
{
    Action callback_action;
    switch (action)
    {
        case GLFW_RELEASE:
            callback_action = Action::RELEASE;
            break;
        case GLFW_PRESS:
            callback_action = Action::PRESS;
            break;
        case GLFW_REPEAT:
            callback_action = Action::REPEAT;
            break;
        default:
            callback_action = Action::UNKNOWN;
    }

    Mouse mouse_button;
    switch (button)
    {
        case GLFW_MOUSE_BUTTON_1:
            mouse_button = Mouse::LEFT;
            break;
        case GLFW_MOUSE_BUTTON_2:
            mouse_button = Mouse::RIGHT;
            break;
        case GLFW_MOUSE_BUTTON_3:
            mouse_button = Mouse::MIDDLE;
            break;
        default:
            mouse_button = Mouse::OTHER;
    }

    auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    for(auto& callback : window_ptr->mouse_click_callbacks)
        callback(mouse_button, callback_action);
}

void Window::mouse_move_callback(GLFWwindow *window, double x, double y)
{
    auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    for(auto& callback : window_ptr->mouse_move_callbacks)
        callback(x, y);
}

void Window::scroll_callback(GLFWwindow *window, double x, double y)
{
    auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    for(auto& callback : window_ptr->scroll_callbacks)
        callback(x, y);
}

Window::Settings Window::get_settings() { return active_settings; }

GLFWwindow* Window::get_window_pointer() { return window_ptr; }

bool Window::should_close() { return glfwWindowShouldClose(window_ptr); }
