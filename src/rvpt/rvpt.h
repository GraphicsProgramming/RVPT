#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iostream>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>
#include "window.h"
#include "camera.h"
#include "timer.h"

#include "vk_util.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

class RVPT
{
public:
    explicit RVPT(Window& window);
    ~RVPT();

    RVPT(RVPT const& other) = delete;
    RVPT operator=(RVPT const& other) = delete;
    RVPT(RVPT&& other) = delete;
    RVPT operator=(RVPT&& other) = delete;

    // Create a Vulkan context and do all the one time initialization
    bool initialize();

    bool update();

    enum class draw_return
    {
        success,
        swapchain_out_of_date
    };

    draw_return draw();

    void shutdown();
    void reload_shaders();

    Camera scene_camera;
    Timer time;

    struct RenderSettings
    {
        int max_bounces = 8;
        int aa = 16;
    } render_settings;

private:
    Window& window_ref;
    std::string source_folder = "";

    // from a callback
    bool framebuffer_resized = false;
    // Random numbers (generated every frame)
    std::vector<float> random_numbers;

    struct Context
    {
        VkSurfaceKHR surf{};
        vkb::Instance inst{};
        vkb::Device device{};
    } context;
    VkDevice vk_device{};

    // safe to assume its always available
    std::optional<VK::Queue> graphics_queue;
    std::optional<VK::Queue> present_queue;

    // not safe to assume, not all hardware has a dedicated compute queue
    std::optional<VK::Queue> compute_queue;

    VK::PipelineBuilder pipeline_builder;
    VK::MemoryAllocator memory_allocator;

    vkb::Swapchain vkb_swapchain;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    uint32_t current_frame_index = 0;
    std::vector<VK::SyncResources> sync_resources;
    std::vector<VkFence> frames_inflight_fences;

    VkRenderPass fullscreen_tri_render_pass;

    std::vector<VK::Framebuffer> framebuffers;

    struct RenderingResources
    {
        VK::DescriptorPool image_pool;

        VK::DescriptorPool raytrace_descriptor_pool;

        VK::PipelineHandle fullscreen_triangle_pipeline;
        VK::PipelineHandle raytrace_pipeline;
    };

    struct PerFrameDescriptorSets
    {
        VK::DescriptorSet image_descriptor_set;
        VK::DescriptorSet raytracing_descriptor_sets;
    };

    std::optional<RenderingResources> rendering_resources;
    std::vector<VK::Image> per_frame_output_image;
    std::vector<VK::Buffer> per_frame_camera_uniform;
    std::vector<VK::Buffer> per_frame_random_uniform;
    std::vector<VK::Buffer> per_frame_settings_uniform;
    std::vector<VK::CommandBuffer> per_frame_raytrace_command_buffer;
    std::vector<VK::Fence> per_frame_raytrace_work_fence;
    std::vector<PerFrameDescriptorSets> per_frame_descriptor_sets;

    // helper functions
    bool context_init();
    bool swapchain_init();
    bool swapchain_reinit();
    bool swapchain_get_images();
    void create_framebuffers();

    RenderingResources create_rendering_resources();

    void record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index);
    void record_compute_command_buffer();
};
