//
// Created by AregevDev on 23/04/2020.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <iostream>

const char *APP_NAME = "RVPT";

struct Context
{
    GLFWwindow *win_ptr{};
    vkb::Instance inst{};
    vkb::Device device{};
    VkSurfaceKHR surf{};
};

/*
 * Sets up the GLFW window and Vulkan instance and device
 */
bool init_context(Context &ctx)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    ctx.win_ptr = glfwCreateWindow(500, 500, APP_NAME, nullptr, nullptr);

    vkb::InstanceBuilder inst_builder;
    auto inst_ret = inst_builder.set_app_name(APP_NAME)
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .build();

    if (!inst_ret)
    {
        std::cout << "Failed to create an instance: "
                  << vkb::to_string(inst_ret.error().type) << '\n';
        return false;
    }

    ctx.inst = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(ctx.inst.instance, ctx.win_ptr,
                                                nullptr, &ctx.surf);
    if (surf_res != VK_SUCCESS)
    {
        std::cout << "Failed to create a surface" << '\n';
    }

    vkb::PhysicalDeviceSelector selector(ctx.inst);
    auto phys_ret =
        selector.set_surface(ctx.surf).set_minimum_version(1, 1).select();

    if (!phys_ret)
    {
        std::cout << "Failed to find a physical device: "
                  << vkb::to_string(phys_ret.error().type) << '\n';
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        std::cout << "Failed create a device: "
                  << vkb::to_string(dev_ret.error().type) << '\n';
    }

    ctx.device = dev_ret.value();

    return true;
}

void destroy_context(Context &ctx)
{
    vkb::destroy_device(ctx.device);
    vkDestroySurfaceKHR(ctx.inst.instance, ctx.surf, nullptr);
    vkb::destroy_instance(ctx.inst);

    glfwDestroyWindow(ctx.win_ptr);
    ctx.win_ptr = nullptr;

    glfwTerminate();
}

int main()
{
    Context ctx;
    if (!init_context(ctx))
    {
        std::cout << "Failed to create the context" << '\n';
    }

    while (!glfwWindowShouldClose(ctx.win_ptr))
    {
        glfwPollEvents();
    }

    destroy_context(ctx);
    return 0;
}
