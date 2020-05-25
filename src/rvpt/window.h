//
// Created by legend on 5/20/20.
//

#pragma once

#include <GLFW/glfw3.h>

class window
{
   public:
    struct settings
    {
        int width = 800;
        int height = 600;
        const char* title = "RVPT";
        bool resizeable = false;
        bool fullscreen = false;
        bool vsync = true;
    };

    explicit window(settings settings);
    ~window();

    settings get_settings();
    GLFWwindow* get_window_pointer();

    bool should_close();

   private:
    settings active_settings;
    GLFWwindow* window_ptr;
};
