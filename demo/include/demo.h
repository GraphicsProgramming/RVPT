#pragma once

#include "vk_util.h"
#include "rvpt/rvpt.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "window.h"
#include "imgui_impl.h"

class RVPTDemo
{
public:
    explicit RVPTDemo(Window::Settings settings) noexcept;
    ~RVPTDemo() noexcept;

    RVPTDemo(RVPTDemo const& other) = delete;
    RVPTDemo operator=(RVPTDemo const& other) = delete;
    RVPTDemo(RVPTDemo&& other) = delete;
    RVPTDemo operator=(RVPTDemo&& other) = delete;

    // Create a Vulkan context and do all the one time initialization
    bool initialize();
    void initialize_rvpt();

    void update();

    void update_camera_imgui(Camera& camera);
    void update_imgui();
    void draw();

    void shutdown();

    // helper functions
    bool context_init();
    bool swapchain_init();
    bool swapchain_reinit();
    bool swapchain_get_images();
    void create_framebuffers();
    void record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index,
                               VkImage rvpt_image);
    Window window;
    Timer time;
    vkb::Instance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical_device{};
    vkb::Device vkb_device{};
    VkDevice device;

    std::optional<VK::Queue> graphics_queue;
    std::optional<VK::Queue> present_queue;
    std::optional<VK::Queue> compute_queue;

    VK::PipelineBuilder pipeline_builder;
    VK::MemoryAllocator memory_allocator;

    uint32_t current_sync_index = 0;
    std::vector<VK::SyncResources> sync_resources;
    std::vector<VkFence> frames_inflight_fences;

    VkRenderPass render_pass;

    vkb::Swapchain vkb_swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    std::vector<VK::Framebuffer> framebuffers;

    std::optional<ImguiImpl> imgui_impl;

    std::optional<RVPT> rvpt;

    uint32_t current_frame_index = 0;
    VkCommandPool command_pool;
    std::vector<VK::CommandBuffer> command_buffers;
    std::vector<VK::Fence> fences;

    bool show_imgui = true;
    // from a callback
    bool framebuffer_resized = false;
};