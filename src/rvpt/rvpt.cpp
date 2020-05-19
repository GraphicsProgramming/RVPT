#include "rvpt.h"

RVPT::RVPT(Settings init_settings) : settings(init_settings) {}

RVPT::~RVPT()
{
    vkb::destroy_device(context.device);
    if (context.inst.instance)
        vkDestroySurfaceKHR(context.inst.instance, context.surf, nullptr);
    vkb::destroy_instance(context.inst);

    glfwDestroyWindow(context.glfw_window);
    context.glfw_window = nullptr;

    glfwTerminate();
}

bool RVPT::initialize()
{
    if (!context_init())
    {
        return false;
    }
}

bool RVPT::draw() { return true; }

GLFWwindow* RVPT::get_window() { return context.glfw_window; }

// Private functions //

bool RVPT::context_init()
{
    const char* APP_NAME = "RVPT";

    auto glfw_ret = glfwInit();
    if (!glfw_ret)
    {
        std::cout << "Failed to initialize glfw" << '\n';
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    context.glfw_window = glfwCreateWindow(settings.width, settings.height,
                                           APP_NAME, nullptr, nullptr);
    if (context.glfw_window == nullptr)
    {
        std::cout << "Failed to create a glfw window" << '\n';
        return false;
    }

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

    context.inst = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(
        context.inst.instance, context.glfw_window, nullptr, &context.surf);
    if (surf_res != VK_SUCCESS)
    {
        std::cout << "Failed to create a surface" << '\n';
        return false;
    }

    vkb::PhysicalDeviceSelector selector(context.inst);
    auto phys_ret =
        selector.set_surface(context.surf).set_minimum_version(1, 1).select();

    if (!phys_ret)
    {
        std::cout << "Failed to find a physical device: "
                  << vkb::to_string(phys_ret.error().type) << '\n';
        return false;
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        std::cout << "Failed create a device: "
                  << vkb::to_string(dev_ret.error().type) << '\n';
        return false;
    }

    context.device = dev_ret.value();
    vk_device = dev_ret.value().device;
    return true;
}