#pragma once

#include <string>
#include <vector>
#include <optional>
#include <random>

#include <vulkan/vulkan.h>

#include <VkBootstrap.h>

#include "vk_util.h"
#include "timer.h"

#include "camera.h"
#include "geometry.h"
#include "material.h"
#include "bvh.h"
#include "bvh_builder.h"

static const char* RenderModes[] = {"binary",       "color",          "depth",
                                    "normals",      "Utah model",     "ambient occlusion",
                                    "Arthur Appel", "Turner Whitted", "Robert Cook",
                                    "James Kajiya", "John Hart"};

const std::vector<glm::vec3> colors = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0},   {1, .5, 0},
                                       {1, 0, 1}, {1, 1, 0}, {1, 1, 1}, {.5, .25, 0}};

class RVPT
{
public:
    explicit RVPT(float aspect_ratio) noexcept;
    ~RVPT() noexcept;

    RVPT(RVPT const& other) = delete;
    RVPT operator=(RVPT const& other) = delete;
    RVPT(RVPT&& other) noexcept;
    RVPT operator=(RVPT&& other) noexcept;

    // Create a Vulkan context and do all the one time initialization
    bool initialize(VkPhysicalDevice physical_device, VkDevice device,
                    uint32_t graphics_queue_index, uint32_t compute_queue_index, VkExtent2D extent);

    void update();

    void draw();
    VkImage get_image();

    void shutdown();

    void reload_shaders();
    void toggle_debug();
    void toggle_wireframe_debug();
    void toggle_bvh_debug();
    void toggle_view_last_bvh_depths();
    void set_raytrace_mode(int mode);

    void add_material(Material material);
    void add_triangle(Triangle triangle);

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

    // private:
    VkExtent2D extent;
    uint32_t width = 0;
    uint32_t height = 0;

    // enable debug overlay
    bool debug_overlay_enabled = false;
    bool debug_wireframe_mode = false;

    bool debug_bvh_enabled = false;

    std::string source_folder = "";

    // Random number generators
    std::mt19937 random_generator;
    std::uniform_real_distribution<float> distribution;

    // Random numbers (generated every frame)
    std::vector<float> random_numbers;

    // BVH AABB's
    BinnedBvhBuilder bvh_builder;
    Bvh top_level_bvh;

    // Debug BVH view
    std::vector<std::vector<AABB>> depth_bvh_bounds;
    size_t bvh_vertex_count = 0;
    int max_bvh_view_depth = 1;
    bool view_previous_depths = true;

    std::vector<Triangle> triangles;
    std::vector<Triangle> sorted_triangles;
    std::vector<Material> materials;

    struct PreviousFrameState
    {
        RenderSettings settings;
        std::vector<glm::vec4> camera_data;

        bool operator==(RVPT::PreviousFrameState const& right);
    } previous_frame_state;

    VkPhysicalDevice physical_device;
    VkDevice device{};

    // safe to assume its always available
    std::optional<VK::Queue> graphics_queue;
    // not safe to assume, not all hardware has a dedicated compute queue
    std::optional<VK::Queue> compute_queue;

    VK::PipelineBuilder pipeline_builder;
    VK::MemoryAllocator memory_allocator;

    VkRenderPass fullscreen_tri_render_pass;

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

        VK::Framebuffer framebuffer;
    };
    std::vector<PerFrameData> per_frame_data;

    RVPT::RenderingResources create_rendering_resources();
    void add_per_frame_data(int index);

    void record_graphics_command_buffer(VK::SyncResources& current_frame);
    void record_compute_command_buffer();
};
