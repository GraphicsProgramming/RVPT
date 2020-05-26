//
// Created by legend on 5/20/20.
//

#pragma once

#include <GLFW/glfw3.h>

class Window
{
   public:
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

    float get_aspect_ratio();
    Settings get_settings();
    GLFWwindow* get_window_pointer();

    bool should_close();

   private:
    Settings active_settings;
    GLFWwindow* window_ptr;
};
