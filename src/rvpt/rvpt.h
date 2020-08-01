#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <random>

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <VkBootstrap.h>

#include "window.h"
#include "imgui_impl.h"
#include "vk_util.h"
#include "camera.h"
#include "timer.h"
#include "geometry.h"
#include "material.h"

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

static const char* RenderModes[] = {"binary", "depth", "normals", "ambient occlusion", "Kajiya"};

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

    void update_imgui();
    draw_return draw();

    void shutdown();

    void reload_shaders();
    void toggle_debug();
    void toggle_wireframe_debug();
    void set_raytrace_mode(int mode);

    Camera scene_camera;
    Timer time;

    struct RenderSettings
    {
        int max_bounces = 8;
        int aa = 1;
        uint32_t current_frame = 1;
        int render_mode = 4;
    } render_settings;

private:
    bool show_imgui = true;
    
    // from a callback
    bool framebuffer_resized = false;

    // enable debug overlay
    bool debug_overlay_enabled = false;
    bool debug_wireframe_mode = false;

    Window& window_ref;
    std::string source_folder = "";

    // Random number generators
    std::mt19937 random_generator;
    std::uniform_real_distribution<float> distribution;

    // Random numbers (generated every frame)
    std::vector<float> random_numbers;

    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Material> materials;

    glm::mat4 last_camera_mat;

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

    uint32_t current_sync_index = 0;
    std::vector<VK::SyncResources> sync_resources;
    std::vector<VkFence> frames_inflight_fences;

    VkRenderPass fullscreen_tri_render_pass;

    std::optional<ImguiImpl> imgui_impl;

    std::vector<VK::Framebuffer> framebuffers;

    struct RenderingResources
    {
        VK::DescriptorPool image_pool;
        VK::DescriptorPool raytrace_descriptor_pool;
        VK::DescriptorPool debug_descriptor_pool;

        VkPipelineLayout fullscreen_triangle_pipeline_layout;
        VK::GraphicsPipelineHandle fullscreen_triangle_pipeline;
        VkPipelineLayout raytrace_pipeline_layout;
        VK::ComputePipelineHandle raytrace_pipeline;

        VkPipelineLayout debug_pipeline_layout;
        VK::GraphicsPipelineHandle debug_opaque_pipeline;
        VK::GraphicsPipelineHandle debug_wireframe_pipeline;

        VK::Image temporal_storage_image;
    };

    std::optional<RenderingResources> rendering_resources;

    uint32_t current_frame_index = 0;
    struct PerFrameData
    {
        VK::Buffer settings_uniform;
        VK::Image output_image;
        VK::Buffer random_buffer;
        VK::Buffer camera_uniform;
        VK::Buffer sphere_buffer;
        VK::Buffer triangle_buffer;
        VK::Buffer material_buffer;
        VK::CommandBuffer raytrace_command_buffer;
        VK::Fence raytrace_work_fence;
        VK::DescriptorSet image_descriptor_set;
        VK::DescriptorSet raytracing_descriptor_sets;

        VK::Buffer debug_camera_uniform;
        VK::Buffer debug_vertex_buffer;
        VK::DescriptorSet debug_descriptor_sets;
    };
    std::vector<PerFrameData> per_frame_data;

    // helper functions
    bool context_init();
    bool swapchain_init();
    bool swapchain_reinit();
    bool swapchain_get_images();
    void create_framebuffers();

    RenderingResources create_rendering_resources();
    void add_per_frame_data(int index);

    // Todo remove this(PLEASE REMEMBER)
    void addRectangle(glm::vec3, glm::vec3, glm::vec3, glm::vec3, int mat);

    void record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index);
    void record_compute_command_buffer();
};
