#pragma once

#include <string>
#include <vector>
#include <iostream>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>

class RVPT
{
   public:
    struct Settings
    {
        int width = 800;
        int height = 600;
        bool fullscreen = false;
        bool vsync = true;
    };

    RVPT(Settings init_settings);
    ~RVPT();

    RVPT(RVPT const& other) = delete;
    RVPT operator=(RVPT const& other) = delete;
    RVPT(RVPT&& other) = delete;
    RVPT operator=(RVPT&& other) = delete;

    bool initialize();

    bool draw();

    GLFWwindow* get_window();

   private:
    Settings settings;

    struct Context
    {
        GLFWwindow* glfw_window{};
        VkSurfaceKHR surf{};
        vkb::Instance inst{};
        vkb::Device device{};
    } context;
    VkDevice vk_device{};

    // helper functions
    bool context_init();
};