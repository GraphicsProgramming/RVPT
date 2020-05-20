#include "rvpt.h"

RVPT::RVPT(window& window) : window_ref(window) {}

RVPT::~RVPT()
{
    vkb::destroy_device(context.device);
    if (context.inst.instance)
        vkDestroySurfaceKHR(context.inst.instance, context.surf, nullptr);
    vkb::destroy_instance(context.inst);

    glfwTerminate();
}

bool RVPT::initialize() { return context_init(); }

bool RVPT::draw() { return true; }

// Private functions //

bool RVPT::context_init()
{
    vkb::InstanceBuilder inst_builder;
    auto inst_ret = inst_builder.set_app_name(window_ref.get_settings().title)
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .build();

    if (!inst_ret)
    {
        std::cerr << "Failed to create an instance: "
                  << inst_ret.error().message() << '\n';
        return false;
    }

    context.inst = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(context.inst.instance,
                                                window_ref.get_window_pointer(),
                                                nullptr, &context.surf);
    if (surf_res != VK_SUCCESS)
    {
        std::cerr << "Failed to create a surface" << '\n';
        return false;
    }

    vkb::PhysicalDeviceSelector selector(context.inst);
    auto phys_ret =
        selector.set_surface(context.surf).set_minimum_version(1, 1).select();

    if (!phys_ret)
    {
        std::cerr << "Failed to find a physical device: "
                  << phys_ret.error().message() << '\n';
        return false;
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        std::cerr << "Failed create a device: " << dev_ret.error().message()
                  << '\n';
        return false;
    }

    context.device = dev_ret.value();
    vk_device = dev_ret.value().device;

    auto graphics_queue_ret =
        context.device.get_queue(vkb::QueueType::graphics);
    auto graphics_queue_index_ret =
        context.device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_ret || !graphics_queue_index_ret)
    {
        std::cerr << "Failed get the graphics queue: "
                  << dev_ret.error().message() << '\n';

        return false;
    }
    graphics_queue.emplace(graphics_queue_ret.value(),
                           graphics_queue_index_ret.value());
    auto compute_queue_ret =
        context.device.get_dedicated_queue(vkb::QueueType::compute);
    auto compute_queue_index_ret =
        context.device.get_dedicated_queue_index(vkb::QueueType::compute);
    if (compute_queue_ret && compute_queue_index_ret)
    {
        compute_queue.emplace(compute_queue_ret.value(),
                              compute_queue_index_ret.value());
    }
    return true;
}

bool RVPT::swapchain_init()
{
    vkb::SwapchainBuilder swapchain_builder(context.device);
    auto ret = swapchain_builder.build();
    if (!ret)
    {
        std::cerr << "Failed to create a swapchain:" << ret.error().message()
                  << '\n';
        return false;
    }
    vkb_swapchain = ret.value();
    return true;
}

bool RVPT::swapchain_reinit()
{
    vkb::SwapchainBuilder swapchain_builder(context.device);
    auto ret = swapchain_builder.recreate(vkb_swapchain);
    if (!ret)
    {
        std::cerr << "Failed to recreate swapchain:" << ret.error().message()
                  << '\n';
        return false;
    }
    return true;
}
