#include "rvpt.h"

RVPT::RVPT(window& window) : window_ref(window)
{

}

RVPT::~RVPT()
{
    vkb::destroy_device(context.device);
    if (context.inst.instance)
        vkDestroySurfaceKHR(context.inst.instance, context.surf, nullptr);
    vkb::destroy_instance(context.inst);


    glfwTerminate();
}

bool RVPT::initialize()
{ return context_init(); }

bool RVPT::draw() { return true; }

// Private functions //

bool RVPT::context_init()
{
    const char * APP_NAME = "RVPT";
    vkb::InstanceBuilder inst_builder;
    auto inst_ret = inst_builder.set_app_name(APP_NAME)
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .build();

    if (!inst_ret)
    {
        std::cout << "Failed to create an instance: "
                  << inst_ret.error().message() << '\n';
        return false;
    }

    context.inst = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(
        context.inst.instance, window_ref.get_window_pointer(), nullptr, &context.surf);
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
                  << phys_ret.error().message() << '\n';
        return false;
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        std::cout << "Failed create a device: " << dev_ret.error().message()
                  << '\n';
        return false;
    }

    context.device = dev_ret.value();
    vk_device = dev_ret.value().device;
    return true;
}