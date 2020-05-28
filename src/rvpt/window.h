//
// Created by legend on 5/20/20.
//

#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <functional>

class Window
{
public:
    enum class Action
    {
        RELEASE,
        PRESS,
        REPEAT,
        UNKNOWN
    };

    enum class Mouse
    {
        LEFT,
        RIGHT,
        MIDDLE,
        OTHER
    };

    struct Settings
    {
        int width = 800;
        int height = 600;
        const char* title = "RVPT";
        bool resizeable = false;
        bool fullscreen = false;
        bool vsync = true;
    };

    explicit Window(Settings settings);
    ~Window();

    void add_key_callback(std::function<void(int keycode, Action action)> callback);
    void add_mouse_click_callback(std::function<void(Mouse button, Action action)> callback);
    void add_mouse_move_callback(std::function<void(float x, float y)> callback);
    void add_scroll_callback(std::function<void(float x, float y)> callback);
    void poll_events();

    Settings get_settings();
    GLFWwindow* get_window_pointer();

    bool should_close();
    void set_close();

private:
    std::vector<std::function<void(int keycode, Action action)>> key_callbacks;
    std::vector<std::function<void(Mouse button, Action action)>> mouse_click_callbacks;
    std::vector<std::function<void(float x, float y)>> mouse_move_callbacks;
    std::vector<std::function<void(float x, float y)>> scroll_callbacks;
    Settings active_settings;
    GLFWwindow* window_ptr;

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
    static void mouse_move_callback(GLFWwindow* window, double x, double y);
    static void scroll_callback(GLFWwindow* window, double x, double y);
};
