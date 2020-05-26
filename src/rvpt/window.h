//
// Created by legend on 5/20/20.
//

#pragma once

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
    void add_mouse_callback(
        std::function<void(float x, float y, Mouse button, Action action)> callback);
    void poll_events();

    Settings get_settings();
    GLFWwindow* get_window_pointer();

    bool should_close();

private:
    Settings active_settings;
    GLFWwindow* window_ptr;

    std::vector<std::function<void(int keycode, Action action)>> key_callbacks;
    std::vector<std::function<void(float x, float y, Mouse button, Action action)>> mouse_callbacks;

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_callback(GLFWwindow* window, int button, int action, int mods);
};
