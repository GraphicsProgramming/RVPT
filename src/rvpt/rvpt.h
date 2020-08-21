#pragma once

#include <string>
#include <vector>
#include <optional>
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
#include "bvh_node.h"
#include "bvh_builder.h"

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

static const char* RenderModes[] = {"binary",       "color",          "depth",
                                    "normals",      "Utah model",     "ambient occlusion",
                                    "Arthur Appel", "Turner Whitted", "Robert Cook",
                                    "James Kajiya", "John Hart"};

const std::vector<glm::vec3> colors = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0},   {1, .5, 0},
                      {1, 0, 1}, {1, 1, 0}, {1, 1, 1}, {.5, .25, 0}};

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
    void toggle_bvh_debug();
    void toggle_view_last_bvh_depths();
    void set_raytrace_mode(int mode);

    void add_material(Material material);
    void add_sphere(Sphere sphere);
    void add_triangle(Triangle triangle);

    void get_asset_path(std::string& asset_path);

    Camera scene_camera;
    Timer time;

    struct RenderSettings
    {
        int max_bounces = 8;
        int aa = 1;
        uint32_t current_frame = 1;
        int camera_mode = 0;
        int top_left_render_mode = 9;
        int top_right_render_mode = 9;
        int bottom_left_render_mode = 9;
        int bottom_right_render_mode = 9;
        glm::vec2 split_ratio = glm::vec2(0.5, 0.5);

    } render_settings;

private:
    bool show_imgui = true;

    // from a callback
    bool framebuffer_resized = false;

    // enable debug overlay
    bool debug_overlay_enabled = false;
    bool debug_wireframe_mode = false;

    bool debug_bvh_enabled = false;

    Window& window_ref;
    std::string source_folder = "";

    // Random number generators
    std::mt19937 random_generator;
    std::uniform_real_distribution<float> distribution;

    // Random numbers (generated every frame)
    std::vector<float> random_numbers;

    // BVH AABB's
    BvhBuilder bvh_builder;
    BvhNode* tl_bvh; // Highest level BVH
    std::vector<std::vector<AABB>> depth_bvh_bounds;
    size_t bvh_vertex_count = 0;
    int max_bvh_build_depth = 5;
    int max_bvh_view_depth = 2;
    bool view_previous_depths = true;
    bool does_bvh_support_depth_viewing = false;
    // Yes I know, I have an extra set of the AABB's, I did this so instead of recursively going
    // through every. single. node. To setup the vertices for the debug view, we only have to do go
    // through them once which means the split function actually takes a pointer to this, and
    // uploads it's own AABB to the vector every time it gets "split" If you have a better solution
    // for this be my guest. Make a pull request and tell me how, because I can't think of a better
    // solution At least not a better one that I would be able to write in an okay amount of time.

    std::vector<GpuBvhNode> gpu_bvh_nodes;
    std::vector<Sphere> spheres;
    std::vector<Triangle> triangles;
    std::vector<Triangle> sorted_triangles;
    std::vector<Material> materials;

    struct PreviousFrameState
    {
        RenderSettings settings;
        std::vector<glm::vec4> camera_data;

        bool operator==(RVPT::PreviousFrameState const& right);
    } previous_frame_state;

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
        VK::DescriptorPool debug_bvh_descriptor_pool;

        VkPipelineLayout fullscreen_triangle_pipeline_layout;
        VK::GraphicsPipelineHandle fullscreen_triangle_pipeline;
        VkPipelineLayout raytrace_pipeline_layout;
        VK::ComputePipelineHandle raytrace_pipeline;

        VkPipelineLayout debug_pipeline_layout;
        VK::GraphicsPipelineHandle debug_opaque_pipeline;
        VK::GraphicsPipelineHandle debug_wireframe_pipeline;

        VkPipelineLayout debug_bvh_layout;
        VK::GraphicsPipelineHandle debug_bvh_pipeline;

        VK::Image temporal_storage_image;
        VK::Image depth_buffer;
    };

    std::optional<RenderingResources> rendering_resources;

    uint32_t current_frame_index = 0;
    struct PerFrameData
    {
        VK::Buffer settings_uniform;
        VK::Image output_image;
        VK::Buffer random_buffer;
        VK::Buffer camera_uniform;
        VK::Buffer bvh_buffer;
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

        VK::Buffer debug_bvh_camera_uniform;
        VK::Buffer debug_bvh_vertex_buffer;
        VK::DescriptorSet debug_bvh_descriptor_set;
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

    void record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index);
    void record_compute_command_buffer();
};
